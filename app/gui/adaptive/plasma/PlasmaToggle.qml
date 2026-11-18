import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

CheckBox {
    id: root

    property string accessibleName: text
    property string accessibleDescription: ""

    focusPolicy: Qt.TabFocus

    function controllerActivate() {
        if (!enabled) {
            return
        }
        if (typeof click === "function") {
            click()
            return
        }
        // Qt 6.4 floor: AbstractButton.click() is absent and toggle() flips
        // checked without the user-interaction signals; emit them so facade
        // wiring behaves identically on both floors.
        toggle()
        toggled()
        clicked()
    }

    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.pressed: pressed
    Accessible.onPressAction: controllerActivate()
    Accessible.onToggleAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
