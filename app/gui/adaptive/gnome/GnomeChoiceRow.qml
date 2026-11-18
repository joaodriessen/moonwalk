import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// A boxed-list row whose control is an AdwToggleGroup. Beside the words when
// there is room for both, under them when there is not.
Item {
    id: row
    required property var setting
    property bool showSeparator: y > 0.5
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property bool stacked: width - 2 * tokens.spacingM - toggles.implicitWidth - tokens.spacingM < 180

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(tokens.rowMinHeight, layout.implicitHeight + 2 * tokens.spacingS)

    Rectangle {
        objectName: "gnome-group-separator"
        anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: row.tokens.spacingM; rightMargin: row.tokens.spacingM }
        height: 1
        z: 1
        color: row.tokens.cardShade
        visible: row.showSeparator
    }
    GridLayout {
        id: layout
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: row.tokens.spacingM; rightMargin: row.tokens.spacingM }
        columns: row.stacked ? 1 : 2
        columnSpacing: row.tokens.spacingM
        rowSpacing: row.tokens.spacingS
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: row.setting.title
                color: row.setting.sensitive ? row.tokens.text : row.tokens.disabledText
                font.pixelSize: row.tokens.bodySize
                wrapMode: Text.WordWrap
            }
            Label {
                objectName: "gnome-choice-subtitle"
                Layout.fillWidth: true
                visible: text.length > 0
                text: row.setting.subtitle
                color: row.setting.sensitive ? row.tokens.secondaryText : row.tokens.disabledText
                font.pixelSize: row.tokens.subtitleSize
                wrapMode: Text.WordWrap
            }
        }
        GnomeToggleGroup {
            id: toggles
            objectName: row.setting.controlId
            Layout.fillWidth: row.stacked
            Layout.alignment: Qt.AlignVCenter
            enabled: row.setting.sensitive
            model: row.setting.options
            textRole: row.setting.textRole
            currentIndex: row.setting.currentIndex
            displayText: row.setting.displayText
            accessibleName: row.setting.accessibleName
            accessibleDescription: row.setting.accessibleDescription
            onActivated: function(index) { row.setting.activated(index) }
        }
    }
}
