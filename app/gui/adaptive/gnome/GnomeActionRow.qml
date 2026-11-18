import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Layouts 1.15
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat
import "../../starlight/Lucide.js" as Lucide

// Activatable boxed-list row (Adwaita action-row anatomy). This row itself
// is the semantic Button control: pages give it the semantic objectName.
T.AbstractButton {
    id: row

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string title: text
    property string subtitle: ""
    property bool destructive: false
    property bool showChevron: false
    property string accessibleName: title
    property string accessibleDescription: subtitle
    readonly property GnomeTokens tokens: GnomeTokens {}

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(tokens.rowMinHeight,
                             layout.implicitHeight + 2 * tokens.spacingS)

    function controllerActivate() {
        ControlCompat.activate(row)
    }

    property bool showSeparator: y > 0.5
    background: Item {
        Loader {
            anchors.fill: parent
            active: row.enabled && (row.hovered || row.down)
            sourceComponent: GnomeRowFill {
                radius: row.tokens.radiusCard
                color: row.down
                       ? (row.tokens.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 6 / 255, 0.08))
                       : (row.tokens.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 6 / 255, 0.04))
            }
        }
        Rectangle {
            objectName: "gnome-group-separator"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: row.tokens.spacingM
            anchors.rightMargin: row.tokens.spacingM
            height: 1
            color: row.tokens.cardShade
            visible: row.showSeparator
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            radius: Math.max(0, row.tokens.radiusCard - 2)
            border.width: row.visualFocus ? row.tokens.focusWidth : 0
            border.color: row.tokens.focus
        }
    }

    contentItem: RowLayout {
        id: layout
        spacing: row.tokens.spacingM

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: row.tokens.spacingM
            Layout.topMargin: row.tokens.spacingS
            Layout.bottomMargin: row.tokens.spacingS
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: row.title
                color: !row.enabled
                       ? row.tokens.disabledText
                       : (row.destructive
                          ? row.tokens.destructiveText : row.tokens.text)
                font.pixelSize: row.tokens.bodySize
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                color: row.enabled
                       ? row.tokens.secondaryText : row.tokens.disabledText
                font.pixelSize: row.tokens.subtitleSize
                wrapMode: Text.WordWrap
            }
        }

        IconImage {
            visible: row.showChevron
            Layout.rightMargin: row.tokens.spacingM
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            sourceSize: Qt.size(16, 16)
            source: Lucide.url("chevron-right", "#000000")
            color: row.tokens.secondaryText
        }
        Item {
            visible: !row.showChevron
            Layout.rightMargin: row.tokens.spacingM
            implicitWidth: 1
            implicitHeight: 1
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
