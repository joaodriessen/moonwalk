import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// AdwPreferencesGroup: a heading and optional description above a 12px-radius
// card whose rows separate with hairlines. Rows draw their own top hairline
// from their position, so hidden rows and generated rows both stay correct.
ColumnLayout {
    id: group

    property string title: ""
    property string description: ""
    readonly property GnomeTokens tokens: GnomeTokens {}
    default property alias rows: rowsColumn.data

    spacing: tokens.spacingS

    ColumnLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 2
        spacing: 2
        visible: group.title.length > 0 || group.description.length > 0

        Label {
            Layout.fillWidth: true
            visible: group.title.length > 0
            text: group.title
            color: group.tokens.text
            font.pixelSize: group.tokens.bodySize
            font.bold: true
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        Label {
            objectName: "gnome-group-description"
            Layout.fillWidth: true
            visible: group.description.length > 0
            text: group.description
            color: group.tokens.secondaryText
            font.pixelSize: group.tokens.subtitleSize
            wrapMode: Text.WordWrap
            Accessible.name: text
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight

        // .card's own three-layer shadow, cropped from a native GTK render
        // (scripts/design/gnome-shadows.py). Its first layer is the 1px ring,
        // so the card needs no border of its own; both offsets point down, so
        // the shadow reaches 4px above the card and 8px below.
        BorderImage {
            objectName: "gnome-group-card-shadow"
            anchors.fill: card
            anchors.leftMargin: -6
            anchors.rightMargin: -6
            anchors.topMargin: -4
            anchors.bottomMargin: -8
            source: "qrc:/res/card_shadow.png"
            border { left: 22; right: 22; top: 22; bottom: 22 }
        }

        Rectangle {
            id: card
            objectName: "gnome-group-card"
            width: parent.width
            implicitHeight: rowsColumn.implicitHeight
            color: group.tokens.card
            radius: group.tokens.radiusCard
            // Only the raised-contrast rule swaps the shadow's 3% ring for a
            // real border; ordinarily a card has none.
            border.width: group.tokens.highContrast ? 1 : 0
            border.color: group.tokens.cardBorder
            readonly property color cardColor: color
            // The card sits on the page ground, so that is what it has to
            // stand apart from.
            readonly property color parentColor: group.tokens.window
            readonly property color outlineColor: group.tokens.cardRing
            readonly property color elevationColor: group.tokens.cardShadow

            Column {
                id: rowsColumn
                objectName: "gnome-group-rows"
                width: parent.width
            }
        }
    }
}
