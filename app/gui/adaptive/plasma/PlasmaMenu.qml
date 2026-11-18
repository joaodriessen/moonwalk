import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

Menu {
    id: root

    property string accessibleName: title
    property string accessibleDescription: ""
    readonly property Item accessibleItem: semanticItem
    margins: 6

    property Item _semanticItem: Item {
        id: semanticItem
        parent: root.contentItem
        Accessible.role: Accessible.PopupMenu
        Accessible.name: root.accessibleName
        Accessible.description: root.accessibleDescription

        AppStyle.AccessibleIdentity {
            target: semanticItem
            stableId: root.objectName
        }
    }
}
