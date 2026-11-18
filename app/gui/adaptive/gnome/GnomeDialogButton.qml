import QtQuick 2.15

// Adwaita 1.9 alert-dialog response. Measured off a real AdwAlertDialog:
// responses are equal-width regular buttons in a 44px-tall response area,
// with the suggested one accent-filled -- not a flat hairline-separated bar,
// which is what this used to draw. _message-dialog.scss gives
// .response-area > button min-height 24, min-width 60, padding 10px 20px,
// border-radius 12. Semantics, accessibility and controllerActivate() come
// from GnomeButton.
GnomeButton {
    id: root

    topPadding: 10
    bottomPadding: 10
    leftPadding: 20
    rightPadding: 20
    responseRadius: 12

    background: Rectangle {
        radius: root.responseRadius
        color: {
            if (root.suggested)
                return root.down
                       ? root.tokens.accentPressed
                       : (root.hovered ? root.tokens.accentHover
                                       : root.tokens.accent)
            if (root.destructive)
                return root.down || root.hovered
                       ? root.tokens.destructiveHover
                       : root.tokens.destructive
            return root.down
                   ? root.tokens.controlPressed
                   : (root.hovered ? root.tokens.controlHover
                                   : root.tokens.control)
        }
        border.width: root.visualFocus ? root.tokens.focusWidth : 0
        border.color: root.tokens.focus

        Behavior on color {
            ColorAnimation { duration: root.tokens.motionDuration }
        }
    }

    contentItem: Text {
        text: root.text
        font.pixelSize: root.tokens.bodySize
        font.bold: true
        color: !root.enabled
               ? root.tokens.disabledText
               : (root.suggested || root.destructive
                  ? root.tokens.accentInk
                  : root.tokens.text)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation { duration: root.tokens.motionDuration }
        }
    }
}
