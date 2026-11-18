import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

T.Switch {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: text
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property int motionDuration: tokens.motionDuration

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    spacing: tokens.spacingS
    padding: 0
    leftPadding: mirrored || text.length === 0
                 ? padding
                 : indicator.implicitWidth + spacing + padding
    rightPadding: mirrored && text.length > 0
                  ? indicator.implicitWidth + spacing + padding
                  : padding
    implicitWidth: indicator.implicitWidth
                   + (text.length > 0
                      ? spacing + contentItem.implicitWidth : 0)
                   + leftPadding + rightPadding
    implicitHeight: Math.max(indicator.implicitHeight,
                             contentItem.implicitHeight)
                    + topPadding + bottomPadding
    font.pixelSize: tokens.bodySize

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

    contentItem: Text {
        visible: root.text.length > 0
        text: root.text
        font: root.font
        color: root.enabled
               ? root.tokens.text
               : root.tokens.disabledText
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: root.mirrored
                             ? Text.AlignRight : Text.AlignLeft
        elide: Text.ElideRight
    }

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 26
        x: root.text.length === 0
           ? Math.round((root.width - width) / 2)
           : (root.mirrored ? root.width - width - root.padding : root.padding)
        y: Math.round((root.height - height) / 2)
        radius: height / 2
        // Adwaita switch track: unchecked currentColor 15/20/25 (rest/
        // hover/active), checked accent-bg.
        color: root.checked
               ? (root.enabled ? root.tokens.accent
                               : Qt.rgba(root.tokens.accent.r,
                                         root.tokens.accent.g,
                                         root.tokens.accent.b, 0.5))
               : (root.tokens.dark
                  ? Qt.rgba(1, 1, 1, root.down ? 0.25 : (root.hovered ? 0.20 : 0.15))
                  : Qt.rgba(0, 0, 6 / 255, root.down ? 0.25 : (root.hovered ? 0.20 : 0.15)))
        border.width: root.visualFocus
                      ? root.tokens.focusWidth
                      : (root.tokens.highContrast
                         ? root.tokens.borderWidth : 0)
        border.color: root.visualFocus
                      ? root.tokens.focus
                      : root.tokens.border

        Behavior on color {
            ColorAnimation { duration: root.motionDuration }
        }

        // Adwaita knob: white-80%-over-view fill with a soft 0 2px 4px
        // black/20% shadow.
        Item {
            width: parent.height - 6
            height: width
            x: 3 + root.visualPosition * (parent.width - width - 6)
            y: 3

            Behavior on x {
                NumberAnimation {
                    duration: root.motionDuration
                    easing.type: Easing.OutCubic
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 1
                radius: width / 2
                color: Qt.rgba(0, 0, 6 / 255, 0.18)
                z: -1
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: root.tokens.dark ? "#f7f7f8" : "#ffffff"
            }
        }
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
