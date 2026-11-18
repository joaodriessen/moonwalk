import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Layouts 1.15
import "../settings" as Settings
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

// GNOME sidebar row: the host's tile, name, status line with a colored dot.
// Selection is the Adwaita 10%-shade rounded fill, never a solid accent bar.
T.AbstractButton {
    id: row

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string name: ""
    property string statusText: ""
    property bool online: false
    // ready, active, attention, offline or checking
    property string tone: online ? "ready" : "offline"
    property bool selected: false
    property string iconName: ""
    property string seed: ""
    property bool showGlyph: true
    property bool showStatus: statusText.length > 0
    property bool hasOpenPopup: false
    property string accessibleName: showStatus && statusText.length > 0
                                    ? name + " · " + statusText : name
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    // .navigation-sidebar > row: padding 0 8px, min-height 36 (_sidebars.scss).
    leftPadding: 8
    rightPadding: 8
    implicitHeight: Math.max(36, layout.implicitHeight + 10)

    function controllerActivate() {
        ControlCompat.activate(row)
    }

    background: Rectangle {
        radius: 9
        // currentColor state mixes (_colors.scss): hover 7%, active 16%,
        // selected 10%, selected+hover 13%, selected+active 19%. Selection
        // and pointer state combine; neither wins outright.
        readonly property real stateAlpha: {
            var selected = row.selected || row.hasOpenPopup
            if (!row.enabled)
                return selected ? 0.10 : 0
            if (selected)
                return row.down ? 0.19 : (row.hovered ? 0.13 : 0.10)
            return row.down ? 0.16 : (row.hovered ? 0.07 : 0)
        }
        color: row.tokens.dark ? Qt.rgba(1, 1, 1, stateAlpha)
                               : Qt.rgba(0, 0, 6 / 255, stateAlpha)
        border.width: row.visualFocus ? row.tokens.focusWidth : 0
        border.color: row.tokens.focus

        Behavior on color {
            ColorAnimation { duration: row.tokens.motionDuration }
        }
    }

    contentItem: RowLayout {
        id: layout
        spacing: 10

        Settings.HostAvatar {
            visible: row.showGlyph
            Layout.alignment: Qt.AlignVCenter
            size: 32
            seed: row.seed
            iconName: row.iconName.length > 0 ? row.iconName : "monitor"
            dimmed: row.tone === "offline"
            highContrast: row.tokens.highContrast
            schemeDark: row.tokens.dark
            outline: row.tokens.text
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 3
            Layout.bottomMargin: 3
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: row.name
                color: row.enabled ? row.tokens.text
                                   : row.tokens.disabledText
                font.pixelSize: row.tokens.bodySize
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                visible: row.showStatus
                spacing: 5

                GnomeStatusDot { tone: row.tone; implicitWidth: 7 }
                Label {
                    Layout.fillWidth: true
                    text: row.statusText
                    color: row.tokens.secondaryText
                    font.pixelSize: row.tokens.captionSize
                    elide: Text.ElideRight
                }
            }
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
        target: row
        stableId: row.objectName
    }
}
