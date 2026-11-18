import QtQuick 2.9
import Starlight 1.0

// Grouped-form card: a hairline-bordered, rounded container that stacks its
// children (SLRow instances) vertically, per the mockup's .group.
Rectangle {
    id: root

    default property alias content: col.data

    color: Theme.card
    border.width: 1
    border.color: Theme.hairline
    radius: Theme.radiusGroup

    implicitHeight: col.implicitHeight

    Column {
        id: col
        width: parent.width
    }
}
