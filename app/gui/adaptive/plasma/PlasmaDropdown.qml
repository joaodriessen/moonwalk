import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

ComboBox {
    id: root

    property string accessibleName: ""
    property string accessibleDescription: ""

    focusPolicy: Qt.TabFocus

    function controllerActivate() {
        if (!root.enabled) {
            return
        }
        if (root.popup.opened) {
            root.popup.close()
        } else {
            root.popup.open()
        }
    }

    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName.length > 0 ? accessibleName : displayText
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
