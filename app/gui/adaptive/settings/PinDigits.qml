import QtQuick 2.15
import QtQuick.Controls 2.15

// A pairing PIN as separate digits, so it can be read across a room and typed
// one key at a time. Each desktop supplies its own colours and fonts.
Row {
    id: root
    property string pin: ""
    property color surface: "#ffffff"
    property color ink: "#000000"
    property color outline: "transparent"
    property int cellWidth: 56
    property int cellHeight: 68
    property int cellRadius: 12
    property int digitSize: 36
    spacing: 8
    Accessible.role: Accessible.StaticText
    Accessible.name: pin.split("").join(" ")

    Repeater {
        model: root.pin.split("")
        delegate: Rectangle {
            required property string modelData
            width: root.cellWidth
            height: root.cellHeight
            radius: root.cellRadius
            color: root.surface
            border.width: root.outline.a > 0 ? 1 : 0
            border.color: root.outline
            Label {
                anchors.centerIn: parent
                text: modelData
                color: root.ink
                font.pixelSize: root.digitSize
                font.weight: Font.Bold
            }
        }
    }
}
