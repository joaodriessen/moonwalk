import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Controls.impl 2.15 as AppStyleIcon
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat
import "../../starlight/Lucide.js" as Lucide

// libadwaita image-button geometry with Lucide's line glyphs. Qt's theme
// lookup can select opaque full-color artwork at HiDPI, which becomes a solid
// block when recolored. Load these structural glyphs directly.
T.ToolButton {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    // "view-more" (kebab), "open-menu" (hamburger), or "plus".
    property string symbol: "view-more"
    property string accessibleName: ""
    property bool popupVisible: false
    readonly property GnomeTokens tokens: GnomeTokens {}

    readonly property var iconNames: ({
        "plus": "plus", "open-menu": "menu",
        "previous": "chevron-left", "close": "x",
        "search": "search", "edit": "pencil",
        "copy": "copy", "trash": "trash-2",
        "check": "check", "clear": "circle-x"
    })
    readonly property string iconName: iconNames[symbol] || "ellipsis-vertical"

    implicitWidth: 34
    implicitHeight: implicitWidth
    contentItem: AppStyleIcon.IconLabel {
        icon: root.icon
        display: AbstractButton.IconOnly
        color: root.icon.color
    }
    // Drawn black; icon.color recolours it on the CPU.
    icon.source: Lucide.url(iconName, "#000000")
    icon.width: 16
    icon.height: 16
    icon.color: enabled ? tokens.text : tokens.disabledText
    display: AbstractButton.IconOnly
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    function controllerActivate() { ControlCompat.activate(root) }

    // Flat header buttons have gentler states than regular filled buttons.
    // These currentColor mixes match native libadwaita MenuButton renders.
    function flatFill(amount) {
        return Qt.rgba(tokens.text.r, tokens.text.g, tokens.text.b,
                       tokens.text.a * amount)
    }

    background: Rectangle {
        radius: 9
        color: !root.enabled
               ? "transparent"
               : (root.down ? root.flatFill(0.16)
                  : (root.popupVisible ? root.flatFill(0.10)
                     : (root.hovered ? root.flatFill(0.07) : "transparent")))
        border.width: root.visualFocus ? root.tokens.focusWidth : 0
        border.color: root.tokens.focus

        Behavior on color {
            ColorAnimation { duration: root.tokens.motionDuration }
        }
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.pressed: pressed || popupVisible
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
