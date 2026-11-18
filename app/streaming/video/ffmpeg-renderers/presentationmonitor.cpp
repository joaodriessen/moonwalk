#include "presentationmonitor.h"

#include <QtGlobal>
#include <mutex>

static std::mutex s_TotalsLock;
static PresentationMonitor::Totals s_Totals;
static std::atomic<bool> s_Visible {true};

bool PresentationMonitor::windowVisible()
{
    return s_Visible.load(std::memory_order_relaxed);
}

PresentationMonitor::Totals PresentationMonitor::totals()
{
    std::lock_guard<std::mutex> lock(s_TotalsLock);
    return s_Totals;
}

void PresentationMonitor::resetTotals()
{
    std::lock_guard<std::mutex> lock(s_TotalsLock);
    s_Totals = {};
}

#ifndef HAS_WAYLAND

PresentationMonitor::PresentationMonitor() {}
PresentationMonitor::~PresentationMonitor() {}
bool PresentationMonitor::initialize(SDL_Window*) { return false; }
void PresentationMonitor::beforeSwap(uint64_t) {}
void PresentationMonitor::waitForDesktop(int) {}

#else

#include <Limelight.h>
#include <SDL_syswm.h>
#include <cstring>
#include <poll.h>
#include <time.h>
#include <wayland-client.h>

// The stable wp_presentation protocol. Written out here rather than generated, as
// these few messages are all that's used of it.
extern const wl_interface s_FeedbackInterface;
static const wl_interface* s_FeedbackTypes[] = {&wl_surface_interface, &s_FeedbackInterface};
static const wl_message s_PresentationRequests[] = {
    {"destroy", "", nullptr},
    {"feedback", "on", s_FeedbackTypes},
};
static const wl_message s_PresentationEvents[] = {
    {"clock_id", "u", nullptr},
};
static const wl_interface s_PresentationInterface = {"wp_presentation", 1, 2, s_PresentationRequests, 1, s_PresentationEvents};
static const wl_interface* s_OutputTypes[] = {&wl_output_interface};
static const wl_interface* s_NoTypes[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static const wl_message s_FeedbackEvents[] = {
    {"sync_output", "o", s_OutputTypes},
    {"presented", "uuuuuuu", s_NoTypes},
    {"discarded", "", nullptr},
};
const wl_interface s_FeedbackInterface = {"wp_presentation_feedback", 1, 0, nullptr, 3, s_FeedbackEvents};

struct PresentationListener { void (*clockId)(void*, wl_proxy*, uint32_t); };
struct FeedbackListener {
    void (*syncOutput)(void*, wl_proxy*, wl_proxy*);
    void (*presented)(void*, wl_proxy*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    void (*discarded)(void*, wl_proxy*);
};

struct PresentationCallbacks {
    static void clockId(void* data, wl_proxy*, uint32_t id) { static_cast<PresentationMonitor*>(data)->m_ClockId = id; }
    static void syncOutput(void*, wl_proxy*, wl_proxy*) {}
    static void presented(void* data, wl_proxy*, uint32_t secHi, uint32_t secLo, uint32_t nsec, uint32_t refresh, uint32_t, uint32_t, uint32_t)
    {
        auto* pending = static_cast<PresentationMonitor::Pending*>(data);
        pending->owner->finished(pending, true, ((uint64_t(secHi) << 32 | secLo) * 1000000000ull) + nsec, refresh);
    }
    static void discarded(void* data, wl_proxy*)
    {
        auto* pending = static_cast<PresentationMonitor::Pending*>(data);
        pending->owner->finished(pending, false, 0, 0);
    }
    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        if (!strcmp(interface, "wp_presentation"))
            static_cast<PresentationMonitor*>(data)->m_Presentation =
                static_cast<wl_proxy*>(wl_registry_bind(registry, name, &s_PresentationInterface, 1));
    }
    static void globalRemove(void*, wl_registry*, uint32_t) {}
    static void frameDone(void* data, wl_callback* callback, uint32_t)
    {
        auto* me = static_cast<PresentationMonitor*>(data);
        wl_callback_destroy(callback);
        if (me->m_FrameCallback == callback) {
            me->m_FrameCallback = nullptr;
            me->m_Waiting = false;
            s_Visible.store(true, std::memory_order_relaxed);
        }
    }
};

static const PresentationListener s_PresentationListener = {PresentationCallbacks::clockId};
static const FeedbackListener s_FeedbackListener = {PresentationCallbacks::syncOutput, PresentationCallbacks::presented, PresentationCallbacks::discarded};
static const wl_callback_listener s_FrameListener = {PresentationCallbacks::frameDone};
static const wl_registry_listener s_RegistryListener = {PresentationCallbacks::global, PresentationCallbacks::globalRemove};

PresentationMonitor::PresentationMonitor() {}

PresentationMonitor::~PresentationMonitor()
{
    s_Visible.store(true, std::memory_order_relaxed);
    for (auto& pending : m_Pending)
        if (pending.feedback) wl_proxy_destroy(pending.feedback);
    if (m_FrameCallback) wl_callback_destroy(m_FrameCallback);
    if (m_SurfaceWrapper) wl_proxy_wrapper_destroy(m_SurfaceWrapper);
    if (m_Presentation) {
        wl_proxy_marshal_flags(m_Presentation, 0, nullptr, wl_proxy_get_version(m_Presentation), WL_MARSHAL_FLAG_DESTROY);
    }
    if (m_Queue) wl_event_queue_destroy(m_Queue);
}

bool PresentationMonitor::initialize(SDL_Window* window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WAYLAND) return false;
    m_Display = info.info.wl.display;
    m_Surface = info.info.wl.surface;
    if (!m_Display || !m_Surface) return false;

    // Everything of ours stays on its own queue, so it can't disturb SDL's or Mesa's.
    m_Queue = wl_display_create_queue(m_Display);
    auto* wrapped = static_cast<wl_display*>(wl_proxy_create_wrapper(m_Display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), m_Queue);
    wl_registry* registry = wl_display_get_registry(wrapped);
    wl_proxy_wrapper_destroy(wrapped);
    wl_registry_add_listener(registry, &s_RegistryListener, this);
    wl_display_roundtrip_queue(m_Display, m_Queue);
    wl_registry_destroy(registry);
    m_SurfaceWrapper = static_cast<wl_proxy*>(wl_proxy_create_wrapper(m_Surface));
    wl_proxy_set_queue(m_SurfaceWrapper, m_Queue);
    s_Visible.store(true, std::memory_order_relaxed);
    if (!m_Presentation) return false;
    wl_proxy_add_listener(m_Presentation, reinterpret_cast<void (**)(void)>(const_cast<PresentationListener*>(&s_PresentationListener)), this);
    wl_display_roundtrip_queue(m_Display, m_Queue);
    return true;
}

void PresentationMonitor::waitForDesktop(int timeoutMs)
{
    if (!m_Queue) return;
    wl_display_dispatch_queue_pending(m_Display, m_Queue);
    const uint64_t deadline = LiGetMicroseconds() + uint64_t(timeoutMs) * 1000;
    if (m_Holding && LiGetMicroseconds() >= m_HoldUntilUs) {
        m_Holding = false;
        m_CleanSinceUs = LiGetMicroseconds();
    }
    if (!m_Holding) {
        if (m_CleanSinceUs && LiGetMicroseconds() - m_CleanSinceUs >= 30000000) {
            m_HoldSeconds = 2;
            m_CleanSinceUs = 0;
        }
        return;
    }
    // Nothing to wait for while the desktop isn't drawing us.
    if (!visibleNow(LiGetMicroseconds())) return;
    while (m_Waiting) {
        const uint64_t now = LiGetMicroseconds();
        if (now >= deadline) {
            m_Waiting = false;
            return;
        }
        if (wl_display_prepare_read_queue(m_Display, m_Queue) != 0) {
            wl_display_dispatch_queue_pending(m_Display, m_Queue);
            continue;
        }
        wl_display_flush(m_Display);
        pollfd fd {wl_display_get_fd(m_Display), POLLIN, 0};
        // Every thread reading the display wakes on this same descriptor, so none waits on us.
        if (poll(&fd, 1, int((deadline - now + 999) / 1000)) > 0) wl_display_read_events(m_Display);
        else wl_display_cancel_read(m_Display);
        wl_display_dispatch_queue_pending(m_Display, m_Queue);
    }
}

bool PresentationMonitor::visibleNow(uint64_t nowUs) const
{
    // A visible window hears back within a refresh or two.
    return !m_FrameCallback || nowUs - m_FrameRequestedUs < 250000;
}

void PresentationMonitor::beforeSwap(uint64_t decodedUs)
{
    if (m_Queue) {
        wl_display_dispatch_queue_pending(m_Display, m_Queue);
        s_Visible.store(visibleNow(LiGetMicroseconds()), std::memory_order_relaxed);
    }
    if (m_SurfaceWrapper && !m_FrameCallback) {
        m_FrameCallback = wl_surface_frame(reinterpret_cast<wl_surface*>(m_SurfaceWrapper));
        wl_callback_add_listener(m_FrameCallback, &s_FrameListener, this);
        m_FrameRequestedUs = LiGetMicroseconds();
    }
    m_Waiting = m_FrameCallback != nullptr;
    if (!m_Presentation) return;
    wl_display_dispatch_queue_pending(m_Display, m_Queue);
    for (auto& pending : m_Pending) {
        if (pending.feedback) continue;
        pending.decodedUs = decodedUs;
        pending.owner = this;
        pending.feedback = wl_proxy_marshal_flags(m_Presentation, 1, &s_FeedbackInterface, wl_proxy_get_version(m_Presentation), 0,
                                                  m_Surface, nullptr);
        if (pending.feedback)
            wl_proxy_add_listener(pending.feedback, reinterpret_cast<void (**)(void)>(const_cast<FeedbackListener*>(&s_FeedbackListener)), &pending);
        return;
    }
}

void PresentationMonitor::finished(void* raw, bool presented, uint64_t presentedNs, uint32_t refreshNs)
{
    auto* pending = static_cast<Pending*>(raw);
    int64_t latencyUs = -1;
    if (presented) {
        timespec now {};
        clock_gettime(clockid_t(m_ClockId), &now);
        latencyUs = displayLatencyUs(uint64_t(now.tv_sec) * 1000000000ull + uint64_t(now.tv_nsec), presentedNs,
                                     LiGetMicroseconds(), pending->decodedUs);
    }
    wl_proxy_destroy(pending->feedback);
    pending->feedback = nullptr;
    // A frame the desktop drops while it isn't drawing the window was never going to be
    // seen: it is neither judder to count nor a reason to wait.
    if (!presented && !visibleNow(LiGetMicroseconds())) return;
    if (!m_Holding) {
        (presented ? m_WindowPresented : m_WindowDiscarded)++;
        // Two skips in a second is visible, and more than timing noise when frames are steady.
        if (m_WindowDiscarded >= 2) {
            m_Holding = true;
            m_HoldUntilUs = LiGetMicroseconds() + uint64_t(m_HoldSeconds) * 1000000;
            m_HoldSeconds = qMin(m_HoldSeconds * 2, 60);
            m_CleanSinceUs = 0;
            m_WindowPresented = m_WindowDiscarded = 0;
        }
        else if (m_WindowPresented + m_WindowDiscarded >= 60) {
            m_WindowPresented = m_WindowDiscarded = 0;
        }
    }
    std::lock_guard<std::mutex> lock(s_TotalsLock);
    if (presented) {
        ++s_Totals.presented;
        if (latencyUs >= 0) {
            ++s_Totals.latencySamples;
            s_Totals.totalLatencyUs += uint64_t(latencyUs);
        }
        if (refreshNs) s_Totals.refreshNs = refreshNs;
    } else {
        ++s_Totals.discarded;
    }
}

#endif
