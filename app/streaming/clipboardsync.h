#pragma once

//
// In-stream native clipboard sync for Starlight <-> Skylight.
//
// Design doc: Moonwalk Host repository: scratchpad/design-clipboard-v2.md ("Definitive
// Native Design", Part 1 threading + Part 2 safety + Wave C). Supersedes the Phase-1
// SDL-loop-driven busy-poll (docs/design-clipboard.md) - THIS is Phase 2.
//
// THREADING MODEL (load-bearing - design Part 1.3):
//   - ClipboardSync is now a dedicated QThread. It owns ALL networking: its own
//     NvHTTP (and therefore its own child QNetworkAccessManager - constructed
//     inside run(), i.e. ON this thread, never moved here from another thread), the
//     long-poll GET loop (host -> client), the POST (client -> host), the
//     ClipboardLoopGuard, and transport SHA-256 hashing. Failure feedback hashes
//     its bounded attempted content on the caller before signaling this worker.
//     It NEVER calls any
//     SDL_*ClipboardText function.
//   - The SDL/main thread owns ALL SDL_*ClipboardText calls (SDL_HasClipboardText /
//     SDL_GetClipboardText / SDL_SetClipboardText / SDL_CLIPBOARDUPDATE). SDL
//     clipboard access is bound to the windowing connection (Wayland/X11) and is
//     NOT safe off the SDL/video thread - this is why the split exists at all.
//   - Handoff clip-thread -> SDL-thread (apply a pulled host value): this class
//     never calls SDL_SetClipboardText itself. It marshals the text via
//     SDL_PushEvent(SDL_USEREVENT, code = SDL_CODE_CLIPBOARD_APPLY, data1 = an
//     SDL_malloc'd null-terminated UTF-8 copy) - SDL_PushEvent() is documented
//     thread-safe. Session::exec()'s SDL_USEREVENT switch (session.cpp) owns the
//     other half: it publishes through NativeClipboard, reports native failures
//     for guarded retry, and SDL_free()s the copy.
//   - Handoff SDL-thread -> clip-thread (push a local change): Session::exec()
//     reads SDL_CLIPBOARDUPDATE on the SDL thread, applies the focus + empty +
//     non-text guards THERE (that's where focus and SDL clipboard state are safely
//     readable), and calls enqueueLocalChange() below, which is a cheap thread-safe
//     mutex-protected handoff - never network I/O on the SDL thread.
//   - Focus is published from the SDL thread via setFocused() (an atomic<bool>).
//     While unfocused, the clip thread PARKS: no network at all, satisfying the
//     "unfocused window neither reads the host clipboard nor leaks the local one"
//     safety requirement (design §5 / Part 2). FOCUS_GAINED/FOCUS_LOST additionally
//     call requestFocusEdgePull() for a one-shot, short-budget pull on the edge.
//
// Lifetime: constructed AND started just before Session::exec()'s SDL loop, stopped
// (stopAndWait()) and deleted at DispatchDeferredCleanup - see session.cpp. Nothing
// this thread does can throw into or block the SDL render loop: all network errors
// are caught internally, and the long-poll's up-to-~30s wait is independently
// cancellable (CancelToken, see nvhttp.h) so teardown doesn't have to wait it out.
//
// Coexistence with Ctrl+Alt+Shift+V type-paste (app/streaming/input/keyboard.cpp,
// KeyComboPasteText): untouched, distinct mechanism (synthesizes Unicode key events
// via LiSendUtf8TextEvent; never touches the pasteboard or this guard), kept as a
// manual "force-type" fallback. It cannot loop with ClipboardSync.
//

#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include <atomic>
#include <cstdint>

#include "backend/nvhttp.h"

class NvComputer;

// SDL_USEREVENT code used to marshal a pulled host clipboard value from the clip
// thread to the SDL thread (see session.cpp's SDL_USEREVENT switch). Follows the
// existing codebase convention of plain #define'd SDL_USEREVENT codes rather than
// SDL_RegisterEvents() (see session.cpp's SDL_CODE_GAMECONTROLLER_* codes) - picked
// to not collide with the existing 100-105 range.
#define SDL_CODE_CLIPBOARD_APPLY 106
#define SDL_CODE_CLIPBOARD_IMAGE_APPLY 108

/**
 * @brief Faithful C++ reimplementation (~20 lines) of ClipSyncCore's LoopGuard +
 * SizeGuard semantics from the (now-removed) SSH ClipSyncCore Swift package.
 *
 * Semantics (unchanged from Phase 1):
 *  - Hash: SHA-256 over the raw UTF-8 bytes of the string. No normalization, no trim,
 *    no case-fold, no CRLF fixup (byte-exact end to end - normalizing would change the
 *    hash relative to what was pushed and defeat echo-suppression).
 *  - shouldPush(hash)/shouldApplyRemote(hash): both are `hash != lastPushed &&
 *    hash != lastPulled`. State is mutated by the caller only after a *successful*
 *    push/apply, never by the guard itself.
 *  - Fresh/empty state (both hashes empty) never matches a real 32-byte SHA-256 hash,
 *    so the first push/apply of a session always proceeds.
 *  - Empty-string clipboard contents are never synced (checked by the caller before
 *    hashing).
 *
 * Thread-safety: NOT internally synchronized. This is fine because exactly one
 * ClipboardSync instance owns exactly one ClipboardLoopGuard, and it is touched only
 * from that instance's own run() thread (both the push-processing and the
 * apply-processing paths live there now) - see clipboardsync.cpp.
 */
class ClipboardLoopGuard
{
public:
    // SizeGuard: 1 MiB inclusive, measured in UTF-8 bytes (matches SizeGuard.swift's
    // `byteCount <= 1_048_576`). Applied by the caller before hashing.
    static constexpr qint64 kMaxClipboardBytes = 1024 * 1024;

    bool shouldPush(const QByteArray &hash) const
    {
        return hash != m_LastPushedHash && hash != m_LastPulledHash;
    }

    bool shouldApplyRemote(const QByteArray &hash) const
    {
        return hash != m_LastPulledHash && hash != m_LastPushedHash;
    }

    // Call only after a successful POST to the host.
    void markPushed(const QByteArray &hash)
    {
        m_LastPushedHash = hash;
    }

    // Call only after successfully marshalling remote content for the SDL thread to
    // publish through NativeClipboard. An actual publication failure revokes it.
    void markPulled(const QByteArray &hash)
    {
        m_LastPulledHash = hash;
    }

    // Undo only the optimistic acknowledgement of the publication that failed.
    void forgetPulled(const QByteArray &hash)
    {
        if (m_LastPulledHash == hash) m_LastPulledHash.clear();
    }

private:
    QByteArray m_LastPushedHash;
    QByteArray m_LastPulledHash;
};

/**
 * @brief Dedicated clipboard networking thread (design doc Part 1.3 / Wave C).
 *
 * Owns its own NvHTTP (and its child QNetworkAccessManager, constructed inside
 * run() so it is thread-affine to this thread), the long-poll GET loop, the POST,
 * the ClipboardLoopGuard, and all hashing. Communicates with the SDL/main thread
 * exclusively through the thread-safe entry points below plus one SDL_PushEvent -
 * never via QClipboard, never by calling any SDL_*Clipboard* function itself.
 */
class ClipboardSync : public QThread
{
    Q_OBJECT

public:
    explicit ClipboardSync(NvComputer *computer, bool imageSupported = false, bool legacyText = false);
    ~ClipboardSync() override;

    ClipboardSync(const ClipboardSync &) = delete;
    ClipboardSync &operator=(const ClipboardSync &) = delete;

    // --- SDL thread -> clip thread entry points (all thread-safe, all cheap/non-
    // blocking - none of them ever touch the network on the calling thread) ---

    // Call from Session::exec()'s SDL loop on SDL_CLIPBOARDUPDATE, but only AFTER
    // applying the focus + empty + non-text guards there (design §1.3: those checks
    // depend on SDL-thread-only state - window focus flags and SDL_HasClipboardText
    // - so they must run on the SDL thread; everything after that, including the
    // size cap, hashing, and the loop guard, runs here on the clip thread instead).
    // Coalesces: only the latest enqueued value matters, matching clipboard
    // semantics (there is no history to preserve).
    void enqueueLocalChange(const QByteArray &utf8Text);
    void enqueueLocalImage(const QByteArray &png);
    void setDirection(int direction); // 0 paused, 1 both, 2 outgoing, 3 incoming
    void setPermissions(bool read, bool write) { m_ReadAllowed = read; m_WriteAllowed = write; }
    bool canApplyRemote(quintptr generation) const;
    // Call only when a current SDL apply event actually attempted publication and
    // the native backend failed. Bounded content identifies the exact queued value;
    // epoch/local-copy skips are not publication failures. No network runs here.
    void reportRemoteApplyFailure(quintptr generation, const QByteArray &content, bool image = false);
    static bool validPng(const QByteArray &png);
    static QByteArray imageHash(const QByteArray &png);

signals:
    void warning(const QString &message);

public:

    // Call on every SDL_WINDOWEVENT_FOCUS_GAINED/FOCUS_LOST with the new focus
    // state. Losing focus clears queued local pushes; a POST already in flight
    // may finish, but no queued push starts after the worker observes focus loss.
    void setFocused(bool focused);
    // Response epoch advances on focus regain and each local copy.
    quintptr focusGeneration() const { return m_FocusGeneration.load(std::memory_order_acquire); }

    // Call on both SDL_WINDOWEVENT_FOCUS_GAINED and SDL_WINDOWEVENT_FOCUS_LOST
    // (design §1.4's retained one-shot pull mechanism) in addition to setFocused().
    // Wakes the clip thread for a single short-budget ("non-blocking-budget", i.e.
    // deliberately much shorter than the steady-state long-poll's ~30s) probe GET,
    // so the host's latest value is fetched promptly around a focus transition
    // without waiting for the next full long-poll cycle. A no-op once the session
    // has permanently latched clipboard support off (see clipboardsync.cpp).
    void requestFocusEdgePull();

    // Stops the long-poll loop (cancels any in-flight request via CancelToken so
    // this does not block for the host's ~25s hold budget), wakes anything parked,
    // and QThread::wait()s for run() to return. Safe to call once from the SDL/main
    // thread during Session teardown; idempotent (also called from the destructor
    // as a safety net). Must be called (directly or via the destructor) before the
    // Session/NvComputer this was constructed with is destroyed.
    void stopAndWait();

    // SDL thread only, after stopAndWait(): free apply payloads that the
    // session event loop did not consume before teardown.
    static void discardQueuedApplyEvents();

protected:
    void run() override;

private:
    static QByteArray hashContent(const QByteArray &utf8Bytes);
    static bool withinSizeCap(const QByteArray &utf8Bytes);

    // Marshals a pulled host value to the SDL thread - see the class-level "Handoff
    // clip-thread -> SDL-thread" doc comment above. Never called with empty text.
    static bool marshalApplyToSdlThread(const QByteArray &utf8Text, quintptr generation);

    // POSTs the latest local value, retaining it until acknowledgement. A
    // no-op if nothing is pending or if clipboard support has permanently latched
    // off. Runs entirely on this thread.
    void processPendingPush(NvHTTP &http, bool &permanentlyDisabled);
    void clearPendingPush(quint64 revision);
    bool hasPendingPush();
    void processRemoteApplyFailure(int64_t &since);

    // Interprets one long-poll GET response: updates `since`, applies the
    // never-destroy-the-clipboard rules (D1/D2/D4/D5, see design Part 2), and
    // either marshals an apply to the SDL thread or does nothing. Sets
    // `permanentlyDisabled` on a 404/403/400 (design's graceful-degradation latch).
    void handleLongPollResponse(const NvHTTP::RawResponse &raw,
                               int64_t &since,
                               bool &permanentlyDisabled,
                               bool &loggedMissingHeaderOnce,
                               quintptr generation);

    // Wakes anything blocked in parkWithTimeout() below. Called after every
    // SDL-thread-facing entry point above so a parked thread reacts promptly.
    void wake();

    // Blocks the calling (this) thread for up to `ms`, or until woken by wake() /
    // stopAndWait(), whichever comes first. Used both for the "nothing to do right
    // now" (unfocused / permanently-disabled) park and for the failure backoff
    // sleep - either way it's interruptible so shutdown is never held up by it.
    void parkWithTimeout(unsigned long ms);

    void registerFailure(uint32_t nowMs);
    void registerSuccess();
    uint32_t backoffRemainingMs() const;

    NvComputer *m_Computer;
    const bool m_ImageSupported;
    const bool m_LegacyText;
    std::atomic<bool> m_ReadAllowed {true}, m_WriteAllowed {true};
    std::atomic<int> m_Direction { 1 };
    NvHTTP::CancelToken m_CancelToken;
    NvHTTP::CancelToken m_PushCancelToken;

    // --- state touched ONLY from run() (this thread) - no synchronization needed ---
    ClipboardLoopGuard m_Guard;
    uint32_t m_BackoffUntilMs = 0;
    int m_ConsecutiveFailures = 0;

    // --- cross-thread state ---
    std::atomic<quintptr> m_FocusGeneration { 0 };
    std::atomic<bool> m_Focused { false };
    std::atomic<bool> m_FocusEdgePending { false };
    std::atomic<bool> m_StopRequested { false };

    QMutex m_PushMutex;
    QByteArray m_PendingPush;
    bool m_PendingPushPng = false;
    bool m_HasPendingPush = false;
    quint64 m_PushRevision = 0;

    QMutex m_ApplyMutex;
    struct QueuedApply {
        QByteArray hash;
        quintptr generation = 0;
        int64_t previousSince = 0;
        int64_t newSince = 0;
        bool failed = false;
    } m_QueuedApply;

    QMutex m_CondMutex;
    QWaitCondition m_WakeCondition;
};
