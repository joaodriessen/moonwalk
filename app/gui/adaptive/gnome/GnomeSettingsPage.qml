import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import ".." as Shared
import "../settings" as Settings
import "../../starlight" as AppStyle

// AdwPreferencesDialog content: a title, a close button and every preference
// on one page. main.qml floats it over the hosts page as a dialog.
Item {
    id: root
    objectName: "starlight.settings.page"
    Accessible.role: Accessible.Pane
    Accessible.name: qsTranslate("SettingsView", "Settings")

    AppStyle.AccessibleIdentity { target: root; stableId: root.objectName }

    Shared.FocusReveal { viewport: contentFlickable }

    required property var page
    readonly property Settings.AppSettingsModel settings: Settings.AppSettingsModel { page: root.page }
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property bool narrow: width < 620
    readonly property bool windowActive: Window.window ? Window.window.active : true
    // AdwPreferencesDialog's search: a toggle in the header, Ctrl+F from anywhere.
    property bool searchOpen: false
    readonly property bool searching: searchField.text.trim().length > 0
    function closeSearch() {
        searchField.text = ""
        searchOpen = false
        searchToggle.forceActiveFocus(Qt.TabFocusReason)
    }
    onSearchingChanged: if (searching) searchOpen = true

    // The first setting this desktop can actually use.
    function focusInitialControl() {
        for (var g = 0; g < settings.groups.length; ++g) {
            var rows = settings.groups[g].rows
            for (var r = 0; r < rows.length; ++r) {
                if (rows[r].shown === true && rows[r].sensitive === true) {
                    var control = findControl(content, rows[r].controlId)
                    if (control) { control.forceActiveFocus(Qt.TabFocus); return }
                }
            }
        }
        closeButton.forceActiveFocus(Qt.TabFocus)
    }
    function findControl(item, name) {
        if (item.objectName === name) return item
        for (var index = 0; index < item.children.length; ++index) {
            var hit = findControl(item.children[index], name)
            if (hit) return hit
        }
        return null
    }
    function routeActivated(focusFirstControl) {
        if (focusFirstControl === true)
            focusInitialControl()
        else
            closeButton.forceActiveFocus(Qt.OtherFocusReason)
    }

    Rectangle {
        anchors.fill: parent
        color: root.tokens.dialogSurface
        radius: root.tokens.radiusDialog
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Item {
            objectName: "gnome-settings-header"
            Layout.fillWidth: true
            Layout.preferredHeight: root.tokens.toolbarHeight

            Label {
                anchors.centerIn: parent
                text: qsTranslate("SettingsView", "Preferences")
                color: root.tokens.text
                font.pixelSize: root.tokens.bodySize
                font.bold: true
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            GnomeHeaderButton {
                id: searchToggle
                objectName: "gnome-settings-search-toggle"
                anchors.left: parent.left
                anchors.leftMargin: root.tokens.headerbarPaddingLeft
                anchors.verticalCenter: parent.verticalCenter
                symbol: "search"
                popupVisible: root.searchOpen
                accessibleName: qsTranslate("SettingsView", "Search Preferences")
                onClicked: {
                    if (root.searchOpen) root.closeSearch()
                    else { root.searchOpen = true; searchField.forceActiveFocus() }
                }
            }
            GnomeHeaderButton {
                id: closeButton
                objectName: "gnome-settings-close"
                anchors.right: parent.right
                anchors.rightMargin: root.tokens.headerbarPaddingRight
                anchors.verticalCenter: parent.verticalCenter
                symbol: "close"
                accessibleName: qsTranslate("SettingsView", "Close Preferences")
                onClicked: root.page.requestBack(false)
            }
            // Undershoot when content scrolls behind the flat bar (_drawing.scss).
            Item {
                objectName: "gnome-settings-undershoot"
                anchors { top: parent.bottom; left: parent.left; right: parent.right }
                height: 5
                z: 2
                readonly property bool shown: contentFlickable.contentY > 0
                visible: shown
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.tokens.undershoot }
                Rectangle {
                    anchors.top: parent.top
                    anchors.topMargin: 1
                    width: parent.width
                    height: 4
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: root.tokens.undershoot }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
            }
        }

        GnomeTextField {
            id: searchField
            objectName: "starlight.settings.search"
            Layout.fillWidth: true
            Layout.leftMargin: root.tokens.spacingM
            Layout.rightMargin: root.tokens.spacingM
            Layout.bottomMargin: root.tokens.spacingS
            visible: root.searchOpen
            placeholderText: qsTranslate("SettingsView", "Search preferences")
            accessibleName: placeholderText
            onTextChanged: { root.settings.searchText = text; contentFlickable.contentY = 0 }
        }
        Shortcut {
            sequences: [StandardKey.Find]
            onActivated: { root.searchOpen = true; searchField.forceActiveFocus() }
        }

        GnomeStatusPage {
            objectName: "gnome-settings-no-results"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: root.tokens.spacingXL
            visible: !root.settings.searchHasResults
            title: qsTranslate("SettingsView", "No Results Found")
            description: qsTranslate("SettingsView", "Try a different search.")
        }
        Flickable {
            id: contentFlickable
            objectName: "gnome-settings-flickable"
            visible: root.settings.searchHasResults
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: content.implicitHeight + 2 * root.tokens.spacingL
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: GnomeScrollBar { accessibleName: qsTranslate("SettingsView", "Settings scroll bar") }

            GnomeSettingsList {
                id: content
                x: Math.max(root.tokens.spacingM, (contentFlickable.width - width) / 2)
                y: root.tokens.spacingS
                width: Math.min(root.tokens.contentClamp, contentFlickable.width - 2 * root.tokens.spacingM)
                groups: root.settings.groups
            }
        }
    }
}
