import QtQuick 2.15

// Soft rings spreading from behind a glyph: looking for something, or reaching
// for it. Purely decorative and motionless when the desktop asks for less motion.
Item {
    id: root
    property color color: "#3584e4"
    property bool running: true
    property bool reducedMotion: false
    property int duration: 2400
    // How far the rings spread, as a multiple of this item's size.
    property real spread: 1.7
    readonly property bool animating: running && visible && !reducedMotion

    Repeater {
        model: 2
        delegate: Rectangle {
            id: ring
            required property int index
            anchors.centerIn: parent
            width: root.width
            height: root.height
            radius: width / 2
            color: "transparent"
            border.color: root.color
            border.width: 2
            opacity: 0
            SequentialAnimation {
                running: root.animating
                loops: Animation.Infinite
                PauseAnimation { duration: ring.index * root.duration / 2 }
                ParallelAnimation {
                    NumberAnimation { target: ring; property: "scale"; from: 1; to: root.spread; duration: root.duration; easing.type: Easing.OutCubic }
                    NumberAnimation { target: ring; property: "opacity"; from: 0.45; to: 0; duration: root.duration; easing.type: Easing.OutCubic }
                }
                PauseAnimation { duration: (1 - ring.index) * root.duration / 2 }
            }
        }
    }
}
