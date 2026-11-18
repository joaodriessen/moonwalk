import QtQml 2.15

QtObject {
    default property list<SettingRow> rows
    property string groupId: ""
    property string title: ""
    property string description: ""
    property bool shown: true
    property bool filteredOut: false
}
