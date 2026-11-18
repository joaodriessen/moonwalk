#pragma once

#include <SDL.h>
#include <atomic>
#include <cstdint>

// Asks the desktop what became of each frame we hand it. Without this, a frame the
// compositor replaced before it reached the screen looks exactly like one that was
// shown, and judder from that shows up in no statistic.
class PresentationMonitor {
public:
    struct Totals {
        uint64_t presented = 0;
        uint64_t discarded = 0;
        uint64_t latencySamples = 0;
        uint64_t totalLatencyUs = 0;    // From decoded to on the display.
        uint32_t refreshNs = 0;
    };

    // From decoded to on the display, on the LiGetMicroseconds clock, or -1 when the
    // sample can't be trusted. The desktop says when it showed the frame on its own
    // clock, so both clocks are read at once and the frame's age carried across. A
    // presentation time at or after the desktop's now, or one before the frame was
    // decoded, is a clock quirk, not a latency.
    static int64_t displayLatencyUs(uint64_t desktopNowNs, uint64_t presentedNs, uint64_t nowUs, uint64_t decodedUs)
    {
        if (presentedNs >= desktopNowNs) return -1;
        const uint64_t ageUs = (desktopNowNs - presentedNs) / 1000;
        if (ageUs >= nowUs) return -1;
        const uint64_t onDisplayUs = nowUs - ageUs;
        return onDisplayUs > decodedUs ? int64_t(onDisplayUs - decodedUs) : -1;
    }

    // Whether the desktop is drawing the window. One it isn't (hidden, minimised, on
    // another workspace) stops answering frame callbacks, and what becomes of its frames
    // says nothing about the picture. True where the desktop can't say.
    static bool windowVisible();

    PresentationMonitor();
    ~PresentationMonitor();

    // On the thread that swaps, which is the only thread to call any of these.
    bool initialize(SDL_Window* window);
    // Just before the swap. decodedUs is on the LiGetMicroseconds clock.
    void beforeSwap(uint64_t decodedUs);
    // A frame committed before the desktop has drawn the last one replaces it unseen, and
    // the picture skips. Waiting for the desktop stops that, at a cost of up to a refresh of
    // delay, so this only waits while frames are really landing badly. Whether they do
    // depends on when they arrive against the display's refresh, which drifts. It lets go
    // now and then to see if it's still needed, for longer each time it was.
    // A window that isn't being drawn won't answer, hence the timeout.
    void waitForDesktop(int timeoutMs);

    static Totals totals();
    static void resetTotals();

private:
#ifdef HAS_WAYLAND
    friend struct PresentationCallbacks;
    void finished(void* pending, bool presented, uint64_t presentedNs, uint32_t refreshNs);

    struct wl_display* m_Display = nullptr;
    struct wl_surface* m_Surface = nullptr;
    struct wl_event_queue* m_Queue = nullptr;
    struct wl_proxy* m_Presentation = nullptr;
    uint32_t m_ClockId = 1;
    struct wl_proxy* m_SurfaceWrapper = nullptr;
    struct wl_callback* m_FrameCallback = nullptr;
    bool m_Waiting = false;
    uint64_t m_FrameRequestedUs = 0;
    bool visibleNow(uint64_t nowUs) const;
    bool m_Holding = false;
    uint64_t m_HoldUntilUs = 0;
    uint64_t m_CleanSinceUs = 0;
    int m_HoldSeconds = 2;
    int m_WindowPresented = 0;
    int m_WindowDiscarded = 0;
    static constexpr int MaxPending = 8;
    struct Pending { struct wl_proxy* feedback; uint64_t decodedUs; PresentationMonitor* owner; } m_Pending[MaxPending] {};
#endif
};
