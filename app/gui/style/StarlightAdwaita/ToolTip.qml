import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

// GtkTooltip: a small dark rounded bubble in both colour schemes.
T.ToolTip {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - 6
    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
    margins: 6
    padding: 6
    leftPadding: 10
    rightPadding: 10
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent
    font.pixelSize: tokens.bodySize
    contentItem: Text {
        text: control.text
        font: control.font
        wrapMode: Text.Wrap
        color: "#ffffff"
    }
    background: Rectangle {
        radius: control.tokens.radiusControl
        color: Qt.rgba(0.11, 0.11, 0.13, 0.95)
    }
}
