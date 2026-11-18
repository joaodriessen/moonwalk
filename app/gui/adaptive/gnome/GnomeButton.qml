import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

T.Button {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: text
    property string accessibleDescription: ""
    property bool suggested: false
    property bool destructive: false
    property bool flatStyle: false
    property bool linkStyle: false
    property bool pill: false
    // Alert responses use their own 12px radius; everything else follows the
    // 9px button radius or the pill.
    property int responseRadius: 0
    readonly property GnomeTokens tokens: GnomeTokens {}

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    // Adwaita button metrics: padding 5px 10px, pill 10px 32px.
    leftPadding: linkStyle
                 ? tokens.spacingXS
                 : (pill ? 32 : 10)
    rightPadding: leftPadding
    topPadding: linkStyle
                ? 2
                : (pill ? 10 : 5)
    bottomPadding: topPadding
    implicitWidth: Math.max(flatStyle || linkStyle
                            ? 0
                            : (responseRadius > 0
                               ? 60 : tokens.buttonMinContentWidth),
                            contentItem.implicitWidth)
                   + leftPadding + rightPadding
    implicitHeight: linkStyle
                    ? contentItem.implicitHeight + topPadding + bottomPadding
                    : Math.max(tokens.buttonMinContentHeight,
                               contentItem.implicitHeight)
                      + topPadding + bottomPadding
    font.pixelSize: tokens.bodySize
    // Adwaita buttons are bold; link/flat text stays regular weight.
    font.bold: !linkStyle && !flatStyle

    function controllerActivate() {
        ControlCompat.activate(root)
    }

    readonly property color ink: !root.enabled
           ? root.tokens.disabledText
           : (root.suggested
              ? root.tokens.accentInk
              : (root.destructive
                 ? "#ffffff"
                 : (root.linkStyle
                    ? (root.hovered ? root.tokens.text
                                    : root.tokens.secondaryText)
                    : root.tokens.text)))
    // A glyph is monochrome, so it takes the label's colour unless told otherwise.
    icon.color: ink

    contentItem: IconLabel {
        spacing: root.tokens.spacingS
        mirrored: root.mirrored
        display: root.display
        icon: root.icon
        text: root.text
        font: root.font
        color: root.ink
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: root.tokens.controlHeight
        radius: root.pill ? root.tokens.radiusPill : 9
        color: {
            if (root.suggested)
                return root.down
                       ? root.tokens.accentPressed
                       : (root.hovered
                          ? root.tokens.accentHover
                          : root.tokens.accent)
            if (root.destructive)
                return root.down || root.hovered
                       ? root.tokens.destructiveHover
                       : root.tokens.destructive
            if (root.linkStyle && !root.visualFocus)
                return "transparent"
            if (root.flatStyle && !root.down && !root.hovered
                    && !root.visualFocus)
                return "transparent"
            return root.down
                   ? root.tokens.controlPressed
                   : (root.hovered
                      ? root.tokens.controlHover
                      : root.tokens.control)
        }
        border.width: root.visualFocus
                      ? root.tokens.focusWidth
                      : (root.tokens.highContrast ? 2 : 0)
        border.color: root.visualFocus
                      ? root.tokens.focus
                      : root.tokens.border

        Behavior on color {
            ColorAnimation { duration: root.tokens.motionDuration }
        }
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
