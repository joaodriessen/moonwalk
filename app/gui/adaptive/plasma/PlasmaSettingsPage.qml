import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import ".." as Shared
import "../settings" as Settings
import "../../starlight" as AppStyle

// "Configure Starlight": a toolbar with Back above the shared preferences, as a
// single System Settings style form.
Item {
    id: root
    objectName: "starlight.settings.page"
    Accessible.role: Accessible.Pane
    Accessible.name: qsTranslate("SettingsView", "Settings")

    AppStyle.AccessibleIdentity { target: root; stableId: root.objectName }

    Shared.FocusReveal { viewport: contentFlickable }

    required property var page
    readonly property Settings.AppSettingsModel settings: Settings.AppSettingsModel { page: root.page }
    readonly property bool narrow: width < 620

    function findControl(item, name) {
        if (item.objectName === name) return item
        for (var index = 0; index < item.children.length; ++index) {
            var hit = findControl(item.children[index], name)
            if (hit) return hit
        }
        return null
    }
    // The first setting this desktop can actually use.
    function focusInitialControl() {
        for (var g = 0; g < settings.groups.length; ++g) {
            var rows = settings.groups[g].rows
            for (var r = 0; r < rows.length; ++r) {
                if (rows[r].shown === true && rows[r].sensitive === true) {
                    var control = findControl(form, rows[r].controlId)
                    if (control) { control.forceActiveFocus(Qt.TabFocus); return }
                }
            }
        }
        backButton.forceActiveFocus(Qt.TabFocus)
    }
    function routeActivated(focusFirstControl) {
        if (focusFirstControl === true)
            focusInitialControl()
    }

    PlasmaPalette { id: pagePalette }
    Rectangle {
        objectName: "plasmaPageGround"
        anchors.fill: parent
        color: pagePalette.palette.window
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ToolBar {
            objectName: "plasma-settings-toolbar"
            Layout.fillWidth: true
            position: ToolBar.Header
            Kirigami.Theme.colorSet: Kirigami.Theme.Header
            Kirigami.Theme.inherit: false
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                PlasmaNavigationButton {
                    id: backButton
                    objectName: "plasma-settings-back"
                    text: qsTranslate("SettingsView", "Back")
                    icon.name: mirrored ? "lucide-chevron-right" : "lucide-chevron-left"
                    accessibleName: qsTranslate("SettingsView", "Back to hosts")
                    onNavigationRequested: root.page.requestBack(false)
                }
                Kirigami.Heading {
                    Layout.fillWidth: true
                    visible: !root.narrow
                    level: 2
                    text: qsTranslate("SettingsView", "Configure Moonwalk")
                    elide: Text.ElideRight
                }
                Kirigami.SearchField {
                    id: searchField
                    objectName: "starlight.settings.search"
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                    Layout.fillWidth: root.narrow
                    placeholderText: qsTranslate("SettingsView", "Search preferences")
                    Accessible.name: placeholderText
                    onTextChanged: { root.settings.searchText = text; contentFlickable.contentY = 0 }
                    Shortcut {
                        sequences: [StandardKey.Find]
                        onActivated: searchField.forceActiveFocus()
                    }
                }
            }
        }
        Kirigami.PlaceholderMessage {
            objectName: "plasma-settings-no-results"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Kirigami.Units.gridUnit * 2
            visible: !root.settings.searchHasResults
            icon.name: "lucide-search-x"
            text: qsTranslate("SettingsView", "No Results Found")
            explanation: qsTranslate("SettingsView", "Try a different search.")
        }
        Flickable {
            id: contentFlickable
            objectName: "plasma-settings-viewport"
            visible: root.settings.searchHasResults
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: form.implicitHeight + Kirigami.Units.gridUnit * 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: PlasmaScrollBar { accessibleName: qsTranslate("SettingsView", "Settings scroll bar") }
            PlasmaSettingsForm {
                id: form
                x: Math.max(Kirigami.Units.largeSpacing * 2, (contentFlickable.width - width) / 2)
                y: Kirigami.Units.gridUnit
                width: Math.min(Kirigami.Units.gridUnit * 40, contentFlickable.width - Kirigami.Units.largeSpacing * 4)
                groups: root.settings.groups
            }
        }
    }
}
