import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import ".." as Shared
import "../settings" as Settings
import "../../starlight" as AppStyle

// AdwNavigationSplitView: a host sidebar, and a preferences-style page for the
// selected host with what to do next at the top and Quality immediately below.
// The GNOME route draws its own header bars; the same surface serves other
// desktops without them, where the compositor owns the window frame.
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
    readonly property GnomeTokens tokens: GnomeTokens {}
    // Client-side decoration: active only under the GNOME adapter, where
    // the window is frameless and the page draws window controls and rounded
    // corners itself.
    readonly property bool chromeActive:
        typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true
    // Read through the attached property inline: a declared object-typed
    // window property would let the test object-graph walk escape into the
    // whole window subtree.
    // Single source of truth: the window owns the CSD geometry and the page
    // mirrors it, so they cannot disagree during maximize/restore.
    readonly property int windowCornerRadius:
        chromeActive && Window.window !== null
        && Window.window.csdRadius !== undefined
        ? Window.window.csdRadius : 0
    readonly property bool floatingChrome: windowCornerRadius > 0
    readonly property int headerHeight: tokens.headerbarHeight
    // AdwBreakpoint: below this, the split view collapses into two pages. With no
    // hosts there is no list to split off, so the page has the whole window.
    readonly property bool narrow: width < 680 || sidebarCount === 0
    readonly property bool compactHeader: detailsColumn.width < 420
    readonly property bool windowActive: Window.window ? Window.window.active : true
    readonly property color sidebarSurfaceColor:
        windowActive ? tokens.sidebar : tokens.sidebarBackdrop
    // headerbar:backdrop dims its contents with filter: opacity(0.5)
    // (_header-bar.scss:41); the 200ms ease-out is $backdrop_transition.
    readonly property real headerContentOpacity: windowActive ? 1.0 : 0.5
    readonly property var selected: page.selectedHostSnapshot || ({})
    property bool narrowShowsDetails: false
    readonly property bool titleInHeader: !detailsFlickable.visible || detailsFlickable.contentY > hero.height * 0.5
    // With no hosts there is no list to show: the guidance is the page.
    readonly property bool detailsFirst: narrowShowsDetails || sidebarCount === 0

    // ListView.count change notifications proved unreliable under offscreen
    // rendering, while direct reads are always fresh: recount from the model's
    // own row signals instead.
    property int sidebarCount: 0
    function refreshSidebarCount() { sidebarCount = hostList.count }
    property Item focusReturnTarget: null
    property Item hostContextSource: null
    property var hostItems: ({})

    function rememberFocusTarget(target) { focusReturnTarget = target }
    function selectedHostItem() {
        // Collapsed to the details page, the list is hidden: use what opened the action.
        if (narrow && narrowShowsDetails && sidebarCount > 0) return hostMenuButton
        return hostItems[page.selectedHostUuid] || hostList.currentItem || hostList
    }
    function selectHost(uuid) {
        page.selectHostByUuid(uuid)
        hostList.forceActiveFocus(Qt.OtherFocusReason)
        if (narrow)
            narrowShowsDetails = true
    }
    function showSelectedHostDetails() {
        rememberFocusTarget(selectedHostItem())
        page.requestDetails(page.selectedHostUuid)
    }
    function renameSelectedHost() {
        rememberFocusTarget(selectedHostItem())
        page.requestRename(page.selectedHostUuid)
    }
    function removeSelectedHost() {
        rememberFocusTarget(selectedHostItem())
        page.requestDelete(page.selectedHostUuid)
    }
    function openHostContextMenu(uuid, sourceItem, x, y) {
        page.selectHostByUuid(uuid)
        hostContextSource = sourceItem
        hostContextMenu.popup(sourceItem, x, y)
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
    function routeActivated() {
        hostList.forceActiveFocus(Qt.TabFocus)
    }
    function startWindowMove() {
        if (root.Window.window) root.Window.window.startSystemMove()
    }
    function toggleMaximized() {
        var win = root.Window.window
        if (!win) return
        win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
    }

    Rectangle {
        anchors.fill: parent
        color: root.tokens.window
        radius: root.windowCornerRadius
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
    // A different host's page settling in.
    ParallelAnimation {
        id: arrival
        NumberAnimation { target: detailsPane; property: "opacity"; from: root.tokens.reducedMotion ? 1 : 0.4; to: 1; duration: root.tokens.reducedMotion ? 0 : 180; easing.type: Easing.OutCubic }
        NumberAnimation { target: detailsShift; property: "y"; from: root.tokens.reducedMotion ? 0 : 6; to: 0; duration: root.tokens.reducedMotion ? 0 : 180; easing.type: Easing.OutCubic }
    }
    // AdwNavigationView: pages slide over each other when the split view collapses.
    NumberAnimation {
        id: narrowSlide
        target: detailsShift
        property: "x"
        duration: root.tokens.reducedMotion ? 0 : 220
        easing.type: Easing.OutCubic
    }
    onNarrowShowsDetailsChanged: {
        if (!narrow || !narrowShowsDetails || tokens.reducedMotion) return
        narrowSlide.from = width * 0.3
        narrowSlide.to = 0
        narrowSlide.restart()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar: the hosts, the add action and the application menu.
        Rectangle {
            id: hostsPane
            objectName: "gnome-hosts-sidebar-pane"
            // Files' own split view: sidebar-width-fraction 0.2 with a
            // 180–240 range, never narrower than the header's contents.
            Layout.preferredWidth: root.narrow
                                   ? parent.width
                                   : Math.max(sidebarHeaderMinimum.width,
                                              Math.max(200, Math.min(260, parent.width * 0.26)))
            Layout.fillHeight: true
            visible: !root.narrow || !root.detailsFirst
            enabled: !root.narrow || !root.detailsFirst
            color: "transparent"

            Item {
                id: sidebarHeaderMinimum
                // The whole title: GNOME users with left-hand window buttons keep their app name.
                width: sidebarHeaderStart.width + sidebarHeaderEnd.width
                       // Rounded up with a pixel spare: layouts round down, and a title
                       // a fraction too wide for its band elides.
                       + Math.max(root.tokens.windowTitleMinimumWidth, Math.ceil(sidebarTitle.implicitWidth) + 1)
                       + root.tokens.headerbarPaddingLeft + root.tokens.headerbarPaddingRight
                       + 2 * root.tokens.headerbarChildSpacing
            }
            // Sidebar surface, rounding only the outer (left) window corners;
            // inner seam corners are patched (per-corner radii are Qt 6.7+).
            Rectangle {
                id: sidebarSurface
                anchors.fill: parent
                color: root.narrow ? root.tokens.window : root.sidebarSurfaceColor
                radius: root.windowCornerRadius
                Behavior on color { ColorAnimation { duration: root.tokens.motionDuration } }
                Rectangle {
                    visible: parent.radius > 0 && !root.narrow
                    color: parent.color
                    width: parent.radius; height: parent.radius
                    anchors.right: parent.right; anchors.top: parent.top
                }
                Rectangle {
                    visible: parent.radius > 0 && !root.narrow
                    color: parent.color
                    width: parent.radius; height: parent.radius
                    anchors.right: parent.right; anchors.bottom: parent.bottom
                }
                // _sidebars.scss: inset -1px 0 var(--sidebar-border-color).
                Rectangle {
                    objectName: "gnome-hosts-sidebar-divider"
                    visible: !root.narrow
                    width: 1
                    color: root.tokens.sidebarBorder
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: root.tokens.spacingXS

                Item {
                    id: sidebarHeader
                    objectName: "gnome-hosts-sidebar-header"
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.headerHeight

                    DragHandler { target: null; onActiveChanged: if (active) root.startWindowMove() }
                    TapHandler { onDoubleTapped: root.toggleMaximized() }

                    Row {
                        id: sidebarHeaderStart
                        objectName: "gnome-hosts-sidebar-start"
                        opacity: root.headerContentOpacity
                        Behavior on opacity { NumberAnimation { duration: root.tokens.reducedMotion ? 0 : 200; easing.type: Easing.OutCubic } }
                        anchors.left: parent.left
                        anchors.leftMargin: root.tokens.headerbarPaddingLeft
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: root.tokens.headerbarChildSpacing

                        GnomeWindowControls { side: "left" }
                        GnomeHeaderButton {
                            id: addHostButton
                            objectName: "starlight.hosts.add-host"
                            symbol: "plus"
                            accessibleName: qsTranslate("PcView", "Add Host")
                            ToolTip.visible: hovered
                            ToolTip.delay: 600
                            ToolTip.text: accessibleName
                            onClicked: {
                                root.rememberFocusTarget(this)
                                root.page.requestAddHostOverlay()
                            }
                        }
                    }
                    Row {
                        id: sidebarHeaderEnd
                        objectName: "gnome-hosts-sidebar-end"
                        opacity: root.headerContentOpacity
                        Behavior on opacity { NumberAnimation { duration: root.tokens.reducedMotion ? 0 : 200; easing.type: Easing.OutCubic } }
                        anchors.right: parent.right
                        anchors.rightMargin: root.tokens.headerbarPaddingRight
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: root.tokens.headerbarChildSpacing

                        GnomeHeaderButton {
                            id: appMenuButton
                            objectName: "gnome-hosts-app-menu-button"
                            symbol: "open-menu"
                            popupVisible: appMenu.visible
                            accessibleName: qsTranslate("PcView", "Main Menu")
                            onClicked: appMenu.popup(appMenuButton, 0, appMenuButton.height)
                        }
                        GnomeWindowControls { side: root.narrow ? "right" : "none" }
                    }
                    Label {
                        id: sidebarTitle
                        objectName: "gnome-hosts-sidebar-title"
                        opacity: root.headerContentOpacity
                        Behavior on opacity { NumberAnimation { duration: root.tokens.reducedMotion ? 0 : 200; easing.type: Easing.OutCubic } }
                        // AdwHeaderBar's LOOSE centering: centred when it
                        // fits, otherwise moved only as far as it must be,
                        // ellipsizing rather than running through the buttons.
                        // Whichever row is on the left: mirroring swaps them.
                        readonly property Item leftRow: sidebarHeaderStart.x <= sidebarHeaderEnd.x ? sidebarHeaderStart : sidebarHeaderEnd
                        readonly property Item rightRow: leftRow === sidebarHeaderStart ? sidebarHeaderEnd : sidebarHeaderStart
                        readonly property real bandStart: leftRow.x + leftRow.width + root.tokens.headerbarChildSpacing
                        readonly property real bandEnd: rightRow.x - root.tokens.headerbarChildSpacing
                        width: Math.max(0, Math.min(implicitWidth, bandEnd - bandStart))
                        x: Math.max(bandStart, Math.min((parent.width - width) / 2, bandEnd - width))
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.chromeActive ? "Moonwalk" : qsTranslate("PcView", "Hosts")
                        color: root.tokens.text
                        font.pixelSize: root.tokens.bodySize
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                ListView {
                    id: hostList
                    objectName: "starlight.hosts.list"
                    AppStyle.AccessibleIdentity { target: hostList; stableId: hostList.objectName }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    interactive: contentHeight > height
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
                    Accessible.name: qsTranslate("PcView", "Available streaming hosts")
                    Accessible.description: qsTranslate("PcView", "Choose a host and manage its streaming settings")

                    delegate: GnomeSidebarRow {
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
                        showStatus: true
                        statusText: root.presentation.statusFor(online, paired, model.statusUnknown === true, serverSupported, busy)
                        tone: root.presentation.toneFor(online, paired, model.statusUnknown === true, serverSupported, busy)
                        selected: !root.narrow && root.page.selectedHostUuid === uuid
                        hasOpenPopup: root.hostContextSource === hostDelegate && hostContextMenu.opened
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
                                root.openHostContextMenu(hostDelegate.uuid, hostDelegate,
                                                         eventPoint.position.x, eventPoint.position.y)
                            }
                        }
                    }
                    ScrollBar.vertical: GnomeScrollBar { accessibleName: qsTranslate("PcView", "Hosts") }
                }
            }
        }

        // Content: the selected host.
        Item {
            id: detailsPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.narrow || root.detailsFirst
            enabled: !root.narrow || root.detailsFirst
            transform: Translate { id: detailsShift }

            Rectangle {
                id: contentSurface
                objectName: "gnome-hosts-content-surface"
                anchors.fill: parent
                // A preferences-style page sits on --window-bg-color;
                // --view-bg-color equals the card colour in light mode.
                color: root.tokens.window
                radius: root.windowCornerRadius
                Rectangle {
                    visible: parent.radius > 0 && !root.narrow
                    color: parent.color
                    width: parent.radius; height: parent.radius
                    anchors.left: parent.left; anchors.top: parent.top
                }
                Rectangle {
                    visible: parent.radius > 0 && !root.narrow
                    color: parent.color
                    width: parent.radius; height: parent.radius
                    anchors.left: parent.left; anchors.bottom: parent.bottom
                }
            }

            Item {
                id: contentHeader
                objectName: "gnome-hosts-content-header"
                opacity: root.headerContentOpacity
                Behavior on opacity { NumberAnimation { duration: root.tokens.reducedMotion ? 0 : 200; easing.type: Easing.OutCubic } }
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: root.headerHeight

                DragHandler { target: null; onActiveChanged: if (active) root.startWindowMove() }
                TapHandler { onDoubleTapped: root.toggleMaximized() }

                Label {
                    objectName: "gnome-hosts-content-title"
                    // The page's heading says this until it scrolls away.
                    opacity: root.titleInHeader ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: root.tokens.motionDuration } }
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 2 * contentHeaderEnd.width - 2 * contentHeaderStart.width)
                    text: root.selected.name || (root.chromeActive ? "Moonwalk" : qsTranslate("PcView", "Hosts"))
                    color: root.tokens.text
                    font.pixelSize: root.tokens.bodySize
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }
                Row {
                    id: contentHeaderStart
                    anchors.left: parent.left
                    anchors.leftMargin: root.tokens.headerbarPaddingLeft
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: root.tokens.headerbarChildSpacing

                    GnomeWindowControls { side: root.narrow ? "left" : "none" }
                    GnomeHeaderButton {
                        visible: root.narrow && root.sidebarCount === 0
                        symbol: "plus"
                        accessibleName: qsTranslate("PcView", "Add Host")
                        onClicked: root.page.requestAddHostOverlay()
                    }
                    GnomeNavigationButton {
                        objectName: "gnome-hosts-back"
                        visible: root.narrow && root.narrowShowsDetails && root.sidebarCount > 0
                        symbol: "previous"
                        accessibleName: qsTranslate("PcView", "Back")
                        onNavigationRequested: {
                            root.narrowShowsDetails = false
                            hostList.forceActiveFocus(Qt.TabFocusReason)
                        }
                    }
                }
                Row {
                    id: contentHeaderEnd
                    anchors.right: parent.right
                    anchors.rightMargin: root.tokens.headerbarPaddingRight
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: root.tokens.headerbarChildSpacing

                    GnomeHeaderButton {
                        id: emptyAppMenuButton
                        objectName: "gnome-hosts-empty-app-menu-button"
                        symbol: "open-menu"
                        visible: root.sidebarCount === 0
                        popupVisible: appMenu.visible
                        accessibleName: qsTranslate("PcView", "Main Menu")
                        onClicked: appMenu.popup(emptyAppMenuButton, 0, emptyAppMenuButton.height)
                    }
                    GnomeHeaderButton {
                        id: hostMenuButton
                        objectName: "gnome-hosts-host-menu-button"
                        symbol: "view-more"
                        visible: !!root.selected.uuid
                        popupVisible: hostMenu.visible
                        accessibleName: qsTranslate("PcView", "Host Actions")
                        onClicked: hostMenu.popup(hostMenuButton, 0, hostMenuButton.height)
                    }
                    GnomeWindowControls { side: "right" }
                }
            }

            GnomeMenu {
                id: appMenu
                objectName: "gnome-hosts-app-menu"
                accessibleName: qsTranslate("PcView", "Main Menu")
                GnomeMenuItem {
                    objectName: "starlight.hosts.connection-test"
                    text: qsTranslate("PcView", "Test Internet Streaming Ports…")
                    onClicked: {
                        root.rememberFocusTarget(appMenuButton)
                        root.page.testConnection()
                    }
                }
                GnomeMenuItem {
                    objectName: "gnome-diagnostics-action"
                    text: qsTranslate("PcView", "Copy Diagnostic Summary")
                    onClicked: {
                        if (typeof window !== "undefined" && window.copyDiagnosticSummary)
                            window.copyDiagnosticSummary()
                    }
                }
                MenuSeparator {
                    objectName: "gnome-hosts-app-menu-separator"
                    contentItem: Rectangle { implicitHeight: 1; color: root.tokens.border }
                    background: null
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.open-settings"
                    text: qsTranslate("SettingsView", "Preferences")
                    onClicked: root.page.requestOpenSettings()
                }
                GnomeMenuItem {
                    objectName: "gnome-keyboard-shortcuts-action"
                    text: qsTranslate("KeyboardShortcuts", "Keyboard Shortcuts")
                    onClicked: shortcutsDialog.open()
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.about"
                    text: qsTranslate("PcView", "About Moonwalk")
                    onClicked: aboutDialog.open()
                }
                GnomeMenuItem {
                    objectName: "gnome-hosts-quit"
                    // Compositors without window buttons still need a visible way out.
                    visible: !root.chromeActive
                    text: qsTranslate("PcView", "Quit")
                    onClicked: Qt.quit()
                }
            }
            GnomeMenu {
                id: hostMenu
                objectName: "gnome-hosts-host-menu"
                accessibleName: qsTranslate("PcView", "Host Actions")
                GnomeMenuItem {
                    objectName: "starlight.hosts.details"
                    text: qsTranslate("PcView", "Host Details")
                    enabled: !!root.selected.uuid
                    onClicked: root.showSelectedHostDetails()
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.rename"
                    text: qsTranslate("PcView", "Rename…")
                    enabled: !!root.selected.uuid
                    onClicked: root.renameSelectedHost()
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.resume-running"
                    text: qsTranslate("PcView", "Resume Running App")
                    visible: root.presentation.sessionActionsAvailable && !root.presentation.resumeDesktop
                    enabled: visible && root.selected.serverSupported === true
                    onClicked: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestResume(root.page.selectedHostUuid) }
                }
                MenuSeparator {
                    contentItem: Rectangle { implicitHeight: 1; color: root.tokens.border }
                    background: null
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.end-session"
                    text: qsTranslate("PcView", "End Remote Session…")
                    visible: root.presentation.sessionActionsAvailable
                    enabled: visible
                    destructive: true
                    onClicked: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestEndSession(root.page.selectedHostUuid) }
                }
                GnomeMenuItem {
                    objectName: "starlight.hosts.delete"
                    text: qsTranslate("PcView", "Remove Host")
                    destructive: true
                    enabled: !!root.selected.uuid
                    onClicked: root.removeSelectedHost()
                }
            }
            GnomeMenu {
                id: hostContextMenu
                objectName: "gnome-host-context-menu"
                accessibleName: qsTranslate("PcView", "Host Actions")
                onClosed: root.hostContextSource = null
                GnomeMenuItem { text: qsTranslate("PcView", "Host Details"); onClicked: root.showSelectedHostDetails() }
                GnomeMenuItem { text: qsTranslate("PcView", "Rename…"); onClicked: root.renameSelectedHost() }
                GnomeMenuItem {
                    text: qsTranslate("PcView", "Resume Running App")
                    visible: root.presentation.sessionActionsAvailable && !root.presentation.resumeDesktop
                    enabled: visible && root.selected.serverSupported === true
                    onClicked: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestResume(root.page.selectedHostUuid) }
                }
                MenuSeparator {
                    contentItem: Rectangle { implicitHeight: 1; color: root.tokens.border }
                    background: null
                }
                GnomeMenuItem {
                    text: qsTranslate("PcView", "End Remote Session…")
                    visible: root.presentation.sessionActionsAvailable
                    enabled: visible
                    destructive: true
                    onClicked: { root.rememberFocusTarget(root.selectedHostItem()); root.page.requestEndSession(root.page.selectedHostUuid) }
                }
                GnomeMenuItem { text: qsTranslate("PcView", "Remove Host"); destructive: true; onClicked: root.removeSelectedHost() }
            }
            GnomeShortcutsDialog { id: shortcutsDialog }
            GnomeAboutDialog {
                id: aboutDialog
                applicationVersion: root.page.hostPreferenceSnapshot.starlightVersionString
                                    || qsTranslate("PcView", "Development build")
            }

            // No hosts yet: first launch, or nothing discovered.
            Item {
                id: emptyState
                objectName: "starlight.hosts.empty-state"
                anchors.fill: parent
                anchors.topMargin: root.headerHeight
                visible: root.sidebarCount === 0
                enabled: root.sidebarCount === 0
                readonly property string emptyMessage: root.presentation.emptyTitle
                readonly property string guidance: root.presentation.emptyBody

                Accessible.role: Accessible.StaticText
                Accessible.name: emptyMessage
                Accessible.description: guidance

                GnomeStatusPage {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * root.tokens.spacingXL, 420)
                    iconName: "monitor"
                    pulsing: root.presentation.stillSearching
                    title: emptyState.emptyMessage
                    description: emptyState.guidance
                    GnomeButton {
                        Layout.alignment: Qt.AlignHCenter
                        objectName: "gnome-hosts-empty-add"
                        pill: true
                        suggested: !root.presentation.stillSearching
                        text: qsTranslate("PcView", "Add Host…")
                        onClicked: {
                            root.rememberFocusTarget(this)
                            root.page.requestAddHostOverlay()
                        }
                    }
                }
            }

            // A host with nothing to configure yet: one message, one action.
            Item {
                id: statusState
                objectName: "gnome-host-status-page"
                anchors.fill: parent
                anchors.topMargin: root.headerHeight
                visible: root.sidebarCount > 0 && !root.presentation.settingsVisible && root.presentation.hasHost

                GnomeStatusPage {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * root.tokens.spacingXL, 420)
                    busy: root.presentation.checking
                    iconName: root.presentation.iconName
                    seed: root.selected.uuid || ""
                    dimmed: !root.presentation.online
                    title: root.presentation.statusPageTitle
                    description: root.presentation.statusPageBody
                    GnomeButton {
                        objectName: "starlight.hosts.pair"
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTranslate("PcView", "Pair")
                        accessibleName: text
                        suggested: true
                        pill: true
                        visible: root.presentation.pairVisible
                        enabled: visible
                        onClicked: {
                            root.rememberFocusTarget(this)
                            root.page.beginPairing()
                        }
                    }
                    GnomeButton {
                        objectName: "gnome-hosts-status-wake"
                        Layout.alignment: Qt.AlignHCenter
                        text: root.presentation.wakeLabel
                        accessibleName: text
                        pill: true
                        suggested: root.page.waitingForWake !== true
                        visible: root.presentation.wakeVisible && !root.presentation.paired
                        onClicked: { if (root.page.waitingForWake === true) root.page.cancelWakeConnect(); else root.page.wakeHost(root.page.selectedHostUuid, true) }
                    }
                }
            }

            // Undershoot indicator: content scrolled behind a flat top bar
            // gets a 1px inset line of --shade-color at 75% plus a 4px fade
            // (_drawing.scss undershoot mixin).
            Item {
                objectName: "gnome-hosts-content-undershoot"
                anchors { top: parent.top; topMargin: root.headerHeight; left: parent.left; right: parent.right }
                height: 5
                z: 2
                // Logical condition, not effective visibility: the latter is
                // false whenever an ancestor is hidden (invisible test roots).
                readonly property bool shown: root.sidebarCount > 0 && root.presentation.settingsVisible
                                              && detailsFlickable.contentY > 0
                visible: shown
                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: root.tokens.undershoot
                }
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

            Flickable {
                id: detailsFlickable
                objectName: "gnome-hosts-content-flickable"
                anchors.fill: parent
                anchors.topMargin: root.headerHeight
                visible: root.sidebarCount > 0 && root.presentation.settingsVisible
                contentWidth: width
                contentHeight: detailsColumn.implicitHeight + 2 * root.tokens.spacingXL
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: GnomeScrollBar { accessibleName: qsTranslate("PcView", "Hosts") }

                ColumnLayout {
                    id: detailsColumn
                    x: Math.max(root.tokens.spacingM, (detailsFlickable.width - width) / 2)
                    y: root.tokens.spacingM
                    width: Math.min(root.tokens.contentClamp, detailsFlickable.width - 2 * root.tokens.spacingM)
                    spacing: root.tokens.spacingXL

                    // Identity, the one thing to do next, and what it will do.
                    GnomeHostHero {
                        id: hero
                        Layout.fillWidth: true
                        page: root.page
                        presentation: root.presentation
                        onAboutToAct: function(source) { root.rememberFocusTarget(source) }
                    }

                    GnomeBanner {
                        objectName: "starlight.hosts.unsupported-server"
                        Layout.fillWidth: true
                        tone: "warning"
                        text: root.presentation.unsupportedNote
                    }
                    GnomeBanner {
                        objectName: "gnome-host-waking-note"
                        Layout.fillWidth: true
                        busy: true
                        text: root.presentation.wakingNote
                    }
                    GnomeBanner {
                        objectName: "gnome-host-running-note"
                        Layout.fillWidth: true
                        text: root.presentation.runningNote
                        actionText: root.presentation.resumeRunningLabel
                        onActionTriggered: {
                            root.rememberFocusTarget(root.selectedHostItem())
                            root.page.requestResume(root.page.selectedHostUuid)
                        }
                    }

                    GnomeSettingsList {
                        objectName: "gnome-host-preferences"
                        Layout.fillWidth: true
                        groups: root.presentation.groups
                        enabled: root.page.selectedHostUuid.length > 0
                    }
                }
            }
        }
    }

    // GTK's primary-menu key.
    Shortcut {
        sequences: ["F10"]
        onActivated: {
            var source = root.sidebarCount === 0 ? emptyAppMenuButton : appMenuButton
            appMenu.popup(source, 0, source.height)
        }
    }
    Shortcut {
        // One sequence only: Qt maps the keyboard layout to it, and a second match for
        // the same press makes the shortcut ambiguous, so it would never activate.
        sequence: "Ctrl+?"
        onActivated: shortcutsDialog.open()
    }

    GnomeHostDialogLayer {
        id: dialogs
        objectName: "gnome-host-dialog-layer"
        page: root.page
        focusReturnTarget: root.focusReturnTarget || hostList
    }
}
