import QtQuick 2.15
import QtQuick.Controls 2.15

// A key in a shortcut, drawn as AdwShortcutLabel draws it.
Rectangle {
    id: root
    property string text: ""
    readonly property GnomeTokens tokens: GnomeTokens {}
    implicitWidth: Math.max(implicitHeight, label.implicitWidth + 14)
    implicitHeight: label.implicitHeight + 8
    radius: 6
    color: tokens.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 6 / 255, 0.06)
    border.width: 1
    border.color: tokens.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 6 / 255, 0.10)
    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.tokens.text
        font.pixelSize: root.tokens.subtitleSize
        font.weight: Font.DemiBold
    }
}
