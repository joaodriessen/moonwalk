import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle

T.TextField {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: placeholderText
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}

    hoverEnabled: true
    leftPadding: tokens.spacingM
    rightPadding: tokens.spacingM
    topPadding: tokens.spacingS
    bottomPadding: tokens.spacingS
    implicitWidth: 220
    implicitHeight: Math.max(tokens.fieldHeight,
                             contentHeight + topPadding + bottomPadding)
    font.pixelSize: tokens.bodySize
    color: enabled ? tokens.text : tokens.disabledText
    // Material's floating label is suppressed; a static GNOME-style
    // placeholder is drawn below instead.
    placeholderTextColor: "transparent"
    selectionColor: tokens.accent
    selectedTextColor: tokens.accentInk

    background: Rectangle {
        radius: root.tokens.radiusControl
        color: root.tokens.control
        // Adwaita entries carry a 1px inset border at rest, accent on focus.
        border.width: root.activeFocus ? root.tokens.focusWidth : 1
        border.color: root.activeFocus ? root.tokens.focus : root.tokens.border

        Text {
            anchors.fill: parent
            anchors.leftMargin: root.leftPadding
            anchors.rightMargin: root.rightPadding
            visible: root.displayText.length === 0
                     && root.preeditText.length === 0
            text: root.placeholderText
            font: root.font
            color: root.tokens.secondaryText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

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
