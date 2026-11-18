import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../settings" as Settings
import "../../starlight" as AppStyle

// AdwStatusPage: dimmed icon, title-1 heading, description and actions, centred
// in what is left of the view. Used wherever there is one thing to say.
ColumnLayout {
    id: root
    property string title: ""
    property string description: ""
    property string iconName: ""
    property string iconTone: "ink"
    property bool busy: false
    // A host's own tile instead of a dimmed glyph.
    property string seed: ""
    property bool dimmed: false
    // Rings spreading from the glyph while something is being looked for.
    property bool pulsing: false
    default property alias actions: actionRow.data
    readonly property GnomeTokens tokens: GnomeTokens {}
    spacing: 0

    Item {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: root.tokens.spacingXL
        visible: root.iconName.length > 0 && !root.busy
        implicitWidth: 96
        implicitHeight: 96
        Settings.PulseRings {
            anchors.centerIn: parent
            width: 96
            height: 96
            visible: root.pulsing
            running: root.pulsing
            reducedMotion: root.tokens.reducedMotion
            color: root.tokens.accentText
        }
        Settings.HostAvatar {
            anchors.fill: parent
            visible: root.seed.length > 0
            size: 96
            seed: root.seed
            iconName: root.iconName
            dimmed: root.dimmed
            highContrast: root.tokens.highContrast
            schemeDark: root.tokens.dark
            outline: root.tokens.text
        }
        AppStyle.SLIcon {
            anchors.centerIn: parent
            visible: root.seed.length === 0
            name: root.iconName
            tone: root.iconTone
            schemeDark: root.tokens.dark
            size: 80
            opacity: root.iconTone === "ink" ? 0.55 : 1
        }
    }
    GnomeBusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: root.tokens.spacingXL
        implicitWidth: 48
        visible: root.busy
        running: visible
    }
    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.title
        color: root.tokens.text
        font.pixelSize: root.tokens.heroTitleSize
        font.weight: Font.Bold
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }
    Label {
        Layout.fillWidth: true
        Layout.topMargin: root.tokens.spacingM
        visible: text.length > 0
        text: root.description
        color: root.tokens.secondaryText
        font.pixelSize: root.tokens.bodySize
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
    ColumnLayout {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: root.tokens.spacingXL
        spacing: root.tokens.spacingM
    }
}
