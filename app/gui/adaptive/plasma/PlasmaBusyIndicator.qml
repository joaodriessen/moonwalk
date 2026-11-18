import QtQuick 2.15
import QtQuick.Controls 2.15
import "../../starlight" as AppStyle

BusyIndicator {
    id: root

    property string accessibleName: qsTr("Busy")
    property string accessibleDescription: ""
    readonly property bool busyState: running

    Accessible.role: Accessible.Animation
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription.length > 0
                            ? accessibleDescription
                            : (running ? qsTr("In progress") : qsTr("Idle"))

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
