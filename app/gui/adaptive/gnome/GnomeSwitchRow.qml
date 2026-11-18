import QtQuick 2.15

// Boxed-list row with a trailing switch. The embedded GnomeToggle is the
// semantic control: pages set switchObjectName/accessibleName and handle
// toggled() exactly as they would on a bare toggle.
GnomeRowBase {
    id: row

    property alias checked: toggleControl.checked
    property alias switchEnabled: toggleControl.enabled
    property alias switchObjectName: toggleControl.objectName
    property alias accessibleName: toggleControl.accessibleName
    property alias accessibleDescription: toggleControl.accessibleDescription
    property alias toggleItem: toggleControl

    signal toggled()

    showHover: true
    rowEnabled: toggleControl.enabled
    onRowClicked: toggleControl.controllerActivate()

    GnomeToggle {
        id: toggleControl
        accessibleName: row.title
        accessibleDescription: row.subtitle
        onToggled: row.toggled()
    }
}
