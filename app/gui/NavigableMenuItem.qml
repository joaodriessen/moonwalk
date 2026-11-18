import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

import Starlight 1.0

MenuItem {
    id: control

    // Ensure focus can't be given to an invisible item
    enabled: visible
    height: visible ? implicitHeight : 0
    focusPolicy: visible ? Qt.TabFocus : Qt.NoFocus

    implicitHeight: Math.max(30, contentRow.implicitHeight + topPadding + bottomPadding)
    // Re-assert the content-driven width measure: Qt 6.11's Menu sizing
    // reported ~0 implicit width for custom-contentItem MenuItems (menu
    // collapsed to a padding-wide sliver once rendered in-scene).
    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
    padding: 6
    leftPadding: 10
    rightPadding: 10

    onTriggered: {
        // We must close the context menu first or
        // it can steal focus from any dialogs that
        // onTriggered may spawn.
        menu.close()
    }

    Keys.onReturnPressed: {
        triggered()
    }

    Keys.onEnterPressed: {
        triggered()
    }

    Keys.onEscapePressed: {
        menu.close()
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: (control.highlighted || control.down) ? Theme.accent : "transparent"
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: 8

        Text {
            // Reserve the gutter width even when unchecked/non-checkable so
            // text stays aligned across mixed checkable/plain menu items.
            opacity: (control.checkable && control.checked) ? 1 : 0
            text: "✓"
            color: (control.highlighted || control.down) ? "#ffffff" : Theme.accent
            font.pixelSize: Theme.sizeBody
            font.bold: true
            Layout.preferredWidth: 14
        }

        Text {
            Layout.fillWidth: true
            text: control.text
            color: control.enabled ?
                       ((control.highlighted || control.down) ? "#ffffff" : Theme.label) :
                       Theme.label3
            font.pixelSize: Theme.sizeBody
            font.bold: control.font.bold
            elide: Text.ElideRight
        }
    }
}
