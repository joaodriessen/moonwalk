import QtQuick 2.15

// Accessible.id arrived after the Ubuntu 24.04 Qt compatibility floor. Keep
// objectName as the universal automation identity and mirror it into the
// attached accessibility ID whenever the running Qt exposes that property.
QtObject {
    id: identity

    property var target: null
    property string stableId: ""

    function synchronize() {
        if (!target) {
            return
        }

        var attached = target.Accessible
        if (attached && "id" in attached) {
            attached["id"] = stableId
        }
    }

    function scheduleSynchronization() {
        Qt.callLater(synchronize)
    }

    onTargetChanged: scheduleSynchronization()
    onStableIdChanged: scheduleSynchronization()
    Component.onCompleted: scheduleSynchronization()
}
