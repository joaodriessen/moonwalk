import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

Dialog {
    id: root

    property string accessibleName: title
    property string accessibleDescription: ""
    // Qt exposes the popup item's Dialog role to AT-SPI. Put the body
    // description there rather than adding a second semantic dialog child.
    readonly property Item accessibleItem: contentItem ? contentItem.parent : null

    // Declared inside a pane that may be hidden; it belongs to the whole window.
    parent: Overlay.overlay
    modal: true
    anchors.centerIn: parent

    function synchronizeAccessibility() {
        if (!accessibleItem) return
        accessibleItem.Accessible.role = Accessible.Dialog
        accessibleItem.Accessible.name = accessibleName
        accessibleItem.Accessible.description = accessibleDescription
    }
    onAccessibleItemChanged: synchronizeAccessibility()
    onAccessibleNameChanged: synchronizeAccessibility()
    onAccessibleDescriptionChanged: synchronizeAccessibility()
    Component.onCompleted: synchronizeAccessibility()

    property QtObject _accessibleIdentity: AppStyle.AccessibleIdentity {
        target: root.accessibleItem
        stableId: root.objectName
    }
}
