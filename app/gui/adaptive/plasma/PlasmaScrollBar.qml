import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

ScrollBar {
    id: root

    property string accessibleName: orientation === Qt.Vertical
                                    ? qsTr("Vertical scroll bar")
                                    : qsTr("Horizontal scroll bar")
    property string accessibleDescription: ""

    interactive: true

    Accessible.role: Accessible.ScrollBar
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: visible && enabled && interactive
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onIncreaseAction: {
        if (root.enabled && root.interactive) {
            root.increase()
        }
    }
    Accessible.onDecreaseAction: {
        if (root.enabled && root.interactive) {
            root.decrease()
        }
    }
    Accessible.onScrollUpAction: {
        if (root.enabled && root.interactive) {
            root.decrease()
        }
    }
    Accessible.onScrollDownAction: {
        if (root.enabled && root.interactive) {
            root.increase()
        }
    }

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
