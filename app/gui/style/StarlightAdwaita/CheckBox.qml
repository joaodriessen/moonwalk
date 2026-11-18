import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G
import "../../starlight/Lucide.js" as Lucide

T.CheckBox {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: implicitIndicatorWidth + (text.length > 0 ? spacing + implicitContentWidth : 0)
    implicitHeight: Math.max(implicitIndicatorHeight, implicitContentHeight, 32)
    spacing: tokens.spacingS
    padding: 0
    leftPadding: mirrored ? 0 : implicitIndicatorWidth + spacing
    rightPadding: mirrored ? implicitIndicatorWidth + spacing : 0
    hoverEnabled: true
    font.pixelSize: tokens.bodySize
    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: control.mirrored ? control.width - width : 0
        y: Math.round((control.height - height) / 2)
        radius: 6
        color: control.checked ? control.tokens.accent
                               : (control.hovered ? control.tokens.controlHover : control.tokens.control)
        border.width: control.visualFocus ? control.tokens.focusWidth : 0
        border.color: control.tokens.focus
        Image {
            anchors.centerIn: parent
            width: 16; height: 16
            sourceSize: Qt.size(32, 32)
            visible: control.checked
            source: Lucide.url("check", "#ffffff")
        }
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? control.tokens.text : control.tokens.disabledText
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
    }
}
