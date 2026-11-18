import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import "../KeyboardShortcuts.js" as KeyboardShortcuts

// Every shortcut in one form, as KDE's shortcut lists read: action, then keys.
PlasmaDialog {
    id: root
    objectName: "plasma-keyboard-shortcuts-dialog"
    title: qsTranslate("KeyboardShortcuts", "Keyboard Shortcuts")
    accessibleName: title
    accessibleDescription: KeyboardShortcuts.reference()
    standardButtons: Dialog.Close
    width: Math.min(Kirigami.Units.gridUnit * 30, parent ? parent.width - Kirigami.Units.gridUnit * 2 : Kirigami.Units.gridUnit * 30)
    height: Math.min(Kirigami.Units.gridUnit * 30, parent ? parent.height - Kirigami.Units.gridUnit * 2 : Kirigami.Units.gridUnit * 30)

    onOpened: scroller.forceActiveFocus()
    contentItem: ScrollView {
        id: scroller
        objectName: "plasma-keyboard-shortcuts-reference"
        focus: true
        activeFocusOnTab: true
        function scrollBy(distance) {
            var view = contentItem
            view.contentY = Math.max(0, Math.min(view.contentHeight - view.height, view.contentY + distance))
        }
        Keys.onUpPressed: scrollBy(-48)
        Keys.onDownPressed: scrollBy(48)
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_PageUp) scrollBy(-height)
            else if (event.key === Qt.Key_PageDown) scrollBy(height)
            else return
            event.accepted = true
        }
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            x: Kirigami.Units.largeSpacing
            width: scroller.availableWidth - 2 * Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: KeyboardShortcuts.groups()
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Heading { level: 4; text: modelData.title; Layout.fillWidth: true }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: modelData.note || ""
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: modelData.shortcuts
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.largeSpacing
                            Label {
                                Layout.fillWidth: true
                                text: modelData.label
                                wrapMode: Text.Wrap
                            }
                            Label {
                                text: modelData.keys.join("+")
                                font.weight: Font.DemiBold
                                Accessible.name: text
                            }
                        }
                    }
                }
            }
        }
    }
}
