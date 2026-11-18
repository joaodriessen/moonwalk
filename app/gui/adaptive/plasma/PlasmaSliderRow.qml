import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Form-card row whose second line is a full-width native slider, with a
// trailing value label on the first line. The embedded PlasmaSlider is the
// semantic control addressed via sliderObjectName.
Control {
    id: row

    PlasmaPalette { id: livePalette }
    palette: livePalette.palette

    property string title: ""
    property string valueText: ""
    property alias from: sliderControl.from
    property alias to: sliderControl.to
    property alias stepSize: sliderControl.stepSize
    property alias value: sliderControl.value
    property alias sliderEnabled: sliderControl.enabled
    property alias sliderObjectName: sliderControl.objectName
    property alias accessibleDescription: sliderControl.accessibleDescription
    property alias accessibleName: sliderControl.accessibleName
    property alias sliderItem: sliderControl

    signal moved()

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(44, column.implicitHeight + 16)

    background: Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: row.palette.mid
        visible: row.parent
                 && row.parent.visibleChildren.length > 0
                 && row.parent.visibleChildren[0] !== row
    }

    contentItem: ColumnLayout {
        id: column
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 8
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: row.title
                color: row.palette.windowText
                enabled: sliderControl.enabled
                wrapMode: Text.WordWrap
            }
            Label {
                visible: row.valueText.length > 0
                text: row.valueText
                Layout.maximumWidth: row.width * 0.55
                wrapMode: Text.WordWrap
                color: row.palette.windowText
                enabled: sliderControl.enabled
                opacity: 0.7
                font.pointSize: Math.max(6, Qt.application.font.pointSize - 1)
            }
        }

        PlasmaSlider {
            id: sliderControl
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            accessibleName: row.title
            onMoved: row.moved()
        }
    }
}
