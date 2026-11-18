import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

T.MenuSeparator {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: 1 + topPadding + bottomPadding
    topPadding: 6
    bottomPadding: 6
    leftPadding: 4
    rightPadding: 4
    contentItem: Rectangle { implicitWidth: 120; implicitHeight: 1; color: control.tokens.border }
}
