import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

T.ItemDelegate {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: tokens.spacingS
    leftPadding: tokens.spacingM
    rightPadding: tokens.spacingM
    hoverEnabled: true
    font.pixelSize: tokens.bodySize
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? control.tokens.text : control.tokens.disabledText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitHeight: 40
        radius: control.tokens.radiusControl
        readonly property real amount: control.down ? 0.16
                                       : (control.highlighted ? (control.hovered ? 0.13 : 0.10)
                                                              : (control.hovered ? 0.07 : 0))
        color: control.tokens.dark ? Qt.rgba(1, 1, 1, amount) : Qt.rgba(0, 0, 6 / 255, amount)
        border.width: control.visualFocus ? control.tokens.focusWidth : 0
        border.color: control.tokens.focus
    }
}
