import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

T.Label {
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    color: enabled ? tokens.text : tokens.disabledText
    linkColor: tokens.accentText
    font.pixelSize: tokens.bodySize
}
