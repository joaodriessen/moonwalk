import QtQuick 2.15
import QtQuick.Layouts 1.15

// Boxed-list row with a trailing flat dropdown (Adwaita combo-row anatomy).
// The embedded GnomeDropdown is the semantic control; pages address it via
// comboObjectName and handle activated(index) on this row.
GnomeRowBase {
    id: row

    property alias model: comboControl.model
    property alias textRole: comboControl.textRole
    property alias displayText: comboControl.displayText
    property alias currentIndex: comboControl.currentIndex
    property alias comboEnabled: comboControl.enabled
    property alias comboObjectName: comboControl.objectName
    property alias accessibleName: comboControl.accessibleName
    property alias accessibleDescription: comboControl.accessibleDescription
    property alias comboItem: comboControl

    signal activated(int index)

    showHover: true
    rowEnabled: comboControl.enabled
    onRowClicked: comboControl.controllerActivate()

    GnomeDropdown {
        id: comboControl
        flatStyle: true
        // A long current value (e.g. "Borderless windowed (Recommended)")
        // elides instead of crushing the title/subtitle column.
        Layout.maximumWidth: Math.ceil(row.width * 0.5)
        accessibleName: row.title
        accessibleDescription: row.subtitle
        onActivated: function(index) { row.activated(index) }
    }
}
