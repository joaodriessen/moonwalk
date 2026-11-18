import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

// Adwaita popover menu item: flat, rounded hover fill, destructive
// variant in red text.
T.MenuItem {
    id: root

    property string accessibleName: text
    property string accessibleDescription: ""
    property bool destructive: false
    readonly property GnomeTokens tokens: GnomeTokens {}

    // Menu's ListView still lays out invisible items. Collapse unavailable
    // actions, and let large text grow rather than clipping inside a fixed row.
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: visible
                    ? Math.max(32, implicitContentHeight + topPadding + bottomPadding)
                    : 0
    topPadding: 6
    bottomPadding: 6
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: tokens.bodySize

    function controllerActivate() {
        ControlCompat.activate(root)
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: !root.enabled
               ? root.tokens.disabledText
               : (root.destructive
                  ? root.tokens.destructiveText : root.tokens.text)
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 9
        color: root.down
               ? (root.tokens.dark ? Qt.rgba(1,1,1,0.19) : Qt.rgba(0,0,6/255,0.19))
               : (root.highlighted || root.hovered
                  ? (root.tokens.dark ? Qt.rgba(1,1,1,0.10) : Qt.rgba(0,0,6/255,0.10))
                  : "transparent")
    }

    Accessible.role: Accessible.MenuItem
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
