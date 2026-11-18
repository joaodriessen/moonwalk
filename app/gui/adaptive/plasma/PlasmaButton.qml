import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

Button {
    id: root

    property string accessibleName: text
    property string accessibleDescription: ""

    focusPolicy: Qt.TabFocus

    function controllerActivate() {
        ControlCompat.activate(root)
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
