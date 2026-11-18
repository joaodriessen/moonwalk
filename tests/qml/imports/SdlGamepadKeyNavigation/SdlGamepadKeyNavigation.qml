pragma Singleton
import QtQml 2.15

// Boundary adapter: records QML focus notifications without initializing SDL.
QtObject {
    property int enableCalls: 0
    property int disableCalls: 0
    property var focusNotifications: []
    property bool uiNavigation: false
    property int connectedGamepads: 0

    function reset() {
        enableCalls = 0
        disableCalls = 0
        focusNotifications = []
        uiNavigation = false
        connectedGamepads = 0
    }

    function enable() {
        enableCalls += 1
    }

    function disable() {
        disableCalls += 1
    }

    function notifyWindowFocus(hasFocus) {
        focusNotifications = focusNotifications.concat([hasFocus])
    }

    function setUiNavMode(enabled) {
        uiNavigation = enabled
    }

    function getConnectedGamepads() {
        return connectedGamepads
    }
}
