import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle

T.ScrollBar {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: orientation === Qt.Vertical
                                    ? qsTr("Vertical scroll bar")
                                    : qsTr("Horizontal scroll bar")
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}

    interactive: true
    minimumSize: 0.08
    policy: ScrollBar.AsNeeded
    visible: size < 1.0
    padding: 3
    property bool accessiblePositionPending: false
    property real pendingAccessiblePosition: 0

    function adjustAccessiblePosition(direction) {
        var basePosition = accessiblePositionPending
                ? pendingAccessiblePosition : root.position
        var accessibleDirection = root.enabled && root.interactive
                ? direction : 0
        pendingAccessiblePosition = Math.max(0,
                                             Math.min(1 - root.size,
                                                      basePosition
                                                      + accessibleDirection
                                                      * root.stepSize))
        if (accessiblePositionPending)
            return

        accessiblePositionPending = true
        Qt.callLater(function() {
            root.position = root.pendingAccessiblePosition
            root.accessiblePositionPending = false
        })
    }

    // _scrolling.scss overlay-indicator: a 3px slider at rest that widens to
    // the 8px slider width while hovering. currentColor at 20/40/60% for
    // rest/hover/active, with the outline colour keeping it legible on any
    // background.
    readonly property bool expanded: hovered || active || pressed

    contentItem: Rectangle {
        implicitWidth: root.expanded ? 8 : 3
        implicitHeight: implicitWidth
        radius: Math.min(width, height) / 2
        // The alpha lives in the colour rather than in opacity: the active
        // Controls style animates contentItem.opacity imperatively, which
        // would overwrite a binding here.
        color: Qt.rgba(root.tokens.dark ? 1 : 0,
                       root.tokens.dark ? 1 : 0,
                       root.tokens.dark ? 1 : 6 / 255,
                       !root.enabled ? 0
                                     : (root.pressed
                                        ? 0.6
                                        : (root.expanded ? 0.4 : 0.2)))
        border.width: 1
        border.color: Qt.rgba(root.tokens.scrollbarOutline.r,
                              root.tokens.scrollbarOutline.g,
                              root.tokens.scrollbarOutline.b,
                              root.expanded ? 0.6 : 0.35)

        Behavior on implicitWidth {
            NumberAnimation { duration: root.tokens.motionDuration }
        }
        Behavior on opacity {
            NumberAnimation { duration: root.tokens.motionDuration }
        }
    }

    // The trough only tints while hovering; otherwise it is fully transparent,
    // which is what makes an overlay scrollbar an overlay.
    background: Rectangle {
        color: Qt.rgba(root.tokens.dark ? 1 : 0,
                       root.tokens.dark ? 1 : 0,
                       root.tokens.dark ? 1 : 6 / 255,
                       root.expanded ? 0.1 : 0)
        radius: Math.min(width, height) / 2

        Behavior on color {
            ColorAnimation { duration: root.tokens.motionDuration }
        }
    }

    Accessible.role: Accessible.ScrollBar
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: visible && enabled && interactive
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onIncreaseAction: root.adjustAccessiblePosition(1)
    Accessible.onDecreaseAction: root.adjustAccessiblePosition(-1)
    Accessible.onScrollUpAction: root.adjustAccessiblePosition(-1)
    Accessible.onScrollDownAction: root.adjustAccessiblePosition(1)

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
