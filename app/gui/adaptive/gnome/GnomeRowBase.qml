import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Shared boxed-list row anatomy: title/subtitle column, trailing content,
// automatic top hairline for every row after the first visible one.
// Purely presentational; embedded controls own semantics and focus.
Item {
    id: row

    property string title: ""
    property string subtitle: ""
    property string warning: ""
    property string warningObjectName: ""
    property bool rowEnabled: true
    property alias hovered: hoverHandler.hovered
    property bool showHover: false
    // Set by whatever places the row: the first visible row of a list has none.
    property bool showSeparator: y > 0.5
    default property alias trailing: trailingHolder.data
    readonly property GnomeTokens tokens: GnomeTokens {}

    signal rowClicked()

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(tokens.rowMinHeight,
                             layout.implicitHeight + 2 * tokens.spacingS)

    Loader {
        anchors.fill: parent
        active: row.showHover && row.hovered && row.rowEnabled
        sourceComponent: GnomeRowFill {
            radius: row.tokens.radiusCard
            color: row.tokens.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 6 / 255, 0.04)
        }
    }

    Rectangle {
        objectName: "gnome-group-separator"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: row.tokens.spacingM
        anchors.rightMargin: row.tokens.spacingM
        height: 1
        z: 1
        // %boxed_list_row: border-bottom is --card-shade-color.
        color: row.tokens.cardShade
        visible: row.showSeparator
    }

    HoverHandler {
        id: hoverHandler
        enabled: row.showHover
    }

    TapHandler {
        enabled: row.showHover && row.rowEnabled
        onTapped: row.rowClicked()
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: row.tokens.spacingM
        anchors.rightMargin: row.tokens.spacingM
        anchors.topMargin: row.tokens.spacingS
        anchors.bottomMargin: row.tokens.spacingS
        spacing: row.tokens.spacingM

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Label {
                Layout.fillWidth: true
                visible: row.title.length > 0
                text: row.title
                color: row.rowEnabled
                       ? row.tokens.text : row.tokens.disabledText
                font.pixelSize: row.tokens.bodySize
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                color: row.rowEnabled
                       ? row.tokens.secondaryText : row.tokens.disabledText
                font.pixelSize: row.tokens.subtitleSize
                wrapMode: Text.WordWrap
            }
            Label {
                objectName: row.warningObjectName
                Layout.fillWidth: true
                visible: row.warning.length > 0
                text: row.warning
                // --warning-color: a consequence to know about, not an error.
                color: row.tokens.warning
                font.pixelSize: row.tokens.subtitleSize
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: trailingHolder
            Layout.alignment: Qt.AlignVCenter
            spacing: row.tokens.spacingS
        }
    }
}
