import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "adaptive/settings" as Settings

// A full-window status page for connecting, quitting and recovering. Plain
// Controls, so the active style makes it an AdwStatusPage or a Kirigami message.
ColumnLayout {
    id: root
    // What is being reached for, and the tile that stands for it.
    property string heading: ""
    property string hostSeed: ""
    property string hostIcon: ""
    property bool hostDimmed: false
    property string title: ""
    property string message: ""
    property string hint: ""
    property bool busy: false
    // 0 to 1 when the steps can be counted; negative when they cannot.
    property real progress: -1
    default property alias actions: actionFlow.data
    // Quieter actions, on their own line under the main ones.
    property alias secondaryActions: secondaryRow.data
    spacing: 16

    Label { id: metrics; visible: false; text: " " }

    Item {
        objectName: "starlight.status.identity"
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 8
        visible: root.hostSeed.length > 0
        implicitWidth: 88
        implicitHeight: 88
        Settings.PulseRings {
            anchors.fill: parent
            running: root.busy
            visible: root.busy
            color: root.palette.highlight
            reducedMotion: typeof Appearance !== "undefined" && Appearance.reducedMotion === true
        }
        Settings.HostAvatar {
            anchors.fill: parent
            size: 88
            seed: root.hostSeed
            iconName: root.hostIcon.length > 0 ? root.hostIcon : "monitor"
            dimmed: root.hostDimmed
        }
    }
    BusyIndicator {
        objectName: "starlight.status.spinner"
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 40
        implicitHeight: 40
        visible: root.busy && root.hostSeed.length === 0
        running: visible
    }
    Label {
        objectName: "starlight.status.heading"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.heading
        font.pixelSize: Math.round(metrics.fontInfo.pixelSize * 1.6)
        font.bold: true
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }
    Label {
        objectName: "starlight.status.title"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.title
        font.pixelSize: root.heading.length > 0 ? metrics.fontInfo.pixelSize
                                                : Math.round(metrics.fontInfo.pixelSize * 1.6)
        font.bold: root.heading.length === 0
        opacity: root.heading.length > 0 ? 0.75 : 1
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Accessible.role: root.heading.length > 0 ? Accessible.StaticText : Accessible.Heading
        Accessible.name: text
    }
    ProgressBar {
        objectName: "starlight.status.progress"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: Math.min(root.width, 240)
        visible: root.busy && root.progress >= 0
        from: 0
        to: 1
        value: Math.max(0, root.progress)
        Behavior on value { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
        Accessible.name: root.title
    }
    ScrollView {
        Layout.fillWidth: true
        visible: root.message.length > 0
        implicitHeight: Math.min(messageLabel.implicitHeight, 240)
        contentWidth: availableWidth
        Label {
            id: messageLabel
            objectName: "starlight.status.message"
            width: parent.width
            text: root.message
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Accessible.name: text
        }
    }
    Flow {
        id: actionFlow
        Layout.fillWidth: true
        Layout.topMargin: 8
        spacing: 8
        readonly property real actionsWidth: {
            var total = -spacing
            for (var index = 0; index < visibleChildren.length; ++index)
                total += visibleChildren[index].implicitWidth + spacing
            return total
        }
        // Centred while the actions fit on one line, then left to wrap.
        leftPadding: Math.max(0, (width - actionsWidth) / 2)
    }
    Row {
        id: secondaryRow
        Layout.alignment: Qt.AlignHCenter
    }
    Label {
        objectName: "starlight.status.hint"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.hint
        opacity: 0.7
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }
}
