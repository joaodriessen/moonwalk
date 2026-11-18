import QtQuick 2.9
import QtQuick.Controls 2.2
import Starlight 1.0

// macOS-style overlay scrollbar: a thin rounded thumb that lives on top of
// the content (no reserved layout space), invisible when idle. Per HIG: it
// stays fully visible while scrolling/hovered, HOLDS for ~1200ms after
// activity stops (holdTimer below), then fades out over ~400ms. Drop-in
// replacement for `ScrollBar { }` / `ScrollBar.vertical: ScrollBar {}`
// wherever a Flickable/ListView/ScrollView needs a scrollbar in the HIG surfaces.
ScrollBar {
    id: root

    property string accessibleName: orientation === Qt.Vertical
                                    ? qsTr("Vertical scroll bar")
                                    : qsTr("Horizontal scroll bar")
    property string accessibleDescription: ""

    policy: ScrollBar.AsNeeded
    interactive: true
    hoverEnabled: true

    Accessible.role: Accessible.ScrollBar
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    // Overlay scrollbars are intentionally removed from virtual focus while
    // visually hidden, matching the control's existing visual/focus route.
    Accessible.focusable: root.visible && root.enabled && root.interactive
    Accessible.focused: root.activeFocus
    Accessible.pressed: root.pressed
    Accessible.onIncreaseAction: {
        if (root.enabled && root.interactive) {
            root.increase()
        }
    }
    Accessible.onDecreaseAction: {
        if (root.enabled && root.interactive) {
            root.decrease()
        }
    }
    Accessible.onScrollUpAction: {
        if (root.enabled && root.interactive) {
            root.decrease()
        }
    }

    AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
    Accessible.onScrollDownAction: {
        if (root.enabled && root.interactive) {
            root.increase()
        }
    }

    // Overlay: never reserve layout space for the bar.
    padding: 0

    readonly property real thickness: 7
    readonly property real thicknessHovered: 9
    readonly property int holdDuration: 1200
    readonly property int fadeDuration: 400

    // True while actively scrolling/dragging or hovered.
    readonly property bool wantsVisible: root.policy !== ScrollBar.AlwaysOff &&
                                          root.size < 1.0 &&
                                          (root.active || root.pressed || root.hovered)

    // Latched "still showing" state: goes true immediately with wantsVisible,
    // but only drops back to false after holdTimer's delay -- this is what
    // actually drives opacity, so the fade-out doesn't start the instant
    // scrolling stops.
    property bool _holdVisible: false

    onWantsVisibleChanged: {
        if (wantsVisible) {
            holdTimer.stop()
            _holdVisible = true
        } else {
            holdTimer.restart()
        }
    }

    Timer {
        id: holdTimer
        interval: root.holdDuration
        onTriggered: root._holdVisible = false
    }

    opacity: root._holdVisible ? 1.0 : 0.0
    visible: opacity > 0.01

    Behavior on opacity {
        NumberAnimation {
            duration: root._holdVisible ? 100 : root.fadeDuration
            easing.type: Easing.OutCubic
        }
    }

    contentItem: Rectangle {
        implicitWidth: root.hovered || root.pressed ? root.thicknessHovered : root.thickness
        implicitHeight: root.hovered || root.pressed ? root.thicknessHovered : root.thickness
        radius: width / 2

        // Dark thumb in light mode, light thumb in dark mode -- translucent
        // either way, per the macOS overlay-scrollbar look.
        color: Theme.isDark ? Qt.rgba(1, 1, 1, root.pressed ? 0.55 : 0.4)
                             : Qt.rgba(0, 0, 0, root.pressed ? 0.5 : 0.38)

        Behavior on implicitWidth { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on implicitHeight { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    background: Item {}
}
