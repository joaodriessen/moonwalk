import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

TextField {
    id: root

    property string accessibleName: placeholderText
    property string accessibleDescription: ""

    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.editable: !readOnly
    Accessible.readOnly: readOnly
    Accessible.focusable: enabled && !readOnly
    Accessible.focused: activeFocus
    Accessible.onPressAction: {
        if (root.enabled && !root.readOnly) {
            root.forceActiveFocus()
        }
    }

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
