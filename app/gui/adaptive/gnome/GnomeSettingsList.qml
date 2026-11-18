import QtQuick 2.15
import QtQuick.Layouts 1.15

// Every shared SettingGroup as an AdwPreferencesGroup, in the model's order.
ColumnLayout {
    id: root
    required property var groups
    readonly property GnomeTokens tokens: GnomeTokens {}
    spacing: tokens.spacingXL

    Repeater {
        model: root.groups
        delegate: GnomeGroup {
            required property var modelData
            objectName: "gnome-settings-group-" + modelData.groupId
            Layout.fillWidth: true
            visible: modelData.shown && !modelData.filteredOut
            title: modelData.title
            description: modelData.description
            Repeater {
                model: parent ? modelData.rows : null
                delegate: GnomeSettingRow {
                    required property var modelData
                    row: modelData
                }
            }
        }
    }
}
