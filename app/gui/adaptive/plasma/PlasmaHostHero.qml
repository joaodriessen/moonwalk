import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import "../settings" as Settings

// The top of a host's page, as a Breeze card: who it is, whether it can be
// reached, the one thing to do next, and what the next connection will be.
Item {
    id: root
    required property var page
    required property var presentation
    property alias hostMenuButton: hostMenuButton
    readonly property var selected: page.selectedHostSnapshot || ({})
    readonly property bool compact: width < Kirigami.Units.gridUnit * 24
    readonly property bool highContrast: typeof Appearance !== "undefined" && Appearance.highContrast === true
    signal aboutToAct(Item source)
    signal hostMenuRequested(Item source)

    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: content.implicitHeight + Kirigami.Units.gridUnit * 2
        radius: Kirigami.Units.cornerRadius
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Kirigami.Units.gridUnit }
            spacing: Kirigami.Units.largeSpacing * 2

            GridLayout {
                objectName: "plasma-host-identity"
                Layout.fillWidth: true
                columns: root.compact ? 1 : 3
                columnSpacing: Kirigami.Units.largeSpacing * 2
                rowSpacing: Kirigami.Units.largeSpacing
                Settings.HostAvatar {
                    Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignVCenter
                    size: Kirigami.Units.gridUnit * 3
                    seed: root.selected.uuid || ""
                    iconName: root.presentation.iconName
                    dimmed: !root.presentation.online
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0
                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 1
                        text: root.selected.name || ""
                        wrapMode: Text.Wrap
                        horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignLeft
                        spacing: Kirigami.Units.smallSpacing
                        PlasmaStatusDot {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: Math.max(0, (statusLine.lineHeightPixels - height) / 2)
                            tone: root.presentation.statusTone
                        }
                        Label {
                            id: statusLine
                            readonly property real lineHeightPixels: fontMetrics.height
                            FontMetrics { id: fontMetrics; font: statusLine.font }
                            objectName: "plasma-host-status-line"
                            Layout.fillWidth: !root.compact
                            text: root.presentation.statusLine
                            opacity: 0.75
                            wrapMode: Text.Wrap
                            Accessible.name: text
                        }
                    }
                }
                RowLayout {
                    Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignVCenter
                    spacing: Kirigami.Units.smallSpacing
                    PlasmaButton {
                        objectName: "starlight.hosts.connect"
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                        visible: root.presentation.connectVisible
                        enabled: root.presentation.connectEnabled
                        highlighted: true
                        icon.name: "lucide-play"
                        text: root.presentation.connectLabel
                        accessibleName: text
                        accessibleDescription: root.presentation.runningNote
                        onClicked: {
                            root.aboutToAct(this)
                            root.page.requestConnect(root.page.selectedHostUuid)
                        }
                    }
                    PlasmaButton {
                        objectName: "starlight.hosts.wake"
                        visible: root.presentation.wakeVisible
                        highlighted: root.page.waitingForWake !== true
                        icon.name: "lucide-sunrise"
                        text: root.presentation.wakeLabel
                        accessibleName: text
                        onClicked: {
                            if (root.page.waitingForWake === true) root.page.cancelWakeConnect()
                            else root.page.wakeHost(root.page.selectedHostUuid, true)
                        }
                    }
                    ToolButton {
                        id: hostMenuButton
                        objectName: "plasma-host-actions-button"
                        Layout.minimumWidth: 40
                        Layout.minimumHeight: 40
                        text: qsTranslate("PcView", "Host Actions")
                        icon.name: "lucide-ellipsis-vertical"
                        display: AbstractButton.IconOnly
                        focusPolicy: Qt.TabFocus
                        Accessible.name: text
                        Accessible.onPressAction: clicked()
                        ToolTip.visible: hovered
                        ToolTip.delay: Kirigami.Units.toolTipDelay
                        ToolTip.text: text
                        onClicked: root.hostMenuRequested(hostMenuButton)
                    }
                }
            }
            Kirigami.Separator {
                Layout.fillWidth: true
                visible: facts.visible
            }
            RowLayout {
                id: facts
                objectName: "plasma-host-facts"
                Layout.fillWidth: true
                visible: root.presentation.streamFacts.length > 0
                spacing: Kirigami.Units.largeSpacing
                Repeater {
                    model: root.presentation.streamFacts
                    delegate: ColumnLayout {
                        required property var modelData
                        objectName: "plasma-host-fact-" + modelData.objectName
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 0
                        Accessible.role: Accessible.StaticText
                        Accessible.name: modelData.label + ": " + modelData.value + ", " + modelData.note
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            font: Kirigami.Theme.smallFont
                            opacity: 0.7
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                        Kirigami.Heading {
                            Layout.fillWidth: true
                            level: root.compact ? 4 : 3
                            text: modelData.value
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.note
                            font: Kirigami.Theme.smallFont
                            opacity: 0.7
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                    }
                }
            }
        }
    }
}
