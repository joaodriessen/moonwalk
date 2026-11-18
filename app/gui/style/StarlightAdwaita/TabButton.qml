import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

T.TabButton {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Math.max(36, implicitContentHeight + topPadding + bottomPadding)
    padding: 6
    leftPadding: tokens.spacingM
    rightPadding: tokens.spacingM
    hoverEnabled: true
    font.pixelSize: tokens.bodySize
    font.bold: true
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? control.tokens.text : control.tokens.disabledText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: control.tokens.radiusControl
        color: control.checked ? control.tokens.card
                               : (control.hovered ? control.tokens.control : "transparent")
        border.width: control.visualFocus ? control.tokens.focusWidth : 0
        border.color: control.tokens.focus
    }
}
