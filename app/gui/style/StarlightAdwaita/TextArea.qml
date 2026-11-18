import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

T.TextArea {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding, 220)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding, tokens.fieldHeight)
    padding: tokens.spacingS
    font.pixelSize: tokens.bodySize
    color: enabled ? tokens.text : tokens.disabledText
    placeholderTextColor: tokens.secondaryText
    selectionColor: tokens.accent
    selectedTextColor: tokens.accentInk
    background: Rectangle {
        radius: control.tokens.radiusControl
        color: control.tokens.control
    }
}
