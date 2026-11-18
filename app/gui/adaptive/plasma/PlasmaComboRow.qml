import QtQuick 2.15

// Form-card row with a trailing native combo box. The embedded
// PlasmaDropdown is the semantic control; pages address it via
// comboObjectName and handle activated(index) on this row.
PlasmaFormRow {
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

    rowEnabled: comboControl.enabled

    PlasmaDropdown {
        id: comboControl
        accessibleName: row.title
        accessibleDescription: row.subtitle
        onActivated: function(index) { row.activated(index) }
    }
}
