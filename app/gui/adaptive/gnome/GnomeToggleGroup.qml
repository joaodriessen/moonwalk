import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle

// AdwToggleGroup: every option in view, the chosen one lifted out of a shared
// trough, its highlight travelling to the new choice. Arrow keys move the choice,
// as they do in any radio group, and Tab moves on to the next control.
T.Control {
    id: root
    property var model: []
    property string textRole: ""
    property int currentIndex: 0
    // What a combo showing this choice would say; survives an option a host no longer offers.
    property string displayText: ""
    property string accessibleName: ""
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property int count: model ? model.length : 0
    readonly property real cellWidth: count > 0 ? (availableWidth - (count - 1) * cells.spacing) / count : 0
    readonly property real widestOption: {
        var widest = 0
        for (var index = 0; index < repeater.count; ++index) {
            var item = repeater.itemAt(index)
            if (item) widest = Math.max(widest, item.implicitWidth)
        }
        return widest
    }
    signal activated(int index)

    function textFor(index) {
        var option = model[index]
        return textRole.length > 0 ? option[textRole] : option
    }
    function choose(index) {
        if (!enabled || index < 0 || index >= count) return
        if (index !== currentIndex)
            activated(index)
    }

    padding: 3
    focusPolicy: Qt.TabFocus
    implicitWidth: count * widestOption + (count - 1) * cells.spacing + leftPadding + rightPadding
    implicitHeight: 34 + topPadding + bottomPadding
    opacity: enabled ? 1 : 0.5

    background: Rectangle {
        radius: 9 + root.topPadding - 1
        color: root.tokens.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 6 / 255, 0.07)
        border.width: root.visualFocus ? root.tokens.focusWidth : (root.tokens.highContrast ? 1 : 0)
        border.color: root.visualFocus ? root.tokens.focus : root.tokens.border
    }

    Keys.onLeftPressed: choose(currentIndex + (mirrored ? 1 : -1))
    Keys.onRightPressed: choose(currentIndex + (mirrored ? -1 : 1))

    contentItem: Item {
        Rectangle {
            id: highlight
            objectName: "gnome-toggle-highlight"
            readonly property int shownIndex: Math.max(0, Math.min(root.currentIndex, root.count - 1))
            visible: root.count > 0 && root.currentIndex >= 0
            width: root.cellWidth
            height: parent.height
            x: root.mirrored ? parent.width - (shownIndex + 1) * width - shownIndex * cells.spacing
                             : shownIndex * (width + cells.spacing)
            radius: 8
            color: root.tokens.dark ? Qt.rgba(1, 1, 1, 0.16) : "#ffffff"
            border.width: root.tokens.highContrast ? 2 : 0
            border.color: root.tokens.text
            Behavior on x { NumberAnimation { duration: root.tokens.reducedMotion ? 0 : 180; easing.type: Easing.OutCubic } }

            // A hairline and a soft shadow lift it in light mode; dark mode relies on the fill.
            Rectangle {
                visible: !root.tokens.dark
                anchors.fill: parent
                anchors.topMargin: 1
                anchors.bottomMargin: -1
                z: -1
                radius: parent.radius
                color: Qt.rgba(0, 0, 6 / 255, 0.10)
            }
        }
        Row {
            id: cells
            anchors.fill: parent
            spacing: 2
            Repeater {
                id: repeater
                model: root.model
                delegate: T.AbstractButton {
                    id: cell
                    required property int index
                    readonly property bool chosen: index === root.currentIndex
                    width: root.cellWidth
                    height: cells.height
                    implicitWidth: label.implicitWidth + 24
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    checkable: false
                    onClicked: {
                        root.forceActiveFocus(Qt.MouseFocusReason)
                        root.choose(index)
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        visible: !cell.chosen && (cell.hovered || cell.down)
                        color: root.tokens.dark ? Qt.rgba(1, 1, 1, cell.down ? 0.08 : 0.04)
                                                : Qt.rgba(0, 0, 6 / 255, cell.down ? 0.08 : 0.04)
                    }
                    Label {
                        id: label
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width - 8)
                        text: root.textFor(cell.index)
                        color: root.tokens.text
                        font.pixelSize: root.tokens.bodySize
                        font.weight: cell.chosen ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: label.text
                    Accessible.checkable: true
                    Accessible.checked: chosen
                    Accessible.onPressAction: root.choose(index)
                }
            }
        }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled
    Accessible.focused: activeFocus

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
