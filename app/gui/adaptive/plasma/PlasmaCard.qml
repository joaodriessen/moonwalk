import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Plasma 6 form-card anatomy: bold section header above a view-background
// card with hairline border, rows separated by hairlines. Colors come from
// the active style's resolved palette so Breeze color schemes apply.
ColumnLayout {
    id: card

    property string title: ""
    default property alias rows: rowsColumn.data

    spacing: 6

    PlasmaPalette {
        id: livePalette
    }

    Label {
        Layout.fillWidth: true
        visible: card.title.length > 0
        text: card.title
        color: livePalette.palette.windowText
        font.bold: true
        wrapMode: Text.WordWrap
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Rectangle {
        objectName: "plasmaCardGround"
        Layout.fillWidth: true
        implicitHeight: rowsColumn.implicitHeight
        radius: 5
        color: livePalette.palette.base
        border.width: 1
        border.color: livePalette.palette.mid
        clip: true

        Column {
            id: rowsColumn
            width: parent.width
        }
    }
}
