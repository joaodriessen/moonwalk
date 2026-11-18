import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../settings" as Settings
import "../../starlight/Lucide.js" as Lucide

// The top of a host's page: who it is, whether it can be reached, the one thing
// to do next, and what the next connection will be.
Item {
    id: root
    required property var page
    required property var presentation
    readonly property var selected: page.selectedHostSnapshot || ({})
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property bool compact: width < 460
    signal aboutToAct(Item source)

    implicitHeight: card.implicitHeight

    BorderImage {
        anchors.fill: card
        anchors { leftMargin: -6; rightMargin: -6; topMargin: -4; bottomMargin: -8 }
        source: "qrc:/res/card_shadow.png"
        border { left: 22; right: 22; top: 22; bottom: 22 }
    }
    Rectangle {
        id: card
        width: parent.width
        implicitHeight: content.implicitHeight + 2 * root.tokens.spacingL
        radius: root.tokens.radiusCard
        color: root.tokens.card
        border.width: root.tokens.highContrast ? 1 : 0
        border.color: root.tokens.cardBorder

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: root.tokens.spacingL }
            spacing: root.tokens.spacingL

            GridLayout {
                objectName: "gnome-host-identity"
                Layout.fillWidth: true
                columns: root.compact ? 1 : 3
                columnSpacing: root.tokens.spacingL
                rowSpacing: root.tokens.spacingM
                Settings.HostAvatar {
                    id: avatar
                    Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignVCenter
                    size: 64
                    seed: root.selected.uuid || ""
                    iconName: root.presentation.iconName
                    dimmed: !root.presentation.online
                    highContrast: root.tokens.highContrast
                    schemeDark: root.tokens.dark
                    outline: root.tokens.text
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: root.selected.name || ""
                        color: root.tokens.text
                        font.pixelSize: root.tokens.heroTitleSize
                        font.weight: Font.Bold
                        wrapMode: Text.Wrap
                        horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignLeft
                        spacing: 6
                        GnomeStatusDot {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: Math.max(0, (statusLine.lineHeightPixels - height) / 2)
                            tone: root.presentation.statusTone
                        }
                        Label {
                            id: statusLine
                            readonly property real lineHeightPixels: fontMetrics.height
                            FontMetrics { id: fontMetrics; font: statusLine.font }
                            objectName: "gnome-host-status-line"
                            Layout.fillWidth: !root.compact
                            text: root.presentation.statusLine
                            color: root.tokens.secondaryText
                            font.pixelSize: root.tokens.bodySize
                            wrapMode: Text.Wrap
                            Accessible.name: text
                        }
                    }
                }
                ColumnLayout {
                    Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignVCenter
                    spacing: root.tokens.spacingS
                    GnomeButton {
                        objectName: "starlight.hosts.connect"
                        Layout.alignment: Qt.AlignHCenter
                        text: root.presentation.connectLabel
                        accessibleName: text
                        accessibleDescription: root.presentation.runningNote
                        suggested: true
                        pill: true
                        icon.source: Lucide.url("play", "#000000")
                        icon.color: enabled ? root.tokens.accentInk : root.tokens.disabledText
                        icon.width: 16
                        icon.height: 16
                        visible: root.presentation.connectVisible
                        enabled: root.presentation.connectEnabled
                        onClicked: {
                            root.aboutToAct(this)
                            root.page.requestConnect(root.page.selectedHostUuid)
                        }
                    }
                    GnomeButton {
                        objectName: "starlight.hosts.wake"
                        Layout.alignment: Qt.AlignHCenter
                        text: root.presentation.wakeLabel
                        accessibleName: text
                        pill: true
                        suggested: root.page.waitingForWake !== true
                        visible: root.presentation.wakeVisible
                        onClicked: {
                            if (root.page.waitingForWake === true) root.page.cancelWakeConnect()
                            else root.page.wakeHost(root.page.selectedHostUuid, true)
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                visible: facts.visible
                color: root.tokens.cardShade
            }
            // What the next connection will be.
            RowLayout {
                id: facts
                objectName: "gnome-host-facts"
                Layout.fillWidth: true
                visible: root.presentation.streamFacts.length > 0
                spacing: root.tokens.spacingM
                Repeater {
                    model: root.presentation.streamFacts
                    delegate: ColumnLayout {
                        required property var modelData
                        objectName: "gnome-host-fact-" + modelData.objectName
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 0
                        Accessible.role: Accessible.StaticText
                        Accessible.name: modelData.label + ": " + modelData.value + ", " + modelData.note
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: root.tokens.secondaryText
                            font.pixelSize: root.tokens.captionSize
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            color: root.tokens.text
                            font.pixelSize: root.compact ? root.tokens.bodySize : root.tokens.titleSize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.note
                            color: root.tokens.secondaryText
                            font.pixelSize: root.tokens.captionSize
                            elide: Text.ElideRight
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Text.AlignLeft
                        }
                    }
                }
            }
        }
    }
}
