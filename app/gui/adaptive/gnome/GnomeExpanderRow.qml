import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat
import "../../starlight/Lucide.js" as Lucide

// AdwExpanderRow: an activatable header whose chevron turns as a nested list
// opens beneath it. Nested controls exist while closed, but are not visible.
Column {
    id: root
    required property var row
    property bool showSeparator: false
    readonly property GnomeTokens tokens: GnomeTokens {}
    width: parent ? parent.width : implicitWidth

    AbstractButton {
        id: header
        objectName: root.row.controlId
        width: parent.width
        implicitHeight: Math.max(root.tokens.rowMinHeight, layout.implicitHeight + 2 * root.tokens.spacingS)
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        onClicked: root.row.expanded = !root.row.expanded

        function controllerActivate() { ControlCompat.activate(header) }

        background: Item {
            Loader {
                anchors.fill: parent
                active: header.hovered || header.down
                sourceComponent: GnomeRowFill {
                    radius: root.tokens.radiusCard
                    color: root.tokens.dark ? Qt.rgba(1, 1, 1, header.down ? 0.08 : 0.04)
                                            : Qt.rgba(0, 0, 6 / 255, header.down ? 0.08 : 0.04)
                }
            }
            Rectangle {
                objectName: "gnome-group-separator"
                anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: root.tokens.spacingM; rightMargin: root.tokens.spacingM }
                height: 1
                color: root.tokens.cardShade
                visible: root.showSeparator
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: "transparent"
                radius: Math.max(0, root.tokens.radiusCard - 2)
                border.width: header.visualFocus ? root.tokens.focusWidth : 0
                border.color: root.tokens.focus
            }
        }
        contentItem: RowLayout {
            id: layout
            spacing: root.tokens.spacingM
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.tokens.spacingM
                Layout.alignment: Qt.AlignVCenter
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: root.row.title
                    color: root.tokens.text
                    font.pixelSize: root.tokens.bodySize
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.row.subtitle
                    color: root.tokens.secondaryText
                    font.pixelSize: root.tokens.subtitleSize
                    wrapMode: Text.WordWrap
                }
            }
            IconImage {
                Layout.rightMargin: root.tokens.spacingM
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                sourceSize: Qt.size(16, 16)
                source: Lucide.url("chevron-down", "#000000")
                color: root.tokens.secondaryText
                rotation: root.row.expanded ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: root.tokens.motionDuration; easing.type: Easing.OutCubic } }
            }
        }
        Accessible.role: Accessible.Button
        Accessible.name: root.row.accessibleName
        Accessible.description: root.row.accessibleDescription
        Accessible.checkable: true
        Accessible.checked: root.row.expanded
        Accessible.onPressAction: controllerActivate()
        AppStyle.AccessibleIdentity { target: header; stableId: header.objectName }
    }
    Column {
        id: nested
        objectName: "gnome-expander-rows"
        width: parent.width
        visible: root.row.expanded
        Repeater {
            model: root.row.rows
            // By URL: GnomeSettingRow is what instantiates this expander.
            delegate: Loader {
                required property var modelData
                width: parent ? parent.width : implicitWidth
                visible: modelData.shown === true
                Component.onCompleted: setSource(Qt.resolvedUrl("GnomeSettingRow.qml"),
                                                 { row: modelData, alwaysSeparated: true })
            }
        }
    }
}
