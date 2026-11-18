import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../starlight" as AppStyle
import "../../starlight/ControlCompat.js" as ControlCompat

// Activatable form-card row (Plasma 6 FormButtonDelegate anatomy). This row
// itself is the semantic Button control carrying the semantic objectName.
AbstractButton {
    id: row

    PlasmaPalette { id: livePalette }
    palette: livePalette.palette

    property string title: text
    property string subtitle: ""
    property bool destructive: false
    property bool showChevron: false
    property string chevronGlyph: "›"
    property string accessibleName: title
    property string accessibleDescription: subtitle

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(44, layout.implicitHeight + 16)

    function controllerActivate() {
        ControlCompat.activate(row)
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            visible: row.enabled && (row.hovered || row.down)
            color: Qt.rgba(row.palette.highlight.r, row.palette.highlight.g,
                           row.palette.highlight.b, row.down ? 0.3 : 0.15)
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
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            radius: 4
            border.width: row.visualFocus ? 2 : 0
            border.color: row.palette.highlight
        }
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
                objectName: "plasmaButtonRowTitle"
                Layout.fillWidth: true
                text: row.title
                color: !row.enabled ? row.palette.disabled.text
                                    : (row.destructive
                                       ? row.palette.highlight
                                       : row.palette.text)
                enabled: row.enabled
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                color: row.enabled ? row.palette.text
                                   : row.palette.disabled.text
                enabled: row.enabled
                opacity: row.enabled ? 0.7 : 1.0
                font.pointSize: Math.max(6, Qt.application.font.pointSize - 1)
                wrapMode: Text.WordWrap
            }
        }

        Label {
            visible: row.showChevron
            Layout.rightMargin: 12
            text: row.chevronGlyph
            color: row.enabled ? row.palette.text
                               : row.palette.disabled.text
            opacity: row.enabled ? 0.6 : 1.0
            font.pointSize: Qt.application.font.pointSize + 2
        }
        Item {
            visible: !row.showChevron
            Layout.rightMargin: 12
            implicitWidth: 1
            implicitHeight: 1
        }
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: row
        stableId: row.objectName
    }
}
