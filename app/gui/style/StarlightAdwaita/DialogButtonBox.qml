import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

// Responses of an Adwaita alert: filled, equal-width, 12px apart.
T.DialogButtonBox {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: Math.max(implicitBackgroundWidth, contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, contentHeight + topPadding + bottomPadding)
    spacing: tokens.spacingM
    padding: tokens.spacingXL
    topPadding: tokens.spacingM
    alignment: count === 1 ? Qt.AlignRight : undefined
    delegate: G.GnomeDialogButton {
        width: control.count === 1 ? implicitWidth : undefined
        suggested: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                   || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
    }
    contentItem: ListView {
        implicitWidth: contentWidth
        implicitHeight: 44
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }
    background: null
}
