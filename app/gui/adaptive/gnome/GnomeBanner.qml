import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// AdwBanner on a card: one thing worth knowing about this host, with what can
// be done about it beside the words.
Rectangle {
    id: root
    property string text: ""
    // "info" or "warning"
    property string tone: "info"
    property string actionText: ""
    property bool busy: false
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property color ink: tone === "warning" ? tokens.warning : tokens.accentText
    signal actionTriggered()

    visible: text.length > 0
    radius: tokens.radiusCard
    color: Qt.rgba(ink.r, ink.g, ink.b, tokens.dark ? 0.16 : 0.10)
    border.width: tokens.highContrast ? 1 : 0
    border.color: tokens.border
    implicitHeight: Math.max(46, layout.implicitHeight + 2 * tokens.spacingS)
    Accessible.role: Accessible.StaticText
    Accessible.name: text

    RowLayout {
        id: layout
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: root.tokens.spacingM; rightMargin: root.tokens.spacingS }
        spacing: root.tokens.spacingM
        GnomeBusyIndicator {
            implicitWidth: 16
            visible: root.busy
            running: visible
        }
        Label {
            Layout.fillWidth: true
            text: root.text
            color: root.tokens.text
            font.pixelSize: root.tokens.bodySize
            wrapMode: Text.WordWrap
        }
        GnomeButton {
            objectName: "gnome-banner-action"
            visible: root.actionText.length > 0
            text: root.actionText
            accessibleName: text
            onClicked: root.actionTriggered()
        }
    }
}
