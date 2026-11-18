#include "streaming/session.h"
#include "clipboardtext.h"
#include "streaming/nativeclipboard.h"

#include <Limelight.h>
#include "SDL_compat.h"

#if defined(Q_OS_LINUX) && defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRunnable>
#include <QThreadPool>
#include <functional>
#endif

#define VK_0 0x30
#define VK_A 0x41

#if defined(Q_OS_LINUX) && defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
static void queueLocalControl(std::function<void()> action)
{
    static QThreadPool pool;
    static const bool configured = []() {
        pool.setMaxThreadCount(1);
        return true;
    }();
    Q_UNUSED(configured);
    pool.start(QRunnable::create(std::move(action)));
}

static void adjustLocalBrightness(bool increase)
{
    const QDir backlights(QStringLiteral("/sys/class/backlight"));
    for (const QString& name : backlights.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = backlights.filePath(name);
        QFile currentFile(base + QStringLiteral("/brightness"));
        QFile maximumFile(base + QStringLiteral("/max_brightness"));
        if (!currentFile.open(QIODevice::ReadOnly) || !maximumFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        bool currentOk = false;
        bool maximumOk = false;
        const int current = currentFile.readAll().trimmed().toInt(&currentOk);
        const int maximum = maximumFile.readAll().trimmed().toInt(&maximumOk);
        if (!currentOk || !maximumOk || maximum <= 0) {
            continue;
        }

        const int step = qMax(1, maximum / 20);
        const int target = qBound(1, current + (increase ? step : -step), maximum);
        QDBusInterface session(QStringLiteral("org.freedesktop.login1"),
                               QStringLiteral("/org/freedesktop/login1/session/auto"),
                               QStringLiteral("org.freedesktop.login1.Session"),
                               QDBusConnection::systemBus());
        session.setTimeout(250);
        const QDBusMessage reply = session.call(QStringLiteral("SetBrightness"),
                                                QStringLiteral("backlight"), name,
                                                static_cast<uint>(target));
        if (reply.type() != QDBusMessage::ErrorMessage) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Set local brightness to %d/%d", target, maximum);
            return;
        }
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unable to adjust local brightness: %s",
                    qPrintable(reply.errorMessage()));
    }
}

static void adjustLocalVolume(SDL_Scancode key)
{
    auto runPactl = [](const QStringList &arguments) {
        QProcess process;
        process.start(QStringLiteral("pactl"), arguments);
        if (!process.waitForStarted(1000)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unable to start local volume control");
            return;
        }
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Local volume control timed out");
        }
        else if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Local volume control failed");
        }
    };
    QStringList args { QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@") };
    if (key == SDL_SCANCODE_MUTE || key == SDL_SCANCODE_AUDIOMUTE) {
        args << QStringLiteral("toggle");
    }
    else {
        args[0] = QStringLiteral("set-sink-volume");
        args << (key == SDL_SCANCODE_VOLUMEUP ? QStringLiteral("+5%") : QStringLiteral("-5%"));
    }

    runPactl(args);
    if (key == SDL_SCANCODE_VOLUMEUP) {
        runPactl({ QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), QStringLiteral("0") });
    }
}
#endif

// These are real Windows VK_* codes
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F13 0x7C
#define VK_NUMPAD0 0x60
#endif

void SdlInputHandler::performSpecialKeyCombo(KeyCombo combo)
{
    switch (combo) {
    case KeyComboQuit:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected quit key combo");

        // Push a quit event to the main loop
        SDL_Event event;
        event.type = SDL_QUIT;
        event.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&event);
        break;

    case KeyComboUngrabInput:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected mouse capture toggle combo");

        // Stop handling future input
        setCaptureActive(!isCaptureActive());
        Session::get()->m_HaveCaptureOverride = true;
        Session::get()->m_DesiredCapture = isCaptureActive();

        // Force raise all keys to ensure they aren't stuck,
        // since we won't get their key up events.
        raiseAllKeys();
        break;

    case KeyComboToggleFullScreen:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected full-screen toggle combo");
        Session::s_ActiveSession->toggleFullscreen();

        // Force raise all keys just be safe across this full-screen/windowed
        // transition just in case key events get lost.
        raiseAllKeys();
        break;

    case KeyComboToggleStatsOverlay:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected stats toggle combo");

        // Toggle the stats overlay
        Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayDebug,
                                                            !Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug));
        break;

    case KeyComboToggleMouseMode:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected mouse mode toggle combo");

        // Uncapture input
        setCaptureActive(false);

        // Toggle mouse mode
        m_AbsoluteMouseMode = !m_AbsoluteMouseMode;
        Session::get()->notifyMouseModeChanged(m_AbsoluteMouseMode);

        // Recapture input
        setCaptureActive(true);
        break;

    case KeyComboToggleCursorHide:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected show mouse combo");

        if (!SDL_GetRelativeMouseMode()) {
            if (!Session::get()->m_CursorPolicy.mayToggleLocalCursor()) {
                Session::get()->warnCursorReconnectRequired();
                break;
            }
            m_MouseCursorCapturedVisibilityState = !m_MouseCursorCapturedVisibilityState;
            SDL_ShowCursor(m_MouseCursorCapturedVisibilityState);
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Cursor can only be shown in remote desktop mouse mode");
        }
        break;

    case KeyComboToggleMinimize:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected minimize combo");
        SDL_MinimizeWindow(m_Window);
        break;

    case KeyComboPasteText:
    {
        // Manual "force-type" fallback - kept alongside the native in-stream clipboard
        // sync (streaming/clipboardsync.h), not replaced by it. Different mechanism:
        // this synthesizes Unicode key events into the focused host field
        // (LiSendUtf8TextEvent) rather than setting the host pasteboard, so it still
        // works into fields/apps that don't read the pasteboard, works even if
        // clipboard-sync is disabled, and is explicit/user-initiated. It never touches
        // ClipboardSync's LoopGuard and only ever fires on this hotkey, so it cannot
        // double-sync or loop with the automatic push/pull path.
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected type clipboard text combo");

        // Force raise all keys to ensure that none of them interfere
        // with the text we're going to type.
        raiseAllKeys();

        bool tooLarge = false;
        auto text = NativeClipboard::readText(&tooLarge);
        if (tooLarge) {
            Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::ClipboardTooLarge, SDL_GetTicks());
        }
        else if (!text.isEmpty()) {
            const auto prepared = ClipboardText::prepare(text.data());
            if (prepared.status == ClipboardText::Status::Ready) {
                // One bounded queue operation preserves ordering with key events.
                if (LiSendUtf8TextEvent(text.constData(), static_cast<unsigned int>(prepared.length)) != 0) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not queue clipboard text for typing");
                    Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::ClipboardQueueFailed, SDL_GetTicks());
                }
                else {
                    // A new accepted request supersedes an earlier rejection, without
                    // claiming that the host has delivered or inserted the text yet.
                    Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::None, SDL_GetTicks());
                }
            }
            else if (prepared.status == ClipboardText::Status::TooLarge) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Type Clipboard skipped: text exceeds 1 MiB");
                Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::ClipboardTooLarge, SDL_GetTicks());
            }
            else {
                Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::ClipboardEmpty, SDL_GetTicks());
            }
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "No text in clipboard to paste!");
            Session::get()->m_StreamStatus.showClipboardNotice(StreamStatus::Notice::ClipboardEmpty, SDL_GetTicks());
        }
        break;
    }

    case KeyComboTogglePointerRegionLock:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected pointer region lock toggle combo");
        m_PointerRegionLockActive = !m_PointerRegionLockActive;

        // Remember that the user changed this manually, so we don't mess with it anymore
        // during windowed <-> full-screen transitions.
        m_PointerRegionLockToggledByUser = true;

        // Apply the new region lock
        updatePointerRegionLock();
        break;

    case KeyComboQuitAndExit:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected quitAndExit key combo");

        // Indicate that we want to exit afterwards
        Session::get()->setShouldExit(true);

        // Push a quit event to the main loop
        SDL_Event quitExitEvent;
        quitExitEvent.type = SDL_QUIT;
        quitExitEvent.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&quitExitEvent);
        break;

    case KeyComboToggleKeyboardGrab:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected keyboard grab toggle combo");

        // Toggle the system key capture mode
        if (isSystemKeyCaptureActive()) {
            m_CaptureSystemKeysMode = StreamingPreferences::CSK_OFF;
        }
        else {
            m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
        }
        Session::get()->m_Preferences->captureSysKeysMode = m_CaptureSystemKeysMode;

        updateKeyboardGrabState();
        break;

    case KeyComboControls:
        Session::get()->openControls();
        break;

    default:
        Q_UNREACHABLE();
    }
}

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    short keyCode;
    char modifiers;
    bool shouldNotConvertToScanCodeOnServer = false;

    if (event->repeat) {
        // Ignore repeat key down events
        SDL_assert(event->state == SDL_PRESSED);
        return;
    }

#if defined(Q_OS_LINUX) && defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
    if (event->keysym.scancode == SDL_SCANCODE_BRIGHTNESSUP ||
        event->keysym.scancode == SDL_SCANCODE_BRIGHTNESSDOWN) {
        if (event->state == SDL_PRESSED) {
            const bool increase = event->keysym.scancode == SDL_SCANCODE_BRIGHTNESSUP;
            queueLocalControl([increase]() { adjustLocalBrightness(increase); });
        }
        return;
    }
    if (event->keysym.scancode == SDL_SCANCODE_VOLUMEUP ||
        event->keysym.scancode == SDL_SCANCODE_VOLUMEDOWN ||
        event->keysym.scancode == SDL_SCANCODE_MUTE ||
        event->keysym.scancode == SDL_SCANCODE_AUDIOMUTE) {
        if (event->state == SDL_PRESSED) {
            const SDL_Scancode key = event->keysym.scancode;
            queueLocalControl([key]() { adjustLocalVolume(key); });
        }
        return;
    }
#endif

    // Check for our special key combos
    if ((event->state == SDL_PRESSED) &&
            (event->keysym.mod & KMOD_CTRL) &&
            (event->keysym.mod & KMOD_ALT) &&
            (event->keysym.mod & KMOD_SHIFT)) {
        // First we test the SDLK combos for matches,
        // that way we ensure that latin keyboard users
        // can match to the key they see on their keyboards.
        // If nothing matches that, we'll then go on to
        // checking scancodes so non-latin keyboard users
        // can have working hotkeys (though possibly in
        // odd positions). We must do all SDLK tests before
        // any scancode tests to avoid issues in cases
        // where the SDLK for one shortcut collides with
        // the scancode of another.

        for (int i = 0; i < KeyComboMax; i++) {
            if (m_SpecialKeyCombos[i].enabled && event->keysym.sym == m_SpecialKeyCombos[i].keyCode) {
                performSpecialKeyCombo(m_SpecialKeyCombos[i].keyCombo);
                return;
            }
        }

        for (int i = 0; i < KeyComboMax; i++) {
            if (m_SpecialKeyCombos[i].enabled && event->keysym.scancode == m_SpecialKeyCombos[i].scanCode) {
                performSpecialKeyCombo(m_SpecialKeyCombos[i].keyCombo);
                return;
            }
        }
    }

    if (m_LocalTextInput.suppress(event->keysym.scancode, event->keysym.sym,
                                 event->keysym.mod, event->state == SDL_PRESSED)) return;

    // Set modifier flags
    modifiers = 0;
    if (event->keysym.mod & KMOD_CTRL) {
        modifiers |= MODIFIER_CTRL;
    }
    if (event->keysym.mod & KMOD_ALT) {
        modifiers |= MODIFIER_ALT;
    }
    if (event->keysym.mod & KMOD_SHIFT) {
        modifiers |= MODIFIER_SHIFT;
    }
    if (event->keysym.mod & KMOD_GUI) {
        if (isSystemKeyCaptureActive()) {
            modifiers |= MODIFIER_META;
        }
    }

    // Set keycode. We explicitly use scancode here because GFE will try to correct
    // for AZERTY layouts on the host but it depends on receiving VK_ values matching
    // a QWERTY layout to work.
    if (event->keysym.scancode >= SDL_SCANCODE_1 && event->keysym.scancode <= SDL_SCANCODE_9) {
        // SDL defines SDL_SCANCODE_0 > SDL_SCANCODE_9, so we need to handle that manually
        keyCode = (event->keysym.scancode - SDL_SCANCODE_1) + VK_0 + 1;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_A && event->keysym.scancode <= SDL_SCANCODE_Z) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_A) + VK_A;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_F1 && event->keysym.scancode <= SDL_SCANCODE_F12) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_F1) + VK_F1;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_F13 && event->keysym.scancode <= SDL_SCANCODE_F24) {
        keyCode = (event->keysym.scancode - SDL_SCANCODE_F13) + VK_F13;
    }
    else if (event->keysym.scancode >= SDL_SCANCODE_KP_1 && event->keysym.scancode <= SDL_SCANCODE_KP_9) {
        // SDL defines SDL_SCANCODE_KP_0 > SDL_SCANCODE_KP_9, so we need to handle that manually
        keyCode = (event->keysym.scancode - SDL_SCANCODE_KP_1) + VK_NUMPAD0 + 1;
    }
    else {
        switch (event->keysym.scancode) {
            case SDL_SCANCODE_MUTE:
            case SDL_SCANCODE_AUDIOMUTE:
                keyCode = 0xAD; // VK_VOLUME_MUTE
                break;
            case SDL_SCANCODE_VOLUMEDOWN:
                keyCode = 0xAE; // VK_VOLUME_DOWN
                break;
            case SDL_SCANCODE_VOLUMEUP:
                keyCode = 0xAF; // VK_VOLUME_UP
                break;
            case SDL_SCANCODE_BACKSPACE:
                keyCode = 0x08;
                break;
            case SDL_SCANCODE_TAB:
                keyCode = 0x09;
                break;
            case SDL_SCANCODE_CLEAR:
                keyCode = 0x0C;
                break;
            case SDL_SCANCODE_KP_ENTER: // FIXME: Is this correct?
            case SDL_SCANCODE_RETURN:
                keyCode = 0x0D;
                break;
            case SDL_SCANCODE_PAUSE:
                keyCode = 0x13;
                break;
            case SDL_SCANCODE_CAPSLOCK:
                keyCode = 0x14;
                break;
            case SDL_SCANCODE_ESCAPE:
                keyCode = 0x1B;
                break;
            case SDL_SCANCODE_SPACE:
                keyCode = 0x20;
                break;
            case SDL_SCANCODE_PAGEUP:
                keyCode = 0x21;
                break;
            case SDL_SCANCODE_PAGEDOWN:
                keyCode = 0x22;
                break;
            case SDL_SCANCODE_END:
                keyCode = 0x23;
                break;
            case SDL_SCANCODE_HOME:
                keyCode = 0x24;
                break;
            case SDL_SCANCODE_LEFT:
                keyCode = 0x25;
                break;
            case SDL_SCANCODE_UP:
                keyCode = 0x26;
                break;
            case SDL_SCANCODE_RIGHT:
                keyCode = 0x27;
                break;
            case SDL_SCANCODE_DOWN:
                keyCode = 0x28;
                break;
            case SDL_SCANCODE_SELECT:
                keyCode = 0x29;
                break;
            case SDL_SCANCODE_EXECUTE:
                keyCode = 0x2B;
                break;
            case SDL_SCANCODE_PRINTSCREEN:
                keyCode = 0x2C;
                break;
            case SDL_SCANCODE_INSERT:
                keyCode = 0x2D;
                break;
            case SDL_SCANCODE_DELETE:
                keyCode = 0x2E;
                break;
            case SDL_SCANCODE_HELP:
                keyCode = 0x2F;
                break;
            case SDL_SCANCODE_KP_0:
                // See comment above about why we only handle SDL_SCANCODE_KP_0 here
                keyCode = VK_NUMPAD0;
                break;
            case SDL_SCANCODE_0:
                // See comment above about why we only handle SDL_SCANCODE_0 here
                keyCode = VK_0;
                break;
            case SDL_SCANCODE_KP_MULTIPLY:
                keyCode = 0x6A;
                break;
            case SDL_SCANCODE_KP_PLUS:
                keyCode = 0x6B;
                break;
            case SDL_SCANCODE_KP_COMMA:
                keyCode = 0x6C;
                break;
            case SDL_SCANCODE_KP_MINUS:
                keyCode = 0x6D;
                break;
            case SDL_SCANCODE_KP_PERIOD:
                keyCode = 0x6E;
                break;
            case SDL_SCANCODE_KP_DIVIDE:
                keyCode = 0x6F;
                break;
            case SDL_SCANCODE_NUMLOCKCLEAR:
                keyCode = 0x90;
                break;
            case SDL_SCANCODE_SCROLLLOCK:
                keyCode = 0x91;
                break;
            case SDL_SCANCODE_LSHIFT:
                keyCode = 0xA0;
                break;
            case SDL_SCANCODE_RSHIFT:
                keyCode = 0xA1;
                break;
            case SDL_SCANCODE_LCTRL:
                keyCode = 0xA2;
                break;
            case SDL_SCANCODE_RCTRL:
                keyCode = 0xA3;
                break;
            case SDL_SCANCODE_LALT:
                keyCode = 0xA4;
                break;
            case SDL_SCANCODE_RALT:
                keyCode = 0xA5;
                break;
            case SDL_SCANCODE_LGUI:
                if (!isSystemKeyCaptureActive()) {
                    return;
                }
                keyCode = 0x5B;
                break;
            case SDL_SCANCODE_RGUI:
                if (!isSystemKeyCaptureActive()) {
                    return;
                }
                keyCode = 0x5C;
                break;
            case SDL_SCANCODE_APPLICATION:
                keyCode = 0x5D;
                break;
            case SDL_SCANCODE_AC_BACK:
                keyCode = 0xA6;
                break;
            case SDL_SCANCODE_AC_FORWARD:
                keyCode = 0xA7;
                break;
            case SDL_SCANCODE_AC_REFRESH:
                keyCode = 0xA8;
                break;
            case SDL_SCANCODE_AC_STOP:
                keyCode = 0xA9;
                break;
            case SDL_SCANCODE_AC_SEARCH:
                keyCode = 0xAA;
                break;
            case SDL_SCANCODE_AC_BOOKMARKS:
                keyCode = 0xAB;
                break;
            case SDL_SCANCODE_AC_HOME:
                keyCode = 0xAC;
                break;
            case SDL_SCANCODE_SEMICOLON:
                keyCode = 0xBA;
                break;
            case SDL_SCANCODE_EQUALS:
                keyCode = 0xBB;
                break;
            case SDL_SCANCODE_COMMA:
                keyCode = 0xBC;
                break;
            case SDL_SCANCODE_MINUS:
                keyCode = 0xBD;
                break;
            case SDL_SCANCODE_PERIOD:
                keyCode = 0xBE;
                break;
            case SDL_SCANCODE_SLASH:
                keyCode = 0xBF;
                break;
            case SDL_SCANCODE_GRAVE:
                keyCode = 0xC0;
                break;
            case SDL_SCANCODE_LEFTBRACKET:
                keyCode = 0xDB;
                break;
            case SDL_SCANCODE_INTERNATIONAL3:
                shouldNotConvertToScanCodeOnServer = true;
                Q_FALLTHROUGH();
            case SDL_SCANCODE_BACKSLASH:
                keyCode = 0xDC;
                break;
            case SDL_SCANCODE_RIGHTBRACKET:
                keyCode = 0xDD;
                break;
            case SDL_SCANCODE_APOSTROPHE:
                keyCode = 0xDE;
                break;
            case SDL_SCANCODE_INTERNATIONAL1:
                shouldNotConvertToScanCodeOnServer = true;
                Q_FALLTHROUGH();
            case SDL_SCANCODE_NONUSBACKSLASH:
                keyCode = 0xE2;
                break;
            case SDL_SCANCODE_LANG1:
                keyCode = 0x1C;
                break;
            case SDL_SCANCODE_LANG2:
                keyCode = 0x1D;
                break;
            default:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Unhandled button event: %d",
                             event->keysym.scancode);
                return;
        }
    }

    // Track the key state so we always know which keys are down
    if (event->state == SDL_PRESSED) {
        m_KeysDown.insert(keyCode);
    }
    else {
        // A focus/control transition may already have released this key.
        if (!m_KeysDown.remove(keyCode)) return;
    }

    LiSendKeyboardEvent2(0x8000 | keyCode,
                        event->state == SDL_PRESSED ?
                            KEY_ACTION_DOWN : KEY_ACTION_UP,
                        modifiers,
                        shouldNotConvertToScanCodeOnServer ? SS_KBE_FLAG_NON_NORMALIZED : 0);

}
