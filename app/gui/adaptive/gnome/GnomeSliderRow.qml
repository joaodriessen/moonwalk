import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Boxed-list row whose second line is a full-width slider, with a trailing
// value label on the first line. The embedded GnomeSlider is the semantic
// control addressed via sliderObjectName.
Item {
    id: row

    property string title: ""
    property string valueText: ""
    property string warning: ""
    property string warningObjectName: ""
    property alias from: sliderControl.from
    property alias to: sliderControl.to
    property alias stepSize: sliderControl.stepSize
    // Dragging self-writes the slider's `value`, which would permanently
    // break a declarative binding -- the same hazard production PcView
    // documents. Callers bind boundValue instead; the row syncs imperatively
    // and skips mid-drag so the handle doesn't fight the user's hand.
    property real boundValue: 0
    readonly property alias value: sliderControl.value
    property alias sliderEnabled: sliderControl.enabled
    property alias sliderObjectName: sliderControl.objectName
    property alias accessibleDescription: sliderControl.accessibleDescription
    property alias accessibleName: sliderControl.accessibleName
    property alias sliderItem: sliderControl
    readonly property GnomeTokens tokens: GnomeTokens {}

    signal moved()

    onBoundValueChanged: if (!sliderControl.pressed) sliderControl.value = boundValue
    Component.onCompleted: sliderControl.value = boundValue

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(tokens.rowMinHeight,
                             column.implicitHeight + 2 * tokens.spacingS)

    property bool showSeparator: y > 0.5
    Rectangle {
        objectName: "gnome-group-separator"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: row.tokens.spacingM
        anchors.rightMargin: row.tokens.spacingM
        height: 1
        z: 1
        color: row.tokens.cardShade
        visible: row.showSeparator
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.leftMargin: row.tokens.spacingM
        anchors.rightMargin: row.tokens.spacingM
        anchors.topMargin: row.tokens.spacingS
        anchors.bottomMargin: row.tokens.spacingS
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: row.tokens.spacingM

            Label {
                Layout.fillWidth: true
                text: row.title
                color: sliderControl.enabled
                       ? row.tokens.text : row.tokens.disabledText
                font.pixelSize: row.tokens.bodySize
                wrapMode: Text.WordWrap
            }
            Label {
                visible: row.valueText.length > 0
                text: row.valueText
                Layout.maximumWidth: row.width * 0.55
                wrapMode: Text.WordWrap
                color: sliderControl.enabled
                       ? row.tokens.secondaryText : row.tokens.disabledText
                font.pixelSize: row.tokens.subtitleSize
            }
        }

        GnomeSlider {
            id: sliderControl
            Layout.fillWidth: true
            accessibleName: row.title
            onMoved: row.moved()
        }
        Label {
            objectName: row.warningObjectName
            Layout.fillWidth: true
            visible: row.warning.length > 0
            text: row.warning
            color: row.tokens.warning
            font.pixelSize: row.tokens.subtitleSize
            wrapMode: Text.WordWrap
        }
    }
}
