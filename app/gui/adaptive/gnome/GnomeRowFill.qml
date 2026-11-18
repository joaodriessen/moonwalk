import QtQuick 2.15

// A row's state fill. Hovering the first or last row of a boxed list follows
// the card's corners instead of poking square corners out of it (per-corner
// radii are Qt 6.7+, absent on the floor, so the inner corners are patched).
Item {
    id: fill
    property color color: "transparent"
    property int radius: 12
    readonly property Item row: parent
    readonly property bool first: row && row.y <= 0.5
    readonly property bool last: row && row.parent && row.y + row.height >= row.parent.height - 0.5
    anchors.fill: parent

    Rectangle { anchors.fill: parent; radius: fill.first || fill.last ? fill.radius : 0; color: fill.color }
    Rectangle {
        visible: fill.first && !fill.last
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.min(parent.height, fill.radius)
        color: fill.color
    }
    Rectangle {
        visible: fill.last && !fill.first
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Math.min(parent.height, fill.radius)
        color: fill.color
    }
}
