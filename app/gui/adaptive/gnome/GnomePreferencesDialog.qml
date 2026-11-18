import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Window 2.15
import ".." as Shared

// AdwDialog: floats over the hosts page, closes with its button or Esc, and
// never takes the person away from where they were.
T.Popup {
    id: root
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property var settingsRoute: routeLoader.item
    property bool routeLoaded: false
    onAboutToShow: routeLoaded = true
    onClosed: routeLoaded = false
    signal effectRequested(var effect, var route)

    parent: Overlay.overlay
    modal: true
    focus: true
    closePolicy: T.Popup.NoAutoClose
    padding: 0
    // The CSD shadow margin is part of the overlay but not of the window proper.
    property int outerMargin: 0
    width: parent ? Math.min(640, parent.width - 2 * outerMargin - 16) : 640
    height: parent ? Math.min(720, parent.height - 2 * outerMargin - 16) : 720
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    T.Overlay.modal: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: Window.window && Window.window.csdMargin !== undefined ? Window.window.csdMargin : 0
            radius: Window.window && Window.window.csdRadius !== undefined ? Window.window.csdRadius : 0
            color: Qt.rgba(0, 0, 0, 0.32)
        }
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: root.tokens.reducedMotion ? 0 : 120; easing.type: Easing.InCubic }
    }
    background: Rectangle {
        radius: root.tokens.radiusDialog
        color: root.tokens.dialogSurface
        border.width: root.tokens.highContrast ? root.tokens.borderWidth : (root.tokens.dark ? 1 : 0)
        border.color: root.tokens.dark ? Qt.rgba(1, 1, 1, 0.1) : root.tokens.border
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: {
            if (!root.settingsRoute) return
            // An open search closes first, wherever in the dialog focus has gone.
            var surface = root.settingsRoute.legacySurface
            if (surface && surface.searchOpen === true) surface.closeSearch()
            else root.settingsRoute.page.requestBack(false)
        }
    }
    contentItem: Loader {
        id: routeLoader
        // Outlives visibility, so the route deactivates before it is destroyed.
        active: root.routeLoaded
        sourceComponent: Shared.SettingsPageRoute {
            shellRouteId: "adaptive-settings"
            hostedInDialog: true
            dialogActive: root.visible
            onWindowEffectRequested: function(effect) { root.effectRequested(effect, this) }
        }
    }
}
