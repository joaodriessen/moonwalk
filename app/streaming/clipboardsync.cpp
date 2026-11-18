#include "clipboardsync.h"
#include "pngclipboard.h"

#include "SDL_compat.h"

#include <QCryptographicHash>

#include <cstring>

namespace {

// Long-poll GET timeout: must exceed the host's own ~25s hold budget (design
// doc §1.2) so a healthy host is never mistaken for an unreachable one. 30s gives
// headroom without meaningfully delaying detection of a truly dead host.
constexpr int kLongPollTimeoutMs = 30000;

// The one-shot focus-edge "pull" (design §1.4) is deliberately budgeted far below
// the steady-state long-poll: it exists to catch up promptly around a focus
// transition, not to hold a connection open. A timeout here is the ordinary case
// (most of the time nothing changed) and must NOT be treated as a network failure -
// see the isProbe handling in ClipboardSync::run().
constexpr int kFocusEdgeProbeTimeoutMs = 3000;

// POST (client -> host push) keeps a short timeout - it's a small immediate
// request, not a hold.
constexpr int kPostTimeoutMs = 5000;

// Defensive wake-up cadence while parked (unfocused, or permanently disabled).
// Nothing time-critical depends on this; it's a safety net against any missed
// wakeCondition signal (a standard defensive pattern for condition variables) and
// bounds how long a stop request could theoretically wait to be noticed if a wake()
// were somehow lost.
constexpr unsigned long kParkPollMs = 5000;

constexpr uint32_t kBaseBackoffMs = 2000;
constexpr uint32_t kMaxBackoffMs = 30000;

constexpr char kChangeCountHeader[] = "X-Skylight-Clipboard-Change";

int SDLCALL discardClipboardApplyEvent(void *, SDL_Event *event)
{
    if (event->type == SDL_USEREVENT && event->user.code == SDL_CODE_CLIPBOARD_APPLY) {
        SDL_free(event->user.data1);
        return 0;
    }
    if (event->type == SDL_USEREVENT && event->user.code == SDL_CODE_CLIPBOARD_IMAGE_APPLY) {
        delete static_cast<QByteArray*>(event->user.data1);
        return 0;
    }
    return 1;
}

}

ClipboardSync::ClipboardSync(NvComputer *computer, bool imageSupported, bool legacyText) :
    m_Computer(computer), m_ImageSupported(imageSupported && !legacyText), m_LegacyText(legacyText)
{
    setObjectName(QStringLiteral("ClipboardSync"));
}

ClipboardSync::~ClipboardSync()
{
    // Safety net: session.cpp's teardown is expected to call stopAndWait()
    // explicitly before deleting us (see DispatchDeferredCleanup), but do it here
    // too in case some path we haven't anticipated skips that. Idempotent: setting
    // an already-true stop flag and QThread::wait()ing an already-finished (or
    // never-started) thread both return immediately.
    stopAndWait();
}

QByteArray ClipboardSync::hashContent(const QByteArray &utf8Bytes)
{
    // SHA-256 over raw UTF-8 bytes, no normalization - see ClipboardLoopGuard doc comment.
    return QCryptographicHash::hash(utf8Bytes, QCryptographicHash::Sha256).toHex();
}

bool ClipboardSync::withinSizeCap(const QByteArray &utf8Bytes)
{
    // Inclusive 1 MiB cap, measured in UTF-8 bytes (matches SizeGuard.swift).
    return utf8Bytes.size() <= ClipboardLoopGuard::kMaxClipboardBytes;
}

void ClipboardSync::registerFailure(uint32_t nowMs)
{
    m_ConsecutiveFailures++;

    uint32_t backoff = kBaseBackoffMs;
    for (int i = 1; i < m_ConsecutiveFailures && backoff < kMaxBackoffMs; i++) {
        backoff *= 2;
    }
    if (backoff > kMaxBackoffMs) {
        backoff = kMaxBackoffMs;
    }

    m_BackoffUntilMs = nowMs + backoff;
}

void ClipboardSync::registerSuccess()
{
    m_ConsecutiveFailures = 0;
    m_BackoffUntilMs = 0;
}

uint32_t ClipboardSync::backoffRemainingMs() const
{
    if (m_BackoffUntilMs == 0) return 0;
    uint32_t now = SDL_GetTicks();
    // SDL_TICKS_PASSED-style wraparound-safe comparison: if m_BackoffUntilMs is
    // already behind `now`, there's nothing left to wait out.
    if (SDL_TICKS_PASSED(now, m_BackoffUntilMs)) {
        return 0;
    }
    return m_BackoffUntilMs - now;
}

void ClipboardSync::wake()
{
    QMutexLocker locker(&m_CondMutex);
    m_WakeCondition.wakeAll();
}

void ClipboardSync::parkWithTimeout(unsigned long ms)
{
    QMutexLocker locker(&m_CondMutex);
    if (m_StopRequested.load(std::memory_order_acquire)) {
        return;
    }
    m_WakeCondition.wait(&m_CondMutex, ms);
}

void ClipboardSync::setFocused(bool focused)
{
    {
        QMutexLocker locker(&m_PushMutex);
        if (focused && !m_Focused.load(std::memory_order_relaxed)) {
            m_FocusGeneration.fetch_add(1, std::memory_order_acq_rel);
        }
        m_Focused.store(focused, std::memory_order_release);
        if (!focused) {
            ++m_PushRevision;
            m_PendingPush.clear();
            m_HasPendingPush = false;
        }
    }
    m_CancelToken.interrupt();
    if (!focused) m_PushCancelToken.abortIfArmed();
    wake();
}

void ClipboardSync::requestFocusEdgePull()
{
    m_FocusEdgePending.store(true, std::memory_order_release);
    m_CancelToken.interrupt();
    wake();
}

bool ClipboardSync::validPng(const QByteArray &png) { return PngClipboard::valid(png); }
QByteArray ClipboardSync::imageHash(const QByteArray &png) { return PngClipboard::hash(png); }
bool ClipboardSync::canApplyRemote(quintptr generation) const {
    const int direction=m_Direction.load(std::memory_order_acquire);
    return !m_StopRequested.load(std::memory_order_acquire) &&
        (direction==1 || direction==3) && generation==focusGeneration();
}
void ClipboardSync::reportRemoteApplyFailure(quintptr generation, const QByteArray &content, bool image)
{
    if (!canApplyRemote(generation) || content.isEmpty() ||
            content.size() > (image ? PngClipboard::maxBytes : ClipboardLoopGuard::kMaxClipboardBytes)) return;
    const auto hash = image ? imageHash(content) : hashContent(content);
    {
        QMutexLocker locker(&m_ApplyMutex);
        if (m_QueuedApply.generation != generation || m_QueuedApply.hash != hash || m_QueuedApply.failed) return;
        m_QueuedApply.failed = true;
    }
    m_CancelToken.interrupt();
    wake();
}

void ClipboardSync::processRemoteApplyFailure(int64_t &since)
{
    QueuedApply failed;
    {
        QMutexLocker locker(&m_ApplyMutex);
        if (!m_QueuedApply.failed) return;
        failed = m_QueuedApply;
        m_QueuedApply = {};
    }
    if (m_StopRequested.load(std::memory_order_acquire)) return;
    // A newer local copy can arrive after valid failure feedback. Clear this
    // failed optimistic digest so an identical local copy is still uploadable,
    // but never rewind a newer epoch or a cursor already advanced past this value.
    m_Guard.forgetPulled(failed.hash);
    if (canApplyRemote(failed.generation) && since == failed.newSince) {
        since = failed.previousSince;
        registerFailure(SDL_GetTicks());
    }
}

void ClipboardSync::setDirection(int direction) {
    if(direction<0 || direction>3) return;
    {
        QMutexLocker locker(&m_PushMutex);
        if(m_Direction.exchange(direction,std::memory_order_acq_rel)==direction) return;
        m_FocusGeneration.fetch_add(1,std::memory_order_acq_rel);
        ++m_PushRevision; m_PendingPush.clear(); m_HasPendingPush=false;
    }
    m_CancelToken.interrupt(); m_PushCancelToken.interrupt(); wake();
}
void ClipboardSync::enqueueLocalImage(const QByteArray &png) {
    if(!m_ImageSupported) { emit warning(tr("This host does not support image clipboard transfer.")); return; }
    if(!validPng(png)) { emit warning(tr("Clipboard image is invalid or exceeds 4 MiB / 4096 pixels.")); return; }
    {
        QMutexLocker locker(&m_PushMutex);
        const int direction=m_Direction.load(std::memory_order_acquire);
        if(!m_Focused.load(std::memory_order_acquire) || (direction!=1 && direction!=2))return;
        m_PendingPush=png; m_PendingPushPng=true; m_HasPendingPush=true; ++m_PushRevision;
        m_FocusGeneration.fetch_add(1,std::memory_order_acq_rel);
    }
    m_CancelToken.interrupt(); wake();
}

void ClipboardSync::enqueueLocalChange(const QByteArray &utf8Text)
{
    {
        QMutexLocker locker(&m_PushMutex);
        const int direction=m_Direction.load(std::memory_order_acquire);
        if (!m_Focused.load(std::memory_order_acquire) || (direction!=1 && direction!=2)) return;
        m_PendingPushPng=false;
        m_PendingPush = utf8Text;
        m_HasPendingPush = true;
        ++m_PushRevision;
        // Invalidate an older read, including an apply event already queued to
        // SDL. It must not overwrite the copy now waiting for upload.
        m_FocusGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
    // Also latch when the worker is between draining the queue and arming GET.
    m_CancelToken.interrupt();
    wake();
}

void ClipboardSync::stopAndWait()
{
    m_StopRequested.store(true, std::memory_order_release);

    // Cancel any in-flight long-poll GET so shutdown doesn't have to wait out the
    // host's ~25-30s hold budget - see NvHTTP::CancelToken.
    m_CancelToken.cancel();
    m_PushCancelToken.cancel();

    // Wake anything parked (unfocused park, or a backoff sleep) so it notices the
    // stop flag immediately instead of waiting out its own timeout.
    wake();

    // QThread::wait() with no arguments waits forever for run() to return; if the
    // thread was never started (isRunning() false and never run) this returns
    // immediately, making this safe to call unconditionally and idempotently.
    wait();
}

void ClipboardSync::discardQueuedApplyEvents()
{
    SDL_FilterEvents(discardClipboardApplyEvent, nullptr);
}

bool ClipboardSync::marshalApplyToSdlThread(const QByteArray &utf8Text, quintptr generation)
{
    // SDL_PushEvent() is documented thread-safe and is the only cross-thread
    // primitive used to reach the SDL/main thread from here - see session.cpp's
    // SDL_USEREVENT/SDL_CODE_CLIPBOARD_APPLY case, which publishes through
    // NativeClipboard on that thread (see the class doc in clipboardsync.h).
    //
    // Ownership: we SDL_malloc a null-terminated copy here; the SDL thread's
    // handler SDL_free()s it after publication - the same malloc/free-
    // across-threads contract already used by session.cpp's
    // SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS case (SdlInputHandler::
    // setAdaptiveTriggers() frees the SDL_malloc'd DualSenseOutputReport).
    char *copy = static_cast<char *>(SDL_malloc(static_cast<size_t>(utf8Text.size()) + 1));
    if (copy == nullptr) {
        // OOM - drop rather than crash. Nothing to free; nothing was pushed.
        return false;
    }
    std::memcpy(copy, utf8Text.constData(), static_cast<size_t>(utf8Text.size()));
    copy[utf8Text.size()] = '\0';

    SDL_Event event = {};
    event.type = SDL_USEREVENT;
    event.user.code = SDL_CODE_CLIPBOARD_APPLY;
    event.user.data1 = copy;
    event.user.data2 = reinterpret_cast<void *>(generation);

    // SDL_PushEvent() returns 1 on success, 0 if filtered out, -1 on error. In
    // either non-success case, the SDL loop will never see this event to free our
    // copy for us, so we must free it ourselves here to avoid a leak.
    if (SDL_PushEvent(&event) <= 0) {
        SDL_free(copy);
        return false;
    }
    return true;
}

void ClipboardSync::clearPendingPush(quint64 revision)
{
    QMutexLocker locker(&m_PushMutex);
    if (revision == m_PushRevision) {
        m_PendingPush.clear();
        m_HasPendingPush = false;
    }
}

bool ClipboardSync::hasPendingPush()
{
    QMutexLocker locker(&m_PushMutex);
    return m_HasPendingPush;
}

void ClipboardSync::processPendingPush(NvHTTP &http, bool &permanentlyDisabled)
{
    if (!m_WriteAllowed.load()) {
        QMutexLocker locker(&m_PushMutex);
        m_HasPendingPush = false;
        m_PendingPush.clear();
        return;
    }
    QByteArray text;
    quint64 revision;
    quintptr generation;
    bool png;
    {
        QMutexLocker locker(&m_PushMutex);
        if (!m_HasPendingPush || !m_Focused.load(std::memory_order_acquire)) return;
        text = m_PendingPush;
        png = m_PendingPushPng;
        revision = m_PushRevision;
        generation = focusGeneration();
    }

    const int direction=m_Direction.load(std::memory_order_acquire);
    if (permanentlyDisabled || (direction!=1 && direction!=2) || text.isEmpty() || (png ? !validPng(text) : !withinSizeCap(text))) {
        clearPendingPush(revision);
        return;
    }
    const QByteArray hash = png ? imageHash(text) : hashContent(text);
    if (!m_Guard.shouldPush(hash)) {
        clearPendingPush(revision);
        return;
    }
    if (backoffRemainingMs() > 0) return;

    // Never start a stale queued value after focus loss, replacement, or a newer copy.
    {
        QMutexLocker locker(&m_PushMutex);
        if (!m_HasPendingPush || revision != m_PushRevision ||
            !m_Focused.load(std::memory_order_acquire) || generation != focusGeneration()) return;
    }
    if (m_StopRequested.load(std::memory_order_acquire)) return;

    try {
        http.openConnectionPost(http.m_BaseUrlHttps,
                                "actions/clipboard", png ? "type=png" : "type=text", text,
                                png ? QStringLiteral("image/png") : QStringLiteral("text/plain; charset=utf-8"),
                                kPostTimeoutMs, NvHTTP::NVLL_ERROR);
        m_Guard.markPushed(hash);
        clearPendingPush(revision); // A newer queued copy still belongs to its own revision.
        registerSuccess();
    }
    catch (const GfeHttpResponseException &e) {
        if (!m_StopRequested.load(std::memory_order_acquire)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Clipboard push failed: %s", e.what());
            if (m_LegacyText && e.getStatusCode() == 401) {
                m_WriteAllowed = false;
                clearPendingPush(revision);
                emit warning(tr("The host denied writing its clipboard. Enable Set clipboard for this device in the host settings."));
            }
            else if (m_LegacyText && e.getStatusCode() == 403) registerFailure(SDL_GetTicks());
            else if (e.getStatusCode() == 400 || e.getStatusCode() == 403 || e.getStatusCode() == 404) {
                permanentlyDisabled = true;
                clearPendingPush(revision);
            }
            else registerFailure(SDL_GetTicks());
        }
    }
    catch (const QtNetworkReplyException &e) {
        if (!m_StopRequested.load(std::memory_order_acquire) &&
            m_Focused.load(std::memory_order_acquire) && generation == focusGeneration()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Clipboard push failed: %s", e.what());
            registerFailure(SDL_GetTicks());
        }
    }
}

void ClipboardSync::handleLongPollResponse(const NvHTTP::RawResponse &raw,
                                           int64_t &since,
                                           bool &permanentlyDisabled,
                                           bool &loggedMissingHeaderOnce,
                                           quintptr generation)
{
    switch (raw.httpStatusCode) {
    case 200:
    case 204: {
        bool haveHeader = m_LegacyText;
        int64_t newSince = since;
        if (!raw.headerValue.isEmpty()) {
            bool ok = false;
            qint64 parsed = QString::fromUtf8(raw.headerValue).toLongLong(&ok);
            if (ok) {
                newSince = parsed;
                haveHeader = true;
            }
        }

        if (raw.httpStatusCode == 204) {
            // D1/D2 (design Part 2): a 204 (no change within the hold budget)
            // NEVER applies. There is no body to even consider.
            if (haveHeader) {
                since = newSince;
            }
            registerSuccess();
            return;
        }

        // 200: the host claims a genuine change. Still defensive - our own host
        // always sends this header, but a missing/unparseable one on a "real"
        // response is a contract violation we've never observed; treat it as
        // transient rather than trusting the body without knowing the new
        // changeCount (D1: never apply on ambiguous information).
        if (!haveHeader) {
            if (!loggedMissingHeaderOnce) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Clipboard long-poll: 200 response missing change-count header; ignoring");
                loggedMissingHeaderOnce = true;
            }
            registerFailure(SDL_GetTicks());
            return;
        }

        const int64_t previousSince = since;
        since = newSince;
        registerSuccess();

        if (raw.body.isEmpty()) {
            // D1: an empty body never applies, even on a nominal 200.
            return;
        }

        const bool png=m_ImageSupported && raw.contentType.startsWith("image/png");
        if ((png && !validPng(raw.body)) || (!png && !withinSizeCap(raw.body))) {
            emit warning(tr("Clipboard image is invalid or exceeds the supported size."));
            // D4 belt-and-suspenders: the host already refuses to return an
            // oversized pasteboard, but never apply a truncated/oversized value
            // here either.
            return;
        }

        if(!canApplyRemote(generation)) return;
        QByteArray hash = png ? imageHash(raw.body) : hashContent(raw.body);
        if (!m_Guard.shouldApplyRemote(hash)) {
            // D5: either an echo of our own last push, or identical to what we
            // already applied - never re-apply.
            QMutexLocker locker(&m_ApplyMutex);
            if (m_QueuedApply.generation == generation && m_QueuedApply.hash == hash) {
                m_QueuedApply.newSince = newSince;
            }
            return;
        }

        // Publish identity before enqueueing: the main thread may process the SDL
        // event and report failure before SDL_PushEvent returns on this thread.
        {
            QMutexLocker locker(&m_ApplyMutex);
            m_QueuedApply = {hash, generation, previousSince, newSince, false};
        }

        bool marshaled=false;
        if(png) {
            auto payload=new QByteArray(raw.body);
            SDL_Event event={}; event.type=SDL_USEREVENT;event.user.code=SDL_CODE_CLIPBOARD_IMAGE_APPLY;
            event.user.data1=payload;event.user.data2=reinterpret_cast<void*>(generation);
            marshaled=SDL_PushEvent(&event)>0;
            if(!marshaled)delete payload;
        } else marshaled=marshalApplyToSdlThread(raw.body,generation);
        if (marshaled) {
            m_Guard.markPulled(hash);
        }
        else {
            {
                QMutexLocker locker(&m_ApplyMutex);
                m_QueuedApply = {};
            }
            // Re-request this exact host value; advancing `since` here would
            // make the host return 204 forever until another clipboard change.
            since = previousSince;
            registerFailure(SDL_GetTicks());
        }
        return;
    }

    case 413:
        emit warning(tr("Host clipboard image exceeds 4 MiB or 4096 pixels; the local clipboard was preserved."));
        if(!raw.headerValue.isEmpty()) {bool ok=false;auto next=raw.headerValue.toLongLong(&ok);if(ok)since=next;}
        return;
    case 401:
        if (m_LegacyText) {
            m_ReadAllowed = false;
            emit warning(tr("The host denied reading its clipboard. Enable Read clipboard for this device in the host settings."));
            return;
        }
        permanentlyDisabled = true;
        return;
    case 403:
        if (m_LegacyText) {
            // This protocol uses 403 while the client's stream registration is
            // absent. Permissions use 401; a startup/reconnect race is retryable.
            registerFailure(SDL_GetTicks());
            return;
        }
    case 404:
    case 400:
        // Graceful degradation (design Part 2): stock Sunshine/Apollo/GFE host (no
        // /actions/clipboard route -> 404), clipboard_sync=false host-side (403),
        // or an unsupported type (400 - shouldn't occur, we always send
        // type=text). Latch permanently: zero further retries, zero further log
        // spam for the rest of this session.
        if (!permanentlyDisabled) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Host has no native clipboard support (HTTP %d); disabling clipboard sync for this session",
                        raw.httpStatusCode);
        }
        permanentlyDisabled = true;
        return;

    default:
        // 503 (no active session yet - a start-of-stream race) and any other
        // unexpected status: transient. Keep `since` unchanged and back off.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Clipboard long-poll: unexpected HTTP %d; retrying", raw.httpStatusCode);
        registerFailure(SDL_GetTicks());
        return;
    }
}

void ClipboardSync::run()
{
    // Constructed here, NOT in the constructor, so NvHTTP's child
    // QNetworkAccessManager is thread-affine to THIS thread (the dedicated
    // clipboard thread) rather than whatever thread constructed ClipboardSync (the
    // SDL/main thread) - design doc §1.3's "constructed inside the thread, not
    // moved" requirement.
    NvHTTP http(m_Computer);
    http.setCancelToken(&m_PushCancelToken);

    int64_t since = 0;
    bool permanentlyDisabled = false;
    bool loggedMissingHeaderOnce = false;

    while (!m_StopRequested.load(std::memory_order_acquire)) {
        processRemoteApplyFailure(since);
        processPendingPush(http, permanentlyDisabled);
        // Preserve failed/newer outgoing values until acknowledged. Do not wait behind
        // another long poll or let an older remote value overwrite an unsent local copy.
        if (hasPendingPush() && !permanentlyDisabled && !m_StopRequested.load(std::memory_order_acquire)) {
            parkWithTimeout(qMax<uint32_t>(1, backoffRemainingMs()));
            continue;
        }

        if (m_StopRequested.load(std::memory_order_acquire)) {
            break;
        }

        if (permanentlyDisabled) {
            // Parked forever: no endpoint, or disabled host-side. We still wake
            // periodically only to notice a stop request promptly; no network
            // happens on this path ever again this session.
            parkWithTimeout(kParkPollMs);
            continue;
        }

        const int direction=m_Direction.load(std::memory_order_acquire);
        if((direction!=1 && direction!=3) || !m_ReadAllowed.load()) { parkWithTimeout(kParkPollMs); continue; }
        const quintptr generation = focusGeneration();
        bool focused = m_Focused.load(std::memory_order_acquire);
        bool edgePull = m_FocusEdgePending.exchange(false, std::memory_order_acq_rel);

        if (!focused && !edgePull) {
            // Focus-scoping (design §5 / Part 2): no host reads at all while
            // unfocused, outside of the one-shot focus-edge probe below.
            parkWithTimeout(kParkPollMs);
            continue;
        }

        // A probe is an edge-triggered pull while not in the (focused) steady
        // state: short budget, and a plain timeout on it is the ordinary case
        // (not a failure) - see the exception handling below.
        bool isProbe = !focused;
        int timeoutMs = isProbe || m_LegacyText ? kFocusEdgeProbeTimeoutMs : kLongPollTimeoutMs;

        // Respect a backoff window before issuing the next request (steady-state
        // failures only; a probe never sleeps out a backoff - see below).
        if (!isProbe) {
            uint32_t remain = backoffRemainingMs();
            if (remain > 0) {
                parkWithTimeout(remain);
                continue;
            }
        }

        NvHTTP::RawResponse raw;
        bool haveResponse = false;

        try {
            raw = http.openConnectionRaw(http.m_BaseUrlHttps,
                                         "actions/clipboard",
                                         m_LegacyText ? QStringLiteral("type=text") :
                                             QStringLiteral("type=%1&since=%2").arg(m_ImageSupported ? QStringLiteral("auto") : QStringLiteral("text")).arg(since),
                                         timeoutMs,
                                         QByteArray(kChangeCountHeader),
                                         NvHTTP::NVLL_ERROR,
                                         &m_CancelToken,
                                         m_ImageSupported ? PngClipboard::maxBytes : ClipboardLoopGuard::kMaxClipboardBytes);
            haveResponse = true;
        }
        catch (const GfeHttpResponseException &e) {
            if (m_StopRequested.load(std::memory_order_acquire)) {
                break;
            }
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Clipboard long-poll failed: %s", e.what());
            registerFailure(SDL_GetTicks());
        }
        catch (const QtNetworkReplyException &e) {
            if (m_StopRequested.load(std::memory_order_acquire)) {
                break;
            }

            const bool interrupted = e.getError() == QNetworkReply::OperationCanceledError;
            bool isPlainTimeout = (e.getError() == QNetworkReply::TimeoutError);
            if (!interrupted && !(isProbe && isPlainTimeout)) {
                // A steady-state timeout is a real signal (the host normally
                // never holds past ~25s), and ANY non-timeout transport error
                // (connection refused, DNS failure, TLS/cert mismatch) is a real
                // signal even on a probe. Only a probe's plain timeout is the
                // expected common case (its budget is deliberately far shorter
                // than the host's own hold) and must not count as a failure.
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Clipboard long-poll failed: %s", e.what());
                registerFailure(SDL_GetTicks());
            }
        }

        if (haveResponse && generation == focusGeneration()) {
            handleLongPollResponse(raw, since, permanentlyDisabled, loggedMissingHeaderOnce, generation);
        }
        // Apollo/Vibepollo returns immediately. Bound steady polling to 2Hz;
        // focus changes, outgoing copies and stop still wake the worker early.
        if (m_LegacyText && haveResponse && !m_StopRequested.load()) parkWithTimeout(500);

        // Steady-state: re-issue immediately on every 200/204/failure (backoff is
        // enforced at the top of the loop, not here) - this is what gives the
        // long-poll its push-like latency. Probe: loop straight back to the top,
        // which will re-evaluate focus/edge state and typically park.
    }
}
