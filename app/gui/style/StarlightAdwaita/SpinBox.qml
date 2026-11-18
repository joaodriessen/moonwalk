import QtQuick 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

// GtkSpinButton: an entry with trailing minus and plus buttons.
T.SpinBox {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: 180
    implicitHeight: tokens.fieldHeight
    leftPadding: tokens.spacingM
    rightPadding: 2 * tokens.fieldHeight
    font.pixelSize: tokens.bodySize
    hoverEnabled: true
    editable: true
    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? control.tokens.text : control.tokens.disabledText
        selectionColor: control.tokens.accent
        selectedTextColor: control.tokens.accentInk
        verticalAlignment: TextInput.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        selectByMouse: true
    }
    background: Rectangle {
        radius: control.tokens.radiusControl
        color: control.tokens.control
        border.width: control.activeFocus ? control.tokens.focusWidth : 0
        border.color: control.tokens.focus
    }
    down.indicator: Item {
        x: control.width - 2 * width
        width: control.tokens.fieldHeight
        height: control.height
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: control.tokens.radiusControl - 2
            color: control.down.pressed ? control.tokens.controlPressed
                                        : (control.down.hovered ? control.tokens.controlHover : "transparent")
        }
        Text { anchors.centerIn: parent; text: "−"; font: control.font; color: control.tokens.text }
    }
    up.indicator: Item {
        x: control.width - width
        width: control.tokens.fieldHeight
        height: control.height
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: control.tokens.radiusControl - 2
            color: control.up.pressed ? control.tokens.controlPressed
                                      : (control.up.hovered ? control.tokens.controlHover : "transparent")
        }
        Text { anchors.centerIn: parent; text: "+"; font: control.font; color: control.tokens.text }
    }
}
