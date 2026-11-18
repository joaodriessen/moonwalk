import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import "../settings" as Settings
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

// A sidebar host in the active style's ItemDelegate: OS glyph, name and status.
ItemDelegate {
    id: row

    property string name: ""
    property string statusText: ""
    property bool online: false
    // ready, active, attention, offline or checking
    property string tone: online ? "ready" : "offline"
    property string iconName: ""
    property string seed: ""
    property bool showStatus: statusText.length > 0
    property string accessibleName: statusText.length > 0 ? name + " · " + statusText : name
    property string accessibleDescription: ""

    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    function controllerActivate() { ControlCompat.activate(row) }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Settings.HostAvatar {
            Layout.alignment: Qt.AlignVCenter
            size: Kirigami.Units.iconSizes.medium
            seed: row.seed
            iconName: row.iconName.length > 0 ? row.iconName : "monitor"
            dimmed: row.tone === "offline"
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: row.name
                color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                visible: row.showStatus
                spacing: Kirigami.Units.smallSpacing
                PlasmaStatusDot { tone: row.tone; onHighlight: row.highlighted; implicitWidth: 7 }
                Label {
                    Layout.fillWidth: true
                    text: row.statusText
                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
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
