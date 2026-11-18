import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Shared Plasma form-card row anatomy: title/subtitle column, trailing
// content, hairline separator above every row after the first visible one.
// Purely presentational; embedded controls own semantics and focus.
Control {
    id: row

    PlasmaPalette { id: livePalette }
    palette: livePalette.palette

    property string title: ""
    property string subtitle: ""
    property bool rowEnabled: true
    property bool showHover: false
    default property alias trailing: trailingHolder.data

    signal rowClicked()

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(44, layout.implicitHeight + 16)

    background: Item {
        Rectangle {
            anchors.fill: parent
            visible: row.showHover && rowHover.hovered && row.rowEnabled
            color: Qt.rgba(row.palette.text.r, row.palette.text.g,
                           row.palette.text.b, 0.06)
        }
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: row.palette.mid
            visible: row.parent
                     && row.parent.visibleChildren.length > 0
                     && row.parent.visibleChildren[0] !== row
        }
    }

    HoverHandler {
        id: rowHover
        enabled: row.showHover
    }

    TapHandler {
        enabled: row.showHover && row.rowEnabled
        onTapped: row.rowClicked()
    }

    contentItem: RowLayout {
        id: layout
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Label {
                Layout.fillWidth: true
                visible: row.title.length > 0
                text: row.title
                color: row.palette.windowText
                enabled: row.rowEnabled
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                color: row.palette.windowText
                enabled: row.rowEnabled
                opacity: 0.7
                font.pointSize: Math.max(6, Qt.application.font.pointSize - 1)
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: trailingHolder
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 12
            spacing: 8
        }
    }
}
