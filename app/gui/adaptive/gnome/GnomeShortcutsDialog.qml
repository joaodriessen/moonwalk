import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../KeyboardShortcuts.js" as KeyboardShortcuts

// AdwShortcutsDialog: every shortcut with its keys drawn, grouped by where it works.
GnomeDialog {
    id: root
    objectName: "gnome-keyboard-shortcuts-dialog"
    title: qsTranslate("KeyboardShortcuts", "Keyboard Shortcuts")
    accessibleName: title
    accessibleDescription: KeyboardShortcuts.reference()
    closeInHeader: true
    width: fittedWidth(520)
    height: fittedHeight
    standardButtons: Dialog.NoButton
    onCloseRequested: close()
    onOpened: scroller.forceActiveFocus()

    contentItem: Flickable {
        id: scroller
        objectName: "gnome-keyboard-shortcuts-reference"
        focus: true
        activeFocusOnTab: true
        function scrollBy(distance) {
            contentY = Math.max(0, Math.min(contentHeight - height, contentY + distance))
        }
        Keys.onUpPressed: scrollBy(-48)
        Keys.onDownPressed: scrollBy(48)
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_PageUp) scrollBy(-height)
            else if (event.key === Qt.Key_PageDown) scrollBy(height)
            else if (event.key === Qt.Key_Home) scrollBy(-contentHeight)
            else if (event.key === Qt.Key_End) scrollBy(contentHeight)
            else if (event.key === Qt.Key_Escape) root.close()
            else return
            event.accepted = true
        }
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        contentWidth: width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: GnomeScrollBar { accessibleName: root.title }
        ColumnLayout {
            id: column
            width: scroller.width
            spacing: root.tokens.spacingL
            Repeater {
                model: KeyboardShortcuts.groups()
                delegate: GnomeGroup {
                    required property var modelData
                    Layout.fillWidth: true
                    title: modelData.title
                    description: modelData.note || ""
                    Repeater {
                        model: modelData.shortcuts
                        delegate: GnomeRowBase {
                            required property var modelData
                            title: modelData.label
                            Accessible.role: Accessible.StaticText
                            Accessible.name: modelData.label + ": " + modelData.keys.join("+")
                            Row {
                                spacing: 3
                                Repeater {
                                    model: modelData.keys
                                    delegate: GnomeKeycap {
                                        required property string modelData
                                        text: modelData
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
