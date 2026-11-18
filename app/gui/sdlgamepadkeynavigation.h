#pragma once

#include <QTimer>
#include <QEvent>

#include <optional>

#include "SDL_compat.h"

#include "application/controllerintent.h"
#include "settings/streamingpreferences.h"

struct ControllerKeyMapping
{
    Qt::Key key;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
};

// Pure translation and polling policy kept separate from the Qt/SDL
// dispatcher so the load-bearing controller contract can be characterized
// without starting SDL's mapping manager or a real input device.
class ControllerKeyMapper
{
public:
    static constexpr Uint32 AxisNavigationRepeatDelayMs = 150;

    static std::optional<ControllerKeyMapping> mapButton(Uint8 button, bool uiNavMode)
    {
        const auto intent = ControllerIntent::fromButton(button);
        if (!intent) {
            return std::nullopt;
        }
        return mapIntent(intent->action, uiNavMode);
    }

    static std::optional<ControllerKeyMapping> mapAxis(Sint16 leftX, Sint16 leftY, bool uiNavMode)
    {
        const auto intent = ControllerIntent::fromAxis(leftX, leftY);
        if (!intent) {
            return std::nullopt;
        }
        return mapIntent(intent->action, uiNavMode);
    }

    static ControllerKeyMapping mapIntent(ControllerIntent::Action action, bool uiNavMode)
    {
        switch (action) {
        case ControllerIntent::Action::NavigateUp:
            return verticalMapping(Qt::Key_Up, uiNavMode, true);
        case ControllerIntent::Action::NavigateDown:
            return verticalMapping(Qt::Key_Down, uiNavMode, false);
        case ControllerIntent::Action::NavigateLeft:
            return ControllerKeyMapping { Qt::Key_Left };
        case ControllerIntent::Action::NavigateRight:
            return ControllerKeyMapping { Qt::Key_Right };
        case ControllerIntent::Action::Confirm:
            return ControllerKeyMapping { uiNavMode ? Qt::Key_Space : Qt::Key_Return };
        case ControllerIntent::Action::Back:
            return ControllerKeyMapping { Qt::Key_Escape };
        case ControllerIntent::Action::OpenContextMenu:
            return ControllerKeyMapping { Qt::Key_Menu };
        case ControllerIntent::Action::OpenSettings:
            // main.qml uses this when a focused control consumes Key_Menu.
            return ControllerKeyMapping { Qt::Key_Hangup };
        }

        Q_UNREACHABLE();
    }

    static bool shouldPoll(bool enabled, bool hasFocus)
    {
        return enabled && hasFocus;
    }

    static bool shouldFlushFirstPoll(bool firstPoll)
    {
        return firstPoll;
    }

    static bool canRepeatAxisNavigation(Uint32 now, Uint32 lastEventTime)
    {
        return now - lastEventTime >= AxisNavigationRepeatDelayMs;
    }

private:
    static ControllerKeyMapping verticalMapping(Qt::Key arrow, bool uiNavMode, bool up)
    {
        if (!uiNavMode) {
            return ControllerKeyMapping { arrow };
        }
        return ControllerKeyMapping { Qt::Key_Tab, up ? Qt::ShiftModifier : Qt::NoModifier };
    }
};

class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT

public:
    SdlGamepadKeyNavigation(StreamingPreferences* prefs);

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void notifyWindowFocus(bool hasFocus);

    Q_INVOKABLE void setUiNavMode(bool settingsMode);

    Q_INVOKABLE int getConnectedGamepads();

private:
    void sendIntent(QEvent::Type type, const ControllerIntent& intent);
    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    void updateTimerState();

private slots:
    void onPollingTimerFired();

private:
    StreamingPreferences* m_Prefs;
    QTimer* m_PollingTimer;
    QList<SDL_GameController*> m_Gamepads;
    bool m_Enabled;
    bool m_UiNavMode;
    bool m_FirstPoll;
    bool m_HasFocus;
    Uint32 m_LastAxisNavigationEventTime;
};
