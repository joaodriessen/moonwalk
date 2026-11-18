import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

Slider {
    id: root

    property string accessibleName: ""
    property string accessibleDescription: ""

    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.Slider
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onIncreaseAction: {
        if (root.enabled) {
            root.increase()
        }
    }
    Accessible.onDecreaseAction: {
        if (root.enabled) {
            root.decrease()
        }
    }

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
