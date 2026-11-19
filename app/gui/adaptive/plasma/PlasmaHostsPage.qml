import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import ".." as Shared
import "../settings" as Settings
import "../../starlight" as AppStyle

// A Kirigami application page: one toolbar across the window with the add and
// main-menu actions, a view-coloured host list, and the selected host as a
// single heading above a System Settings style form. KWin draws the titlebar.
Item {
    id: root
    objectName: "starlight.hosts.page"
    Accessible.role: Accessible.Pane
    Accessible.name: qsTranslate("PcView", "Hosts")

    AppStyle.AccessibleIdentity { target: root; stableId: root.objectName }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Shared.FocusReveal { viewport: detailsFlickable }

    required property var page
    readonly property Settings.HostPresentation presentation: Settings.HostPresentation { page: root.page }
    readonly property bool narrow: width < 680
    readonly property bool darkScheme: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.textColor) === Kirigami.ColorUtils.Light
    readonly property var selected: page.selectedHostSnapshot || ({})
    readonly property var selectedPreferences: page.hostPreferenceSnapshot || ({})
    property bool narrowShowsDetails: false
    readonly property bool detailsFirst: narrowShowsDetails || sidebarCount === 0
    // ListView.count change notifications proved unreliable under offscreen
    // rendering, while direct reads are always fresh: recount from row signals.
    property int sidebarCount: 0
    function refreshSidebarCount() { sidebarCount = hostList.count }
    property Item focusReturnTarget: null
    property var hostItems: ({})

    function rememberFocusTarget(target) { focusReturnTarget = target }
    function selectedHostItem() {
        // Collapsed to the details page, the list is hidden: use what opened the action.
        if (narrow && narrowShowsDetails && sidebarCount > 0) return hero.hostMenuButton
        return hostItems[page.selectedHostUuid] || hostList.currentItem || hostList
    }
    function selectHost(uuid) {
        page.selectHostByUuid(uuid)
        hostList.forceActiveFocus(Qt.OtherFocusReason)
        if (narrow) narrowShowsDetails = true
    }
    function moveHostSelection(index) {
        if (hostList.count === 0) return
        index = Math.max(0, Math.min(index, hostList.count - 1))
        if (page.selectHostAtIndex(index)) {
            hostList.positionViewAtIndex(index, ListView.Contain)
            hostList.forceActiveFocus(Qt.TabFocusReason)
        }
    }
    function activateHost(uuid) {
        if (uuid) page.selectHostByUuid(uuid)
        var host = page.selectedHostSnapshot || ({})
        if (!host.uuid || host.statusUnknown === true) return
        rememberFocusTarget(selectedHostItem())
        if (host.online === true) {
            if (host.paired !== true) page.beginPairing()
            else if (host.serverSupported !== false) page.requestConnect(host.uuid)
        } else if (host.wakeable === true) page.wakeHost(host.uuid, true)
    }
    function routeActivated() { hostList.forceActiveFocus(Qt.TabFocus) }
    function showSelectedHostDetails() { rememberFocusTarget(selectedHostItem()); page.requestDetails(page.selectedHostUuid) }
    function renameSelectedHost() { rememberFocusTarget(selectedHostItem()); page.requestRename(page.selectedHostUuid) }
    function removeSelectedHost() { rememberFocusTarget(selectedHostItem()); page.requestDelete(page.selectedHostUuid) }
    function openHostMenu(sourceItem, x, y) { hostMenu.popup(sourceItem, x, y) }

    PlasmaPalette { id: pagePalette }
    Rectangle {
        objectName: "plasmaPageGround"
        anchors.fill: parent
        color: pagePalette.palette.window
    }
    Connections {
        target: root.page.hosts || null
        ignoreUnknownSignals: true
        function onRowsInserted() { Qt.callLater(root.refreshSidebarCount) }
        function onRowsRemoved() { Qt.callLater(root.refreshSidebarCount) }
        function onModelReset() { Qt.callLater(root.refreshSidebarCount) }
        function onCountChanged() { Qt.callLater(root.refreshSidebarCount) }
    }
    Component.onCompleted: refreshSidebarCount()
    Connections {
        target: root.page
        function onSelectedHostUuidChanged() {
            detailsFlickable.contentY = 0
            arrival.restart()
        }
    }
    // A different host's page settling in; Kirigami's durations are zero when
    // the desktop's animation speed is set to instant.
    NumberAnimation {
        id: arrival
        target: detailsFlickable
        property: "opacity"
        from: Kirigami.Units.shortDuration > 0 ? 0.4 : 1
        to: 1
        duration: Kirigami.Units.shortDuration
        easing.type: Easing.OutCubic
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            id: toolbar
            objectName: "plasma-hosts-toolbar"
            Layout.fillWidth: true
            position: ToolBar.Header
            Kirigami.Theme.colorSet: Kirigami.Theme.Header
            Kirigami.Theme.inherit: false

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                PlasmaNavigationButton {
                    objectName: "plasma-hosts-back"
                    visible: root.narrow && root.narrowShowsDetails && root.sidebarCount > 0
                    enabled: visible
                    text: qsTranslate("PcView", "Hosts")
                    accessibleName: qsTranslate("PcView", "Back")
                    icon.name: mirrored ? "lucide-chevron-right" : "lucide-chevron-left"
                    display: AbstractButton.IconOnly
                    onNavigationRequested: root.narrowShowsDetails = false
                }
                ToolButton {
                    id: addHostButton
                    objectName: "starlight.hosts.add-host"
                    visible: !root.narrow || !root.narrowShowsDetails || root.sidebarCount === 0
                    text: qsTranslate("PcView", "Add Host…")
                    icon.name: "lucide-plus"
                    focusPolicy: Qt.TabFocus
                    display: root.narrow ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                    Accessible.name: text
                    Accessible.onPressAction: clicked()
                    onClicked: {
                        root.rememberFocusTarget(this)
                        root.page.requestAddHostOverlay()
                    }
                    AppStyle.AccessibleIdentity { target: addHostButton; stableId: addHostButton.objectName }
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    id: appMenuButton
                    objectName: "plasma-app-menu-button"
                    text: qsTranslate("PcView", "Main Menu")
                    icon.name: "lucide-menu"
                    display: AbstractButton.IconOnly
                    focusPolicy: Qt.TabFocus
                    Accessible.name: text
                    Accessible.onPressAction: clicked()
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: text
                    onClicked: appMenu.popup(appMenuButton, 0, appMenuButton.height)
                }
                PlasmaMenu {
                    id: appMenu
                    objectName: "plasma-app-menu"
                    MenuItem {
                        objectName: "starlight.hosts.open-settings"
                        text: qsTranslate("SettingsView", "Configure Moonwalk…")
                        icon.name: "lucide-settings"
                        Accessible.name: text
                        onTriggered: root.page.requestOpenSettings()
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuItem {
                        objectName: "plasma-keyboard-shortcuts-action"
                        text: qsTranslate("KeyboardShortcuts", "Keyboard Shortcuts")
                        icon.name: "lucide-keyboard"
                        Accessible.name: text
                        onTriggered: shortcutsDialog.open()
                    }
                    MenuSeparator {}
                    MenuItem {
                        objectName: "starlight.hosts.connection-test"
                        text: qsTranslate("PcView", "Test Internet Streaming Ports…")
                        icon.name: "lucide-globe"
                        Accessible.name: text
                        onTriggered: {
                            root.rememberFocusTarget(appMenuButton)
                            root.page.testConnection()
                        }
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuItem {
                        objectName: "plasma-diagnostics-action"
                        text: qsTranslate("PcView", "Copy Diagnostic Summary")
                        icon.name: "lucide-copy"
                        Accessible.name: text
                        onTriggered: {
                            if (typeof window !== "undefined" && window.copyDiagnosticSummary)
                                window.copyDiagnosticSummary()
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        objectName: "starlight.hosts.about"
                        text: qsTranslate("PcView", "About Moonwalk")
                        icon.name: "lucide-info"
                        Accessible.name: text
                        onTriggered: aboutDialog.open()
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Hosts, on the view background as a Kirigami sidebar list.
            Rectangle {
                id: hostsPane
                Layout.preferredWidth: root.narrow ? parent.width : Math.max(Kirigami.Units.gridUnit * 12, Math.min(Kirigami.Units.gridUnit * 15, parent.width * 0.26))
                Layout.fillHeight: true
                // With no hosts there is no list to show: the guidance is the page.
                visible: (!root.narrow || !root.detailsFirst) && root.sidebarCount > 0
                enabled: !root.narrow || !root.detailsFirst
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor

                ListView {
                    id: hostList
                    objectName: "starlight.hosts.list"
                    AppStyle.AccessibleIdentity { target: hostList; stableId: hostList.objectName }
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    clip: true
                    spacing: 2
                    model: root.page.hosts
                    currentIndex: root.page.selectedHostIndex
                    keyNavigationEnabled: false
                    boundsBehavior: Flickable.StopAtBounds
                    Keys.onUpPressed: root.moveHostSelection(currentIndex - 1)
                    Keys.onDownPressed: root.moveHostSelection(currentIndex + 1)
                    Keys.onReturnPressed: root.activateHost(root.page.selectedHostUuid)
                    Keys.onEnterPressed: root.activateHost(root.page.selectedHostUuid)
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Home) root.moveHostSelection(0)
                        else if (event.key === Qt.Key_End) root.moveHostSelection(count - 1)
                        else if (event.key === Qt.Key_Space) root.activateHost(root.page.selectedHostUuid)
                        else return
                        event.accepted = true
                    }
                    focus: hostsPane.visible
                    Accessible.role: Accessible.List
                    Accessible.name: qsTranslate("PcView", "Hosts")

                    delegate: PlasmaListItem {
                        id: hostDelegate
                        required name
                        required online
                        required property string uuid
                        required property bool paired
                        required property bool busy
                        required property bool wakeable
                        required property bool serverSupported
                        required property var model
                        objectName: "starlight.hosts.item." + uuid
                        width: ListView.view.width
                        iconName: root.presentation.osIconName(model.hostOs !== undefined ? model.hostOs : "")
                        seed: uuid
                        statusText: root.presentation.statusFor(online, paired, model.statusUnknown === true, serverSupported, busy)
                        tone: root.presentation.toneFor(online, paired, model.statusUnknown === true, serverSupported, busy)
                        highlighted: !root.narrow && root.page.selectedHostUuid === uuid
                        Component.onCompleted: root.hostItems[uuid] = hostDelegate
                        Component.onDestruction: delete root.hostItems[uuid]
                        onClicked: root.selectHost(uuid)
                        onDoubleClicked: root.activateHost(uuid)
                        Keys.onReturnPressed: root.activateHost(uuid)
                        Keys.onEnterPressed: root.activateHost(uuid)
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Space) {
                                root.activateHost(uuid)
                                event.accepted = true
                            }
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: function(eventPoint) {
                                root.page.selectHostByUuid(hostDelegate.uuid)
                                root.openHostMenu(hostDelegate, eventPoint.position.x, eventPoint.position.y)
                            }
                        }
                    }
                    ScrollBar.vertical: PlasmaScrollBar { accessibleName: qsTranslate("PcView", "Hosts") }
                }
            }
            Kirigami.Separator {
                Layout.fillHeight: true
                visible: !root.narrow && root.sidebarCount > 0
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !root.narrow || root.detailsFirst
                enabled: !root.narrow || root.detailsFirst

                PlasmaMenu {
                    id: hostMenu
                    objectName: "plasma-host-actions-menu"
                    MenuItem {
                        objectName: "starlight.hosts.details"
                        text: qsTranslate("PcView", "Host Details")
                        icon.name: "lucide-info"
                        enabled: !!root.selected.uuid
                        Accessible.name: text
                        onTriggered: root.showSelectedHostDetails()
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuItem {
                        objectName: "starlight.hosts.rename"
                        text: qsTranslate("PcView", "Rename…")
                        icon.name: "lucide-pencil"
                        enabled: !!root.selected.uuid
                        Accessible.name: text
                        onTriggered: root.renameSelectedHost()
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuItem {
                        objectName: "starlight.hosts.resume-running"
                        text: qsTranslate("PcView", "Resume Running App")
                        icon.name: "lucide-play"
                        visible: root.presentation.sessionActionsAvailable && !root.presentation.resumeDesktop
                        height: visible ? implicitHeight : 0
                        enabled: visible && root.selected.serverSupported === true
                        Accessible.name: text
                        onTriggered: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestResume(root.page.selectedHostUuid) }
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuSeparator {}
                    MenuItem {
                        objectName: "starlight.hosts.end-session"
                        text: qsTranslate("PcView", "End Remote Session…")
                        icon.name: "lucide-log-out"
                        visible: root.presentation.sessionActionsAvailable
                        height: visible ? implicitHeight : 0
                        enabled: visible
                        Accessible.name: text
                        onTriggered: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestEndSession(root.page.selectedHostUuid) }
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                    MenuItem {
                        objectName: "starlight.hosts.delete"
                        text: qsTranslate("PcView", "Remove Host")
                        icon.name: "lucide-trash-2"
                        enabled: !!root.selected.uuid
                        Accessible.name: text
                        onTriggered: root.removeSelectedHost()
                        AppStyle.AccessibleIdentity { target: parent; stableId: parent.objectName }
                    }
                }
                PlasmaShortcutsDialog { id: shortcutsDialog }
                PlasmaAboutDialog {
                    id: aboutDialog
                    applicationVersion: root.selectedPreferences.starlightVersionString
                                        || qsTranslate("PcView", "Development build")
                }

                // No hosts: first launch or nothing discovered yet.
                Item {
                    id: emptyState
                    objectName: "starlight.hosts.empty-state"
                    anchors.fill: parent
                    visible: root.sidebarCount === 0
                    enabled: root.sidebarCount === 0
                    readonly property bool discoveryOff: root.presentation.discoveryOff
                    readonly property string emptyMessage: root.presentation.emptyTitle
                    readonly property string guidance: root.presentation.emptyBody
                    Accessible.role: Accessible.StaticText
                    Accessible.name: emptyMessage
                    Accessible.description: guidance

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 24)
                        spacing: Kirigami.Units.largeSpacing
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: Kirigami.Units.iconSizes.huge
                            implicitHeight: implicitWidth
                            Settings.PulseRings {
                                anchors.fill: parent
                                visible: root.presentation.stillSearching
                                running: visible
                                reducedMotion: Kirigami.Units.longDuration === 0
                                color: Kirigami.Theme.highlightColor
                            }
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                implicitWidth: Kirigami.Units.iconSizes.large
                                implicitHeight: implicitWidth
                                source: "lucide-monitor"
                            }
                        }
                        Kirigami.PlaceholderMessage {
                            Layout.fillWidth: true
                            text: emptyState.emptyMessage
                            explanation: emptyState.guidance
                            helpfulAction: Kirigami.Action {
                                text: qsTranslate("PcView", "Add Host…")
                                icon.name: "lucide-plus"
                                onTriggered: {
                                    root.rememberFocusTarget(addHostButton)
                                    root.page.requestAddHostOverlay()
                                }
                            }
                        }
                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            objectName: "starlight.hosts.setup-guide"
                            flat: true
                            font.underline: true
                            text: qsTranslate("PcView", "Set Up a Host")
                            onClicked: Qt.openUrlExternally(root.presentation.setupGuide)
                        }
                    }
                }

                // A host with nothing to configure yet: one message, one action.
                Item {
                    objectName: "plasma-host-status-page"
                    anchors.fill: parent
                    visible: root.sidebarCount > 0 && !root.presentation.settingsVisible && root.presentation.hasHost
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 24)
                        spacing: Kirigami.Units.largeSpacing
                        PlasmaBusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.presentation.checking
                            running: visible
                        }
                        Settings.HostAvatar {
                            Layout.alignment: Qt.AlignHCenter
                            visible: !root.presentation.checking
                            size: Kirigami.Units.iconSizes.huge
                            seed: root.selected.uuid || ""
                            iconName: root.presentation.iconName
                            dimmed: !root.presentation.online
                        }
                        Kirigami.Heading {
                            Layout.fillWidth: true
                            level: 1
                            text: root.presentation.statusPageTitle
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.presentation.statusPageBody
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.8
                        }
                        PlasmaButton {
                            objectName: "starlight.hosts.pair"
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.presentation.pairVisible
                            enabled: visible
                            highlighted: true
                            text: qsTranslate("PcView", "Pair")
                            icon.name: "lucide-link"
                            accessibleName: text
                            onClicked: {
                                root.rememberFocusTarget(this)
                                root.page.beginPairing()
                            }
                        }
                        PlasmaButton {
                            objectName: "plasma-hosts-status-wake"
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.presentation.wakeVisible && !root.presentation.paired
                            highlighted: root.page.waitingForWake !== true
                            text: root.presentation.wakeLabel
                            icon.name: "lucide-sunrise"
                            accessibleName: text
                            onClicked: { if (root.page.waitingForWake === true) root.page.cancelWakeConnect(); else root.page.wakeHost(root.page.selectedHostUuid, true) }
                        }
                    }
                }

                Flickable {
                    id: detailsFlickable
                    objectName: "plasma-hosts-viewport"
                    anchors.fill: parent
                    visible: root.sidebarCount > 0 && root.presentation.settingsVisible
                    contentWidth: width
                    contentHeight: detailsColumn.implicitHeight + Kirigami.Units.gridUnit * 2
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: PlasmaScrollBar { accessibleName: qsTranslate("PcView", "Hosts") }

                    ColumnLayout {
                        id: detailsColumn
                        x: Math.max(Kirigami.Units.largeSpacing * 2, (detailsFlickable.width - width) / 2)
                        y: Kirigami.Units.gridUnit
                        // Room is kept for the overlay scroll bar beside the card's edge.
                        width: Math.min(Kirigami.Units.gridUnit * 40, detailsFlickable.width - Kirigami.Units.largeSpacing * 4 - Kirigami.Units.gridUnit)
                        spacing: Kirigami.Units.largeSpacing * 2

                        PlasmaHostHero {
                            id: hero
                            Layout.fillWidth: true
                            page: root.page
                            presentation: root.presentation
                            onAboutToAct: function(source) { root.rememberFocusTarget(source) }
                            onHostMenuRequested: function(source) { root.openHostMenu(source, 0, source.height) }
                        }
                        Kirigami.InlineMessage {
                            objectName: "starlight.hosts.unsupported-server"
                            Layout.fillWidth: true
                            visible: text.length > 0
                            type: Kirigami.MessageType.Warning
                            text: root.presentation.unsupportedNote
                        }
                        Kirigami.InlineMessage {
                            objectName: "plasma-host-waking-note"
                            Layout.fillWidth: true
                            visible: text.length > 0
                            type: Kirigami.MessageType.Information
                            icon.name: "lucide-sunrise"
                            text: root.presentation.wakingNote
                        }
                        Kirigami.InlineMessage {
                            objectName: "plasma-host-running-note"
                            Layout.fillWidth: true
                            visible: text.length > 0
                            type: Kirigami.MessageType.Information
                            text: root.presentation.runningNote
                            actions: [
                                Kirigami.Action {
                                    text: root.presentation.resumeRunningLabel
                                    icon.name: "lucide-play"
                                    onTriggered: {
                                        root.rememberFocusTarget(root.selectedHostItem())
                                        root.page.requestResume(root.page.selectedHostUuid)
                                    }
                                }
                            ]
                        }
                        PlasmaSettingsForm {
                            objectName: "plasma-host-preferences"
                            Layout.fillWidth: true
                            groups: root.presentation.groups
                            enabled: root.page.selectedHostUuid.length > 0
                        }
                    }
                }
            }
        }
    }

    // KDE's hamburger-menu key.
    Shortcut {
        sequences: ["F10"]
        onActivated: appMenu.popup(appMenuButton, 0, appMenuButton.height)
    }
    Shortcut {
        // One sequence only: Qt maps the keyboard layout to it, and a second match for
        // the same press makes the shortcut ambiguous, so it would never activate.
        sequence: "Ctrl+?"
        onActivated: shortcutsDialog.open()
    }

    PlasmaHostDialogLayer {
        objectName: "plasma-host-dialog-layer"
        page: root.page
        focusReturnTarget: root.focusReturnTarget || hostList
    }
}
