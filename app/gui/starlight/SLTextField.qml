import QtQuick 2.9
import Starlight 1.0

// Themed single-line text input, built directly on TextInput — NOT on the
// styled Controls TextField. The Material style (the app's Controls style)
// gives TextField a floating-label placeholder animation that levitates the
// placeholder above the field on focus; wrapping it and overriding
// `background` doesn't suppress that contentItem behavior (seen live in the
// add-host dialog: the placeholder ghosted through the field's top border).
// Building on TextInput sidesteps the style system entirely, so the look is
// 100% Theme-owned: segBg fill, hairline border going accent on focus, an
// in-field placeholder that simply hides once there's text.
//
// Interface kept from the TextField era: text/placeholderText/validator/
// inputMethodHints/maximumLength/clear()/forceActiveFocus(), plus a
// returnPressed() signal callers wire to their dialog's accept().
FocusScope {
    id: root

    property alias text: input.text
    property alias placeholderText: placeholder.text
    property alias validator: input.validator
    property alias inputMethodHints: input.inputMethodHints
    property alias maximumLength: input.maximumLength
    property alias readOnly: input.readOnly
    property string accessibleName: placeholderText
    property string accessibleDescription: ""
    // The semantic object is the actual TextInput; exposing it makes the
    // focus target explicit to callers and to the contract test.
    readonly property Item accessibleItem: input

    signal returnPressed()

    function clear() { input.clear() }
    function selectAll() { input.selectAll() }

    implicitWidth: 200
    implicitHeight: input.implicitHeight + 12

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusControl
        color: Theme.segBg
        // Disabled visual (previously none at all -- audit §4): matches
        // SLDropdown's enabled/0.5-opacity + label3-text convention.
        opacity: root.enabled ? 1.0 : 0.5
        border.width: input.activeFocus ? 2 : 1
        border.color: input.activeFocus ? Theme.accent : Theme.hairline

        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        focus: true
        clip: true

        color: root.enabled ? Theme.label : Theme.label3
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentInk
        font.pixelSize: Theme.sizeBody

        Keys.onReturnPressed: root.returnPressed()
        Keys.onEnterPressed: root.returnPressed()

        Accessible.role: Accessible.EditableText
        Accessible.name: root.accessibleName
        Accessible.description: root.accessibleDescription
        Accessible.editable: !input.readOnly
        Accessible.readOnly: input.readOnly
        Accessible.focusable: root.enabled && !input.readOnly
        Accessible.focused: input.activeFocus
        Accessible.onPressAction: {
            if (root.enabled && !input.readOnly) {
                input.forceActiveFocus()
            }
        }

        AccessibleIdentity {
            target: input
            stableId: root.objectName
        }
    }

    Text {
        id: placeholder
        anchors.fill: input
        verticalAlignment: Text.AlignVCenter
        visible: input.text.length === 0 && !input.inputMethodComposing
        color: Theme.label3
        font.pixelSize: Theme.sizeBody
        elide: Text.ElideRight
    }
}
