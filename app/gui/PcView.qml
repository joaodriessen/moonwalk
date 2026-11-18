import QtQuick 2.9
import "adaptive/HostError.js" as HostError
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import SdlGamepadKeyNavigation 1.0

import Starlight 1.0
import Starlight.Application 1.0
import "starlight"
import "starlight/ControlCompat.js" as ControlCompat
import "starlight/Lucide.js" as Lucide

// Starlight's HIG host list + per-host page, per the approved mockup at
// docs/design/2026-07-13-hig-mockup.html. Replaces the old CenteredGridView
// grid of PC tiles with a sidebar of hosts and an inline settings page for
// whichever host is selected.
//
// Per-host connection settings are supplied as detached presentation values
// and changed only through the injected page facade.
Item {
    id: pcView

    // This painted surface is intentionally renderer-only. HostPageRoute is
    // the sole owner of backend objects and injects this facade.
    required property var page
    objectName: "starlight.hosts.page"
    property string shellRouteId: "legacy-host"
    readonly property bool shellRouteActive:
        StackView.status === StackView.Active

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Hosts")
    Accessible.description: qsTr("Choose a host and manage its streaming settings")

    AccessibleIdentity {
        target: pcView
        stableId: pcView.objectName
    }

    focus: true

    // Keyboard a11y: scroll the host page to whatever control takes focus,
    // same block SettingsView already has — without it, Tab-walking this
    // page focuses controls that remain scrolled out of view.
    function isChildOfFlickable(item) {
        while (item) {
            if (item.parent === contentFlick.contentItem) {
                return true
            }
            item = item.parent
        }
        return false
    }

    NumberAnimation {
        id: autoScrollAnimation
        target: contentFlick
        property: "contentY"
        duration: 100
    }

    Window.onActiveFocusItemChanged: {
        var item = Window.activeFocusItem
        if (item) {
            if (!isChildOfFlickable(item)) {
                return
            }

            var pos = item.mapToItem(contentFlick.contentItem, 0, 0)
            var scrollMargin = contentFlick.height > 100 ? 50 : 0

            if (pos.y - scrollMargin < contentFlick.contentY) {
                autoScrollAnimation.from = contentFlick.contentY
                autoScrollAnimation.to = Math.max(pos.y - scrollMargin, 0)
                autoScrollAnimation.start()
            }
            else if (pos.y + item.height + scrollMargin > contentFlick.contentY + contentFlick.height) {
                autoScrollAnimation.from = contentFlick.contentY
                autoScrollAnimation.to = Math.min(pos.y + item.height + scrollMargin - contentFlick.height,
                                                  contentFlick.contentHeight - contentFlick.height)
                autoScrollAnimation.start()
            }
        }
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    function routeActivated() {
        sidebarList.forceActiveFocus()
        pcView.updateCurrentHost()
    }

    Connections {
        target: page
        function onSelectedHostIndexChanged() {
            if (sidebarList.currentIndex !== page.selectedHostIndex)
                sidebarList.currentIndex = page.selectedHostIndex
        }
    }

    function statusText(cur) {
        if (!cur) {
            return ""
        }
        if (cur.hostStatusUnknown) {
            return qsTr("Checking…")
        }
        if (!cur.hostOnline) {
            return qsTr("Offline")
        }
        return qsTr("Online") + " · " + (cur.hostPaired ? qsTr("Paired") : qsTr("Not paired"))
    }

    function nativeResolution() {
        var r = page.resolutionOptions.length > 0
                ? page.resolutionOptions[0] : ({ w: 1920, h: 1080 })
        return {
            width: r.w > 0 ? r.w : 1920,
            height: r.h > 0 ? r.h : 1080
        }
    }

    function setResolution(w, h) {
        page.setResolution(w, h)
    }

    function setFps(f) {
        page.setFrameRate(f)
    }

    function resolutionLabel() {
        var n = nativeResolution()
        var preferences = page.hostPreferenceSnapshot
        if (preferences.width === n.width && preferences.height === n.height) {
            return qsTr("Native")
        }
        return preferences.width + "×" + preferences.height
    }

    function resolutionSublabel() {
        var n = nativeResolution()
        var preferences = page.hostPreferenceSnapshot
        if (preferences.width === n.width && preferences.height === n.height) {
            return qsTr("Matches this display exactly — %1×%2").arg(n.width).arg(n.height)
        }
        return qsTr("Custom — %1×%2").arg(preferences.width).arg(preferences.height)
    }

    // ── Resolution/frame-rate SLDropdown option lists (interaction H1/H2/H3):
    // replaces the raw Menu/MenuItem "chip" popups, which were unthemed,
    // keyboard-unreachable, and had no hover feedback. "Custom…" stays the
    // last model entry and still opens the existing custom-value dialogs --
    // onActivated below preserves the exact setResolution()/setFps() calls. ──

    // The page offers only sizes this display can show; its Automatic entry
    // is the native size here.
    readonly property var resolutionOptionsList: page.resolutionOptions.filter(function(option) {
        return !option.automatic
    }).map(function(option) {
        var nativeSize = nativeResolution()
        var text = option.w === 0 ? qsTr("Custom…")
                 : option.w === nativeSize.width && option.h === nativeSize.height ? qsTr("Native (%1×%2)").arg(option.w).arg(option.h)
                 : qsTr("%1×%2").arg(option.w).arg(option.h)
        return ({ text: text, w: option.w, h: option.h })
    })

    function resolutionOptionIndex() {
        var opts = resolutionOptionsList
        for (var i = 0; i < opts.length - 1; i++) {
            if (page.hostPreferenceSnapshot.width === opts[i].w &&
                    page.hostPreferenceSnapshot.height === opts[i].h) {
                return i
            }
        }
        return opts.length - 1 // Custom -- current resolution matches no preset
    }

    // Only rates this display shows, as the page offers them.
    readonly property var fpsOptionsList: page.frameRateOptions.filter(function(option) {
        return !option.automatic
    }).map(function(option) {
        var text = option.value > 0 ? qsTr("%1 fps").arg(option.value) : qsTr("Custom…")
        return ({ text: text, value: option.value })
    })

    function fpsOptionIndex() {
        var opts = fpsOptionsList
        for (var i = 0; i < opts.length - 1; i++) {
            if (page.hostPreferenceSnapshot.fps === opts[i].value) {
                return i
            }
        }
        return opts.length - 1 // Custom
    }

    function indexForValue(list, value) {
        for (var i = 0; i < list.length; i++) {
            if (list[i].value === value) {
                return i
            }
        }
        return -1
    }

    // The facade has no option arrays for these closed enums. Keep their
    // production scalar values local to this painted compatibility surface.
    readonly property int codecAutomatic: 0
    readonly property int codecH264: 1
    readonly property int codecHevc: 2
    readonly property int codecAv1: 4

    readonly property var codecOptions: [
        { text: qsTr("Automatic (Recommended)"), value: codecAutomatic },
        { text: qsTr("H.264"), value: codecH264 },
        { text: qsTr("HEVC (H.265)"), value: codecHevc },
        { text: qsTr("AV1"), value: codecAv1 }
    ]

    readonly property var selectedHostPresentation: {
        var snapshot = page.selectedHostSnapshot
        if (!snapshot || !snapshot.uuid)
            return null
        return ({
            hostOnline: snapshot.online,
            hostPaired: snapshot.paired,
            hostBusy: snapshot.busy,
            hostWakeable: snapshot.wakeable,
            hostStatusUnknown: snapshot.statusUnknown,
            hostServerSupported: snapshot.serverSupported,
            hostName: snapshot.name,
            hostAddress: snapshot.address,
            hostDetails: snapshot.details,
            hostUuid: snapshot.uuid,
            hostOs: snapshot.hostOs,
            hostServerSoftware: snapshot.serverSoftware
        })
    }

    // ── Host OS: fully auto-detected (serverinfo <HostOs> tag from
    // Skylight-family hosts, GFE=>Windows inference for GeForce Experience;
    // see NvComputer). The manual Advanced-settings override was retired
    // 2026-07-17 -- detection covers every supported host family. ──

    function osDisplayName(os) {
        switch (os) {
            case "macos": return "macOS"
            case "windows": return "Windows"
            case "linux": return "Linux"
            default: return ""
        }
    }

    function osIconName(os) {
        switch (os) {
            case "macos": return "os_macos"
            case "windows": return "os_windows"
            case "linux": return "os_linux"
            default: return "monitor"
        }
    }

    // Overlay state is presentation data from the route. Keeping this bridge
    // beside the unchanged dialogs preserves their geometry and focus while
    // making the context the only pairing owner.
    function syncBaseOverlay() {
        var overlay = page.baseOverlay
        var payload = overlay.payload || ({})
        if (overlay.kind === "pairing") {
            pairSheet.pin = payload.pin || ""
            pairSheet.hostName = payload.hostName || ""
            if (!pairSheet.visible)
                pairSheet.open()
        } else if (overlay.kind === "confirm-quit" || overlay.kind === "confirm-end") {
            quitAppDialog.endOnly = overlay.kind === "confirm-end"
            quitAppDialog.operationId = overlay.operationId
            quitAppDialog.appName = payload.runningAppLabel || ""
            if (!quitAppDialog.visible)
                quitAppDialog.open()
        } else if (overlay.kind === "confirm-delete") {
            deletePcDialog.operationId = overlay.operationId
            deletePcDialog.pcName = payload.name || ""
            if (!deletePcDialog.visible)
                deletePcDialog.open()
        } else if (overlay.kind === "rename-host") {
            renamePcDialog.operationId = overlay.operationId
            renamePcDialog.originalName = payload.name || ""
            if (!renamePcDialog.visible)
                renamePcDialog.open()
        } else if (overlay.kind === "details") {
            if (showPcDetailsDialog.operationId !== overlay.operationId) {
                detailsAddress.text = String(payload.connectionAddress || "")
                showPcDetailsDialog.addressStatus = ""
            }
            showPcDetailsDialog.operationId = overlay.operationId
            showPcDetailsDialog.pcDetails = payload.details || ""
            if (!showPcDetailsDialog.visible)
                showPcDetailsDialog.open()
        } else if (overlay.kind === "connection-test") {
            testConnectionDialog.operationId = overlay.operationId
            if (!testConnectionDialog.visible)
                testConnectionDialog.open()
            if (payload.dispatched === undefined)
                testConnectionDialog.connectionTestComplete(payload.result, payload.blockedPorts || "")
        } else if (overlay.kind === "custom-resolution") {
            customResDialog.operationId = overlay.operationId
            customResDialog.currentWidth = payload.width || page.hostPreferenceSnapshot.width
            customResDialog.currentHeight = payload.height || page.hostPreferenceSnapshot.height
            if (!customResDialog.visible)
                customResDialog.open()
        } else if (overlay.kind === "custom-frame-rate") {
            customFpsDialog.operationId = overlay.operationId
            customFpsDialog.currentFps = payload.fps || page.hostPreferenceSnapshot.fps
            if (!customFpsDialog.visible)
                customFpsDialog.open()
        }

        if (overlay.kind !== "pairing" && pairSheet.visible)
            pairSheet.close()
        if (overlay.kind !== "confirm-quit" && overlay.kind !== "confirm-end" && quitAppDialog.visible)
            quitAppDialog.close()
        if (overlay.kind !== "confirm-delete" && deletePcDialog.visible)
            deletePcDialog.close()
        if (overlay.kind !== "rename-host" && renamePcDialog.visible)
            renamePcDialog.close()
        if (overlay.kind !== "details" && showPcDetailsDialog.visible)
            showPcDetailsDialog.close()
        if (overlay.kind !== "connection-test" && testConnectionDialog.visible)
            testConnectionDialog.close()
        if (overlay.kind !== "custom-resolution" && customResDialog.visible)
            customResDialog.close()
        if (overlay.kind !== "custom-frame-rate" && customFpsDialog.visible)
            customFpsDialog.close()
    }

    Connections {
        target: page
        function onBaseOverlayChanged() { pcView.syncBaseOverlay() }
        function onInterruptOverlayChanged() {
            var overlay = page.interruptOverlay
            if (overlay.kind === "error") {
                errorDialog.text = HostError.message(overlay.payload)
                if (overlay.payload.code === "add-host-unavailable")
                    errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
                if (overlay.payload.code !== "add-host-unavailable")
                    errorDialog.helpText = ""
                if (!errorDialog.visible)
                    errorDialog.open()
            } else if (errorDialog.visible) {
                errorDialog.close()
            }
        }
    }

    Component.onCompleted: syncBaseOverlay()

    function updateCurrentHost() {
        var uuid = sidebarList.currentItem ? sidebarList.currentItem.hostUuid : ""
        page.selectHostByUuid(uuid)
    }

    // ── Host-adaptive presentation (Part 2 of the adaptive-settings design,
    // 2026-07-18): "skylight" | "stock" | "unknown", derived from the SAME
    // the facade's selected-host snapshot -- no new capability tags, per
    // docs/design/settings-ownership.md's deferred-backlog note.
    // Only the PRESENTATION adapts (host card cursor spec-line label +
    // the Instant-cursor reveal sublabel); no control is ever hidden by host
    // kind. An unknown/future host (no serverSoftware reported) degrades to
    // the same wording as a stock host -- conservative, never a broken or
    // over-promising UI.
    readonly property string currentHostKind: page.selectedHostKind

    // ── Host card spec-line codec label: short label mirroring the same
    // videoCodecConfig value the Video group's dropdown below reads/writes.
    // ──
    // Returns "" for automatic codec selection (owner ruling 2026-07-19:
    // "Auto" told an end user nothing) -- the spec line filters empty parts,
    // so the chip appears only when a codec is explicitly forced.
    function codecChipLabel() {
        switch (page.hostPreferenceSnapshot.videoCodecConfig) {
            case codecH264: return qsTr("H.264")
            case codecHevc: return qsTr("HEVC")
            case codecAv1: return qsTr("AV1")
            default: return ""
        }
    }

    // ── Pairing ──

    function beginPairing() {
        page.beginPairing()
    }

    // ── Connect: launches the host's Desktop session directly, no app-grid step ──
    //
    // The "applications" concept (AppView.qml's grid) is gone from this UI --
    // Starlight is a remote-desktop tool, not a game launcher front-end. The
    // host's Desktop stream target is just an app like any other in its app
    // list (the Sunshine/GFE convention names it literally "Desktop"), so we
    // resolve it directly instead of pushing the grid for the user to pick it.
    //
    // Gating mirrors AppView's launchOrResumeSelectedApp exactly: if a
    // different app/stream is already active on the host, confirm before
    // quitting it (quitAppDialog below); if the Desktop session itself is
    // already running, this is a resume, not a fresh launch.

    // Quick-connect for a specific sidebar row (hover play button and
    // double-click): select the row first — onCurrentIndexChanged pins that
    // host's preferences via updateCurrentHost() — then run the same gated
    // connect path the host card's button uses.
    function connectToHostAtIndex(index) {
        sidebarList.currentIndex = index
        connectToCurrentHost()
    }

    // Quick-wake for a specific sidebar row. UUID is the only operation
    // identity; the row index remains a local visual cursor.
    function wakeHostAtIndex(index, hostName, hostUuid) {
        page.wakeHost(hostUuid)
    }

    function connectToCurrentHost() {
        page.requestConnect(page.selectedHostUuid)
    }

    readonly property string qualityMode: !page.hostPreferenceSnapshot.autoAdjustBitrate ? "manual"
                                          : page.hostPreferenceSnapshot.adaptiveBitrate === true
                                            && page.selectedHostSnapshot.adaptiveBitrateSupported === true
                                            ? "adaptive" : "recommended"
    readonly property var qualityOptions: {
        var options = page.selectedHostSnapshot.adaptiveBitrateSupported === true
            ? [{ text: qsTr("Automatic (recommended)"), value: "adaptive" }]
            : [{ text: qsTr("Recommended"), value: "recommended" }]
        options.push({ text: qsTr("Manual"), value: "manual" })
        return options
    }


    // Full-bleed page ground: the app's ApplicationWindow keeps moonlight's
    // Material-dark background, so without this the Theme's light/dark tokens
    // float on whatever that theme paints (seen live: light cards on a dark
    // window). The page must own its ground in BOTH palettes.
    Rectangle {
        anchors.fill: parent
        color: Theme.isDark ? "#202024" : "#F6F6F8"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ──
        Rectangle {
            id: sidebar
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.sidebar

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.hairline
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 2

                Text {
                    text: qsTr("Hosts")
                    color: Theme.label3
                    font.pixelSize: Theme.sizeFootnote
                    font.weight: Font.DemiBold
                    Layout.topMargin: 4
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 2
                }

                ListView {
                    id: sidebarList
                    objectName: "starlight.hosts.list"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    focus: true
                    activeFocusOnTab: true
                    currentIndex: -1
                    model: page.hosts
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 2

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Hosts")
                    Accessible.description: qsTr("Available streaming hosts")
                    Accessible.focusable: sidebarList.enabled && sidebarList.visible && sidebarList.activeFocusOnTab
                    Accessible.focused: sidebarList.activeFocus

                    AccessibleIdentity {
                        target: sidebarList
                        stableId: sidebarList.objectName
                    }

                    ScrollBar.vertical: SLScrollBar {
                        objectName: "starlight.hosts.scrollbar"
                        accessibleName: qsTr("Hosts list scroll bar")
                        accessibleDescription: qsTr("Scrolls through available streaming hosts")
                    }

                    onCountChanged: {
                        if (currentIndex === -1 && count > 0) {
                            currentIndex = 0
                        }
                    }

                    // Re-point the facade selection every time the visual
                    // cursor changes (including the initial -1 -> 0 landing).
                    onCurrentIndexChanged: {
                        pcView.updateCurrentHost()

                        // Item 11: jump the host page back to the top on
                        // every selection change, so it's never ambiguous
                        // which host's settings are currently on screen.
                        contentFlick.contentY = 0
                    }

                    Keys.onUpPressed: decrementCurrentIndex()
                    Keys.onDownPressed: incrementCurrentIndex()
                    Keys.onMenuPressed: {
                        if (currentItem) {
                            currentItem.openContextMenu()
                        }
                    }
                    Keys.onDeletePressed: {
                        // Reuse the same confirmation flow as the "Remove Host"
                        // context menu item (see navContextMenuLoader below).
                        if (currentItem) {
                            page.requestDelete(currentItem.hostUuid)
                        }
                    }
                    // Keyboard/gamepad equivalent of the row's hover quick-action
                    // (and mirrors onDoubleClicked's routing below): connect if
                    // connectable, else wake if wakeable. Without this, arrow-key
                    // navigation to a row has no way to trigger its primary action
                    // short of tabbing all the way out to the host card's button.
                    Keys.onReturnPressed: sidebarList.activateCurrentItem()
                    Keys.onEnterPressed: sidebarList.activateCurrentItem()
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Space) {
                            sidebarList.activateCurrentItem()
                            event.accepted = true
                        }
                    }

                    function activateCurrentItem() {
                        if (!currentItem) {
                            return
                        }
                        if (currentItem.hostOnline && currentItem.hostPaired) {
                            pcView.connectToHostAtIndex(currentItem.hostIndex)
                        }
                        else if (!currentItem.hostOnline && currentItem.hostWakeable) {
                            pcView.wakeHostAtIndex(currentItem.hostIndex, currentItem.hostName,
                                                   currentItem.hostUuid)
                        }
                    }

                    delegate: Item {
                        id: navItem
                        // UUID is the stable host identity; the model's row
                        // index can change as discovery updates or reorders
                        // entries, so it must not leak into the public ID.
                        objectName: "starlight.hosts.item." + model.uuid
                        width: sidebarList.width
                        // Single-line row: icon + name + selection only. Status
                        // now lives solely on the host card (design doc
                        // 2026-07-18-host-card.md §4) -- the sidebar's job is
                        // "which hosts exist and which is selected," full stop.
                        // 34px is a comfortable pointer/gamepad row now that a
                        // second status line no longer needs the extra height.
                        height: 34

                        readonly property bool hostOnline: model.online
                        readonly property bool hostPaired: model.paired
                        readonly property bool hostBusy: model.busy
                        readonly property bool hostWakeable: model.wakeable
                        readonly property bool hostStatusUnknown: model.statusUnknown
                        readonly property bool hostServerSupported: model.serverSupported
                        readonly property string hostName: model.name
                        readonly property string hostAddress: model.address
                        readonly property string hostDetails: model.details
                        readonly property int hostIndex: index
                        readonly property string hostUuid: model.uuid
                        // Older role sources may omit optional presentation
                        // strings; scalar guards preserve a stable renderer.
                        readonly property string hostOs: model.hostOs !== undefined ? model.hostOs : ""
                        readonly property string hostServerSoftware: model.serverSoftware !== undefined ? model.serverSoftware : ""
                        readonly property string hostServerSoftwareLabel: model.serverSoftwareLabel !== undefined ? model.serverSoftwareLabel : ""

                        readonly property bool isSelected: sidebarList.currentIndex === index

                        Accessible.role: Accessible.ListItem
                        Accessible.name: hostName
                        Accessible.description: pcView.statusText(navItem)
                        Accessible.selectable: true
                        Accessible.selected: isSelected
                        Accessible.onPressAction: {
                            if (navItem.enabled && navItem.visible) {
                                sidebarList.currentIndex = navItem.hostIndex
                                sidebarList.forceActiveFocus()
                            }
                        }

                        AccessibleIdentity {
                            target: navItem
                            stableId: navItem.objectName
                        }

                        function openContextMenu() {
                            // navContextMenuLoader is asynchronous: false (see below), so
                            // .item is guaranteed to exist by the time this delegate is
                            // interactive -- no need to dereference-guard or toggle the
                            // Loader's asynchronous property here. (That toggle used to be
                            // the fix, but flipping `asynchronous` mid-load doesn't force an
                            // in-flight async load to finish synchronously per Qt semantics,
                            // so the very first right-click on a freshly-shown row could
                            // still no-op.)
                            // Anchor to the ROW, not the pointer: popup() with no
                            // arguments places the menu at the global cursor, which
                            // (a) Wayland/VNC platforms can't always resolve and
                            // (b) reads messier than a consistent macOS-style
                            // row-anchored sheet. popup(parent, x, y) is
                            // deterministic everywhere.
                            navContextMenuLoader.active = true
                            if (navContextMenuLoader.item.popup) {
                                navContextMenuLoader.item.popup(navItem, 14, navItem.height - 4)
                            }
                            else {
                                navContextMenuLoader.item.open()
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 7
                            color: navItem.isSelected ? Theme.accent : (hoverArea.containsMouse ? Theme.segBg : "transparent")
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            // OS icon slot (item 10): identity glyph for this
                            // host, always shown regardless of online status
                            // (connectivity is now conveyed by the line-2
                            // status text below instead of swapping this icon).
                            // Pre-tinted asset picked per state (SLIcon): white on the
                            // selected row's accent fill, palette ink otherwise. No
                            // runtime MultiEffect recoloring — the shader layer rendered
                            // the glyph invisible on non-GPU render paths.
                            SLIcon {
                                name: pcView.osIconName(navItem.hostOs)
                                tone: navItem.isSelected ? "onAccent" : "ink"
                                size: 18
                                Layout.alignment: Qt.AlignVCenter
                                opacity: navItem.isSelected ? 1.0 : 0.85
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: navItem.hostName
                                elide: Text.ElideRight
                                color: navItem.isSelected ? Theme.accentInk : Theme.label
                                font.pixelSize: Theme.sizeBody
                            }
                        }

                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                sidebarList.forceActiveFocus()
                                if (mouse.button === Qt.RightButton) {
                                    navItem.openContextMenu()
                                }
                                else {
                                    sidebarList.currentIndex = navItem.hostIndex
                                }
                            }
                            onPressAndHold: navItem.openContextMenu()
                            onDoubleClicked: {
                                if (mouse.button !== Qt.LeftButton) {
                                    return
                                }
                                if (navItem.hostOnline && navItem.hostPaired) {
                                    pcView.connectToHostAtIndex(navItem.hostIndex)
                                }
                                else if (!navItem.hostOnline && navItem.hostWakeable) {
                                    pcView.wakeHostAtIndex(navItem.hostIndex, navItem.hostName,
                                                           navItem.hostUuid)
                                }
                            }
                        }

                        Loader {
                            id: navContextMenuLoader
                            // Synchronous: the menu is a handful of MenuItems, not
                            // worth the async-load complexity, and async here is what
                            // caused the first-right-click no-op (see openContextMenu()
                            // above) -- toggling `asynchronous` mid-load doesn't force
                            // an in-flight load to finish per Qt semantics, so the
                            // Loader has to just not be async in the first place.
                            asynchronous: false
                            // Lazy: built on first right-click, not at row creation
                            // (openContextMenu() flips this before dereferencing .item —
                            // safe because the load is synchronous).
                            active: false
                            sourceComponent: NavigableMenu {
                                initiator: navItem

                                NavigableMenuItem {
                                    text: qsTr("Status: %1").arg(navItem.hostOnline ? qsTr("Online") : qsTr("Offline"))
                                    font.bold: true
                                    enabled: false
                                }
                                NavigableMenuItem {
                                    text: qsTr("Wake host")
                                    visible: !navItem.hostOnline && navItem.hostWakeable
                                    onTriggered: pcView.wakeHostAtIndex(navItem.hostIndex,
                                                                        navItem.hostName,
                                                                        navItem.hostUuid)
                                }
                                NavigableMenuItem {
                                    text: qsTr("Wake and connect")
                                    visible: !navItem.hostOnline && navItem.hostWakeable
                                    onTriggered: page.wakeHost(navItem.hostUuid, true)
                                }
                                NavigableMenuItem {
                                    text: qsTr("Resume running session")
                                    visible: navItem.hostOnline && navItem.hostPaired && navItem.hostBusy
                                    onTriggered: page.requestResume(navItem.hostUuid)
                                }
                                NavigableMenuItem {
                                    text: qsTr("End remote session…")
                                    visible: navItem.hostOnline && navItem.hostPaired && navItem.hostBusy
                                    onTriggered: page.requestEndSession(navItem.hostUuid)
                                }
                                NavigableMenuItem {
                                    text: qsTr("Test Connection")
                                    onTriggered: page.testConnection()
                                }
                                NavigableMenuItem {
                                    text: qsTr("Rename…")
                                    onTriggered: page.requestRename(navItem.hostUuid)
                                }
                                NavigableMenuItem {
                                    text: qsTr("Remove Host")
                                    onTriggered: page.requestDelete(navItem.hostUuid)
                                }
                                NavigableMenuItem {
                                    text: qsTr("View Details")
                                    onTriggered: page.requestDetails(navItem.hostUuid)
                                }
                            }
                        }
                    }

                    footer: AbstractButton {
                        id: addHostButton
                        objectName: "starlight.hosts.add-host"
                        width: sidebarList.width
                        height: 32
                        hoverEnabled: true
                        focusPolicy: Qt.TabFocus

                        onClicked: {
                            if (window && window.requestAddHost) {
                                window.requestAddHost()
                            } else {
                                page.requestAddHostOverlay()
                                if (typeof addPcDialog !== "undefined" && addPcDialog.open)
                                    addPcDialog.open()
                            }
                        }
                        Keys.onReturnPressed: ControlCompat.activate(addHostButton)
                        Keys.onEnterPressed: ControlCompat.activate(addHostButton)

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Pair a new host")
                        Accessible.description: qsTr("Adds a host manually")
                        Accessible.focusable: addHostButton.enabled && addHostButton.visible
                        Accessible.focused: addHostButton.activeFocus
                        Accessible.pressed: addHostButton.pressed
                        Accessible.onPressAction: {
                            if (addHostButton.visible) {
                                ControlCompat.activate(addHostButton)
                            }
                        }

                        AccessibleIdentity {
                            target: addHostButton
                            stableId: addHostButton.objectName
                        }

                        background: Rectangle {
                            radius: 7
                            color: (addHostButton.hovered || addHostButton.visualFocus) ? Theme.segBg : "transparent"
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            SLIcon {
                                name: "plus"
                                size: Theme.iconSmall
                                Layout.alignment: Qt.AlignVCenter
                                opacity: 0.7
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Pair a New Host…")
                                color: Theme.label2
                                font.pixelSize: Theme.sizeBody
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    spacing: 4

                    // Item 8: HIG links open directly, no confirmation --
                    // pointer cursor + hover brighten/underline + tooltip.
                    Text {
                        id: versionLinkText
                        Layout.fillWidth: true
                        text: qsTr("Moonwalk · %1").arg(page.hostPreferenceSnapshot.starlightVersionString)
                        color: versionLinkHover.containsMouse ? Theme.label2 : Theme.label3
                        font.pixelSize: Theme.sizeFootnote
                        // Persistent underline (audit §8 #3): previously this
                        // only underlined ON hover, so at rest it was visually
                        // indistinguishable from the non-interactive "Hosts"
                        // heading right above it -- a live external link with
                        // no at-rest cue that it's clickable. A quiet, always-on
                        // underline is the lowest-noise way to say "link" without
                        // requiring hover, while still reading as low-emphasis
                        // footnote chrome (not promoted to label2/accent color).
                        font.underline: true
                        elide: Text.ElideRight

                        MouseArea {
                            id: versionLinkHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("https://github.com/joaodriessen/moonwalk")

                            ToolTip.delay: 500
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("View on GitHub")
                        }
                    }

                    // Item 5: bigger tap target, with Lucide's gear (the bundled
                    // Material settings.svg read as dated/small at this size).
                    // Drawn black; icon.color below tints it on the CPU.
                    NavigableToolButton {
                        id: settingsButton
                        objectName: "starlight.hosts.open-settings"
                        iconSource: Lucide.url("settings", "#000000")
                        // Override NavigableToolButton's white default -- this
                        // is HIG chrome, not the legacy blue toolbar, so the
                        // glyph needs to track Theme like everything else here.
                        icon.color: Theme.label2
                        implicitWidth: 30
                        implicitHeight: 30
                        // Override the component's own "fill my parent's height"
                        // default (fine in main.qml's fixed-height ToolBar, but
                        // this sidebar footer row sizes to its content instead).
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30

                        ToolTip.delay: 1000
                        ToolTip.timeout: 3000
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("More settings")

                        // Phase-1 escape hatch: everything SettingsView.qml
                        // exposed that this page doesn't (yet) show inline.
                        onClicked: page.requestOpenSettings()

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("More settings")
                        Accessible.description: qsTr("Opens global Moonwalk settings")
                        Accessible.focusable: enabled && visible
                        Accessible.focused: activeFocus
                        Accessible.pressed: pressed
                        Accessible.onPressAction: {
                            if (settingsButton.visible) {
                                ControlCompat.activate(settingsButton)
                            }
                        }

                        AccessibleIdentity {
                            target: settingsButton
                            stableId: settingsButton.objectName
                        }
                    }
                }
            }
        }

        // ── Content ──
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty state: no hosts known yet. This is the app's true
            // first-run screen, so it always gives a real primary action --
            // previously this was centered TEXT ONLY with no button at all,
            // and the sole "add a host" control was a faint 32px hover-only
            // footer row in the sidebar off to the side (audit item A / §8
            // #1). mDNS-on keeps the reassuring spinner + caption as the
            // primary read (don't replace "we're looking" with a button that
            // implies we've given up), but now always offers the manual path
            // too, so a host that never auto-discovers can't strand anyone.
            ColumnLayout {
                id: emptyState
                anchors.centerIn: parent
                spacing: Theme.spacingM

                visible: sidebarList.count === 0

                SLBusy {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.hostPreferenceSnapshot.enableMdns
                    running: visible
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    visible: page.hostPreferenceSnapshot.enableMdns
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.label2
                    font.pixelSize: Theme.sizeBody
                    text: qsTr("Searching for compatible hosts on your local network…")
                }
                // Secondary affordance while mDNS is searching: quieter than
                // the primary button below, but still a real control, not
                // just prose that names an action with no way to take it.
                SLButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXS
                    visible: page.hostPreferenceSnapshot.enableMdns
                    quiet: true
                    text: qsTr("Add a host manually")
                    onClicked: window.requestAddHost()
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    visible: !page.hostPreferenceSnapshot.enableMdns
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.label2
                    font.pixelSize: Theme.sizeBody
                    text: qsTr("Automatic host discovery is off.")
                }
                // The main event when there's no mDNS story to tell: a
                // prominent primary action, same handler as the sidebar
                // footer's "Pair a New Host…" row.
                SLButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXS
                    visible: !page.hostPreferenceSnapshot.enableMdns
                    text: qsTr("Pair a host")
                    onClicked: window.requestAddHost()
                }
            }

            Flickable {
                id: contentFlick
                anchors.fill: parent
                visible: sidebarList.count > 0
                contentWidth: width
                // Height-driven centering (flagship hero treatment,
                // 2026-07-18): contentHeight always covers at least the
                // viewport, so contentColumn's own y-centering below (which
                // needs a stable contentFlick.height to center against) has
                // room to place the hero mid-pane without ever clipping it.
                contentHeight: Math.max(contentFlick.height, contentColumn.implicitHeight + 2 * Theme.pageInsetTop)
                boundsBehavior: Flickable.OvershootBounds
                clip: true

                ScrollBar.vertical: SLScrollBar {
                    objectName: "starlight.hosts.settings-scrollbar"
                    accessibleName: qsTr("Host settings scroll bar")
                    accessibleDescription: qsTr("Scrolls through settings for the selected host")
                }

                Column {
                    id: contentColumn
                    x: Theme.pageInsetX
                    // Vertically centers the whole hero+settings column when
                    // it's shorter than the viewport (collapsed Adjust, the
                    // common case) -- clamps to the normal top inset once the
                    // content grows past the viewport (Adjust expanded, or a
                    // small window), at which point it scrolls normally. One
                    // driver, no separate Behavior: as SLDisclosure's own
                    // height animates open, implicitHeight changes continuously
                    // frame-by-frame and this recomputes in lockstep, so the
                    // hero visibly rises as the panel unfurls.
                    y: Math.max(Theme.pageInsetTop, (contentFlick.height - contentColumn.implicitHeight) / 2)
                    width: parent.width - 2 * Theme.pageInsetX
                    spacing: 0

                    readonly property var cur: pcView.selectedHostPresentation
                    // Host-switch entrance settle (heroSettle, inside
                    // hostCard below) retriggers only when the SELECTED HOST
                    // changes, not when Adjust is toggled on the same host.
                    onCurChanged: heroSettle.restart()

                    // Per item 6: the window titlebar already says "Starlight"
                    // (per HIG, that's sufficient) -- the in-page repeat of
                    // the same title was removed so the content starts right
                    // at the host card, reclaiming the vertical space.

                    // Host card — the single consolidated identity + status +
                    // spec + action surface for the selected host (design doc:
                    // 2026-07-18-host-card.md). Three columns: glyph | facts |
                    // action. The old standalone address chip and "Tuned for
                    // this host" card are gone -- their content lives here now.
                    Rectangle {
                        id: hostCard
                        width: parent.width
                        height: heroCol.implicitHeight + 2 * Theme.spacingXL
                        radius: Theme.radiusCard
                        // Opaque (unlike the reveal groups below, which stay
                        // Theme.card/translucent) -- the hero surface reads as
                        // the elevated focal layer above them via a color swap,
                        // not a shader: MultiEffect drop-shadows render this
                        // app's glyphs invisible on some render paths.
                        //
                        color: Theme.cardHeroTop
                        border.width: 1
                        border.color: Theme.hairline

                        // Absorbs the action-button appear/disappear and
                        // name-wrap reflow smoothly instead of a hard jump.
                        Behavior on height { NumberAnimation { duration: Theme.durationMedium; easing.type: Easing.OutCubic } }

                        // Host-switch entrance settle: fade 0->1 + 6px rise,
                        // retriggered only when the selected host changes (see
                        // contentColumn's onCurChanged above) -- toggling
                        // Adjust on the SAME host never replays this.
                        SequentialAnimation {
                            id: heroSettle
                            PropertyAction { target: heroCol; property: "opacity"; value: 0.0 }
                            PropertyAction { target: heroTranslate; property: "y"; value: 6 }
                            ParallelAnimation {
                                NumberAnimation { target: heroCol; property: "opacity"; to: 1.0; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
                                NumberAnimation { target: heroTranslate; property: "y"; to: 0; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
                            }
                        }

                        // Centered vertical hero stack: glyph -> name -> status
                        // -> one action -> meta/spec footer. Replaces the old
                        // glyph|facts|action three-column RowLayout. Rhythm is
                        // explicit per-item topMargin spacers (spacing:0 below,
                        // the same idiom the rest of this page already uses).
                        Column {
                            id: heroCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: Theme.spacingXL
                            width: Math.min(parent.width - 2 * Theme.spacingL, 460)
                            spacing: 0

                            transform: Translate { id: heroTranslate; y: 0 }

                            // 1. Device identity: a large naked OS glyph (macOS
                            // System Settings style) — no container box. Offline
                            // mutes the whole identity further on top of the
                            // existing light/dark base softening; the
                            // hostStatusUnknown guard keeps a host mid-check at
                            // full strength so it never flickers dim-then-bright.
                            SLIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: pcView.osIconName(contentColumn.cur ? contentColumn.cur.hostOs : "")
                                size: Theme.iconHero
                                // Softer than full label ink — at this size full
                                // ink reads heavy, especially on the light palette.
                                readonly property real baseOpacity: Theme.isDark ? 0.85 : 0.55
                                opacity: baseOpacity * ((contentColumn.cur && !contentColumn.cur.hostStatusUnknown && !contentColumn.cur.hostOnline) ? 0.5 : 1.0)

                                Behavior on opacity { NumberAnimation { duration: Theme.durationMedium } }
                            }

                            Item { width: 1; height: Theme.spacingM }

                            // 2. Host title -- display only (owner ruling
                            // 2026-07-19: the inline click-to-edit affordance
                            // is gone; renaming lives solely in the sidebar
                            // item's right-click "Rename…" context menu, one
                            // rename path, not two). Wraps to 2 lines then
                            // elides -- far more graceful than mid-word elision
                            // on a centered hero.
                            Text {
                                width: parent.width
                                text: contentColumn.cur ? contentColumn.cur.hostName : ""
                                color: Theme.label
                                font.pixelSize: Theme.sizeLargeTitle
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            Item { width: 1; height: Theme.spacingXS }

                            // 3. Status line: dot + text, or -- while status is
                            // still unknown -- a spinner in the dot's place and
                            // "Checking…" (the ONE spinner in the whole card;
                            // the old action-column busy indicator is gone).
                            RowLayout {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 7
                                    Layout.preferredHeight: 7
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 3.5
                                    visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                                    color: (contentColumn.cur && contentColumn.cur.hostOnline) ? Theme.ok : Theme.label3

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                                }

                                SLBusy {
                                    Layout.preferredWidth: 13
                                    Layout.preferredHeight: 13
                                    Layout.alignment: Qt.AlignVCenter
                                    visible: contentColumn.cur && contentColumn.cur.hostStatusUnknown
                                    running: visible
                                }

                                Text {
                                    text: pcView.statusText(contentColumn.cur)
                                    color: Theme.label2
                                    font.pixelSize: Theme.sizeBody
                                }
                            }

                            // 3b. Unpaired invite subline (owner-eyes flourish,
                            // design doc §2.1) -- a single quiet line inviting
                            // the pair action, shown ONLY in the unpaired state
                            // (host online, not yet paired). Every other state
                            // (paired, offline, checking) never shows this --
                            // same visibility guard as the Pair button below,
                            // so it can never desync from it. Column
                            // automatically excludes invisible children from
                            // layout, so this collapses to zero height in every
                            // other state without touching their rhythm.
                            Item {
                                width: 1
                                height: Theme.spacingXS
                                visible: contentColumn.cur && contentColumn.cur.hostOnline && !contentColumn.cur.hostPaired
                            }

                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                visible: contentColumn.cur && contentColumn.cur.hostOnline && !contentColumn.cur.hostPaired
                                text: qsTr("Pair this device to start streaming.")
                                color: Theme.label2
                                font.pixelSize: Theme.sizeCaption
                                wrapMode: Text.WordWrap
                            }

                            // 4. Action slot: exactly ONE state-selected primary
                            // control, hero-styled, nothing stacked under it. No
                            // spinner here -- the checking state's spinner lives
                            // in the status line above. The spacer above the
                            // slot (and the slot itself) collapses entirely
                            // while status is unknown -- checking has no action.
                            Item {
                                width: 1
                                height: Theme.spacingL
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                            }

                            SLButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                hero: true
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown && !contentColumn.cur.hostOnline && contentColumn.cur.hostWakeable
                                text: qsTr("Wake host")
                                onClicked: pcView.wakeHostAtIndex(sidebarList.currentIndex,
                                                                  contentColumn.cur.hostName,
                                                                  contentColumn.cur.hostUuid)
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown && !contentColumn.cur.hostOnline && !contentColumn.cur.hostWakeable
                                text: qsTr("Host is offline")
                                color: Theme.label2
                                font.pixelSize: Theme.sizeCaption
                            }

                            SLButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                hero: true
                                visible: contentColumn.cur && contentColumn.cur.hostOnline && !contentColumn.cur.hostPaired
                                text: qsTr("Pair")
                                onClicked: pcView.beginPairing()
                            }

                            SLButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                hero: true
                                visible: contentColumn.cur && contentColumn.cur.hostOnline && contentColumn.cur.hostPaired
                                text: (contentColumn.cur && contentColumn.cur.hostBusy) ? qsTr("Resume") : qsTr("Connect")
                                onClicked: pcView.connectToCurrentHost()
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: page.waitingForWake === true
                                text: qsTr("Waiting for the host to become ready…")
                                color: Theme.label2
                                font.pixelSize: Theme.sizeCaption
                            }
                            SLButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: page.waitingForWake === true
                                text: qsTr("Cancel wake and connect")
                                onClicked: page.cancelWakeConnect()
                            }

                            // 5. Inset hairline divider -- structure for the
                            // meta/spec foot. Visible whenever the host's
                            // status has resolved (footer exists); never
                            // full-bleed (clear margin either side).
                            Item {
                                width: 1
                                height: Theme.spacingL
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(parent.width, 360)
                                height: 1
                                color: Theme.hairline
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                            }

                            Item {
                                width: 1
                                height: Theme.spacingM
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                            }

                            // 6. Meta line: address · OS · server software --
                            // every part optional (address only while online;
                            // OS/software from live detection, persisted
                            // between polls). This is the SINGLE home of the
                            // address now -- the separate address chip group
                            // that used to duplicate it is gone. Hidden entirely
                            // while status is unknown -- the footer doesn't
                            // exist mid-check.
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                                text: {
                                    var cur = contentColumn.cur
                                    if (!cur) {
                                        return ""
                                    }
                                    var parts = []
                                    if (cur.hostOnline && cur.hostAddress) {
                                        parts.push(cur.hostAddress)
                                    }
                                    var osName = pcView.osDisplayName(cur.hostOs)
                                    if (osName) {
                                        parts.push(osName)
                                    }
                                    if (cur.hostServerSoftware) {
                                        parts.push(cur.hostServerSoftware === "Skylight" ? "Moonwalk Host" : cur.hostServerSoftware)
                                    }
                                    return parts.join(" · ")
                                }
                                color: Theme.label2
                                font.pixelSize: Theme.sizeFootnote
                                elide: Text.ElideRight
                            }

                            Item {
                                width: 1
                                height: Theme.spacingXS
                                visible: contentColumn.cur && contentColumn.cur.hostPaired && !contentColumn.cur.hostStatusUnknown
                            }

                            // 7. Spec line -- the migrated "tuned for this host"
                            // line (previously the standalone "optimizedCard"
                            // below, now deleted), quiet (show-don't-tell, no
                            // announcing header). Hidden for an unpaired or
                            // status-unknown host -- nothing meaningful to tune
                            // for a host never streamed, and state is
                            // indeterminate while checking.
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                visible: contentColumn.cur && contentColumn.cur.hostPaired && !contentColumn.cur.hostStatusUnknown
                                elide: Text.ElideRight
                                color: Theme.label2
                                font.pixelSize: Theme.sizeFootnote
                                // Owner ruling 2026-07-19: end-user-relevant
                                // facts only. The codec chip shows ONLY when a
                                // codec is explicitly forced ("Auto" said
                                // nothing to an end user), and the cursor-mode
                                // chip is gone entirely (an implementation
                                // detail). The resolution earns a "native" tag
                                // when it matches this client's own screen --
                                // same screen-0 source of truth the
                                // native-first seeding uses.
                                text: {
                                    var nativeRes = pcView.nativeResolution()
                                    var preferences = page.hostPreferenceSnapshot
                                    var plan = page.streamPlan || ({})
                                    var width = plan.width || preferences.width
                                    var height = plan.height || preferences.height
                                    var isNative = nativeRes.width === width && nativeRes.height === height
                                    var res = width + "×" + height
                                    if (isNative) {
                                        res += " (" + qsTr("native") + ")"
                                    }
                                    return [
                                        res,
                                        (plan.fps || preferences.fps) + " " + qsTr("fps"),
                                        qsTr("about %1 Mbps").arg(((plan.startBitrateKbps || preferences.bitrateKbps) / 1000.0).toFixed(1)),
                                        pcView.codecChipLabel()
                                    ].filter(function(part) { return part.length > 0 }).join(" · ")
                                }
                            }

                            Item {
                                width: 1
                                height: Theme.spacingS
                                visible: contentColumn.cur && contentColumn.cur.hostPaired && !contentColumn.cur.hostStatusUnknown
                            }

                            // 8. Disclosure affordance -- mirrors SLDisclosure's
                            // own header chevron exactly (same glyph, same
                            // rotation values, same duration) so the affordance
                            // and the panel it drives animate identically.
                            // hostDisclosure stays right where it is on the
                            // page, below the card; this is its only driver.
                            // Same visibility as the spec line above.
                            AbstractButton {
                                id: adjustLink
                                objectName: "starlight.hosts.adjust-settings"
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: contentColumn.cur && contentColumn.cur.hostPaired && !contentColumn.cur.hostStatusUnknown
                                hoverEnabled: true
                                focusPolicy: Qt.TabFocus
                                implicitWidth: adjustLinkRow.implicitWidth + 10
                                implicitHeight: adjustLinkRow.implicitHeight + 6

                                onClicked: hostDisclosure.expanded = !hostDisclosure.expanded
                                Keys.onReturnPressed: clicked()
                                Keys.onEnterPressed: clicked()

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Adjust")
                                Accessible.description: qsTr("Shows or hides settings for the selected host")
                                Accessible.focusable: adjustLink.enabled && adjustLink.visible
                                Accessible.focused: adjustLink.activeFocus
                                Accessible.pressed: adjustLink.pressed
                                Accessible.checkable: true
                                Accessible.checked: hostDisclosure.expanded
                                Accessible.onPressAction: {
                                    if (adjustLink.visible) {
                                        ControlCompat.activate(adjustLink)
                                    }
                                }
                                Accessible.onToggleAction: {
                                    if (adjustLink.visible) {
                                        ControlCompat.activate(adjustLink)
                                    }
                                }

                                AccessibleIdentity {
                                    target: adjustLink
                                    stableId: adjustLink.objectName
                                }

                                background: Rectangle {
                                    radius: Theme.radiusChip
                                    color: (adjustLink.hovered || adjustLink.visualFocus) ? Theme.segBg : "transparent"
                                    border.width: adjustLink.visualFocus ? 2 : 0
                                    border.color: Theme.accent
                                }

                                contentItem: RowLayout {
                                    id: adjustLinkRow
                                    spacing: 4

                                    Text {
                                        text: qsTr("Adjust")
                                        color: Theme.accent
                                        font.pixelSize: Theme.sizeCaption
                                        font.weight: Font.DemiBold
                                    }

                                    SLIcon {
                                        name: "chevron-down"
                                        tone: "accent"
                                        size: 11
                                        rotation: hostDisclosure.expanded ? 0 : -90

                                        Behavior on rotation { NumberAnimation { duration: Theme.durationFast } }
                                    }
                                }
                            }
                        }
                    }

                    Item { width: 1; height: Theme.spacingL }

                    // ── "Adjust" reveal: the exact same Video/Feel/Audio
                    // groups this page has always had, just collapsed by
                    // default now and opened either via its own header (hidden
                    // here -- showHeader:false) or the host card's "Adjust"
                    // affordance above. Every control keeps its existing
                    // presentation and delegates changes through the facade. ──
                    SLDisclosure {
                        id: hostDisclosure
                        objectName: "starlight.hosts.host-settings"
                        width: parent.width
                        showHeader: false

                        SLSectionHeader { text: qsTr("Network") }
                        Item { width: 1; height: 7 }
                        SLGroup {
                            width: parent.width
                            SLRow {
                                topBorder: false
                                label: qsTr("Video quality")
                                sublabel: pcView.qualityMode === "adaptive"
                                    ? qsTr("As sharp as your network allows, adjusting by itself before the picture stutters.")
                                    : pcView.qualityMode === "manual"
                                      ? qsTr("Choose the maximum bitrate yourself.")
                                      : qsTr("Uses a recommended bitrate for the selected resolution and frame rate.")
                                SLDropdown {
                                    id: legacyQualityChoice
                                    objectName: "starlight.hosts.quality-mode"
                                    width: Math.max(130, implicitWidth)
                                    model: pcView.qualityOptions
                                    popupWidth: 230
                                    currentIndex: pcView.indexForValue(model, pcView.qualityMode)
                                    onActivated: (index) => page.setQualityMode(model[index].value)
                                    Text {
                                        visible: legacyQualityChoice.currentIndex < 0
                                        anchors.left: parent.left
                                        anchors.leftMargin: parent.leftPadding
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - parent.leftPadding - parent.rightPadding - 16
                                        text: qsTr("Unavailable")
                                        color: Theme.label
                                        font.pixelSize: Theme.sizeCaption
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            SLRow {
                                visible: pcView.qualityMode === "manual"
                                label: qsTr("Maximum bitrate")
                                sublabel: qsTr("Lower the bitrate on slower connections. Raise the bitrate to increase image quality.")
                                accessibleControl: bitrateSlider

                                RowLayout {
                                    spacing: 8

                                    SLSlider {
                                        id: bitrateSlider
                                        objectName: "starlight.hosts.maximum-bitrate"
                                        accessibleName: qsTr("Maximum bitrate")
                                        accessibleDescription: qsTr("Sets the maximum streaming video bitrate")
                                        Layout.preferredWidth: 140
                                        from: Math.max(500, (page.streamPlan || {}).floorKbps || 0)
                                        to: 150000
                                        stepSize: 500
                                        snapMode: Slider.SnapOnRelease

                                        // Dragging self-writes `value`, which would permanently
                                        // break a declarative `value:` binding — so the pref is
                                        // synced in imperatively instead (skipped mid-drag so the
                                        // handle doesn't fight the user's hand).
                                        readonly property real boundValue: page.hostPreferenceSnapshot.bitrateKbps
                                        onBoundValueChanged: if (!pressed) value = boundValue
                                        Component.onCompleted: value = boundValue

                                        onMoved: {
                                            page.setBitrateKbps(Math.round(value))
                                        }
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignVCenter
                                        text: qsTr("%1 Mbps").arg((page.hostPreferenceSnapshot.bitrateKbps / 1000.0).toFixed(1))
                                        color: Theme.label2
                                        font.pixelSize: Theme.sizeCaption
                                    }
                                }
                            }
                        }
                        Item { width: 1; height: Theme.spacingL }

                        // Video
                        SLSectionHeader { text: qsTr("Video") }
                        Item { width: 1; height: 7 }
                        SLGroup {
                            width: parent.width

                            SLRow {
                                topBorder: false
                                label: qsTr("Resolution")
                                sublabel: pcView.resolutionSublabel()

                                SLDropdown {
                                    model: pcView.resolutionOptionsList
                                    popupWidth: 200
                                    currentIndex: pcView.resolutionOptionIndex()
                                    onActivated: {
                                        if (index === model.length - 1) {
                                            page.requestCustomResolution(page.selectedHostUuid)
                                        } else {
                                            pcView.setResolution(model[index].w, model[index].h)
                                        }
                                    }
                                }
                            }

                            SLRow {
                                label: qsTr("Frame rate")

                                SLDropdown {
                                    model: pcView.fpsOptionsList
                                    popupWidth: 140
                                    currentIndex: pcView.fpsOptionIndex()
                                    onActivated: {
                                        if (index === model.length - 1) {
                                            page.requestCustomFrameRate(page.selectedHostUuid)
                                        } else {
                                            pcView.setFps(model[index].value)
                                        }
                                    }
                                }
                            }


                            SLRow {
                                label: qsTr("Video codec")

                                SLDropdown {
                                    model: pcView.codecOptions
                                    popupWidth: 220
                                    currentIndex: pcView.indexForValue(model, page.hostPreferenceSnapshot.videoCodecConfig)
                                    onActivated: page.setVideoCodec(model[index].value)
                                }
                            }

                            SLRow {
                                label: qsTr("Desktop scaling")
                                sublabel: page.selectedHostSnapshot.desktopScaleSupported === true
                                    ? qsTr("Choose more desktop space or larger text.")
                                    : qsTr("This host does not advertise desktop scaling support.")
                                SLDropdown {
                                    objectName: "starlight.hosts.desktop-scale"
                                    enabled: page.selectedHostSnapshot.desktopScaleSupported === true
                                    model: [{ text: qsTr("Automatic"), value: "auto" },
                                            { text: qsTr("1× — more space"), value: "1" },
                                            { text: qsTr("2× — larger text"), value: "2" }]
                                    popupWidth: 210
                                    currentIndex: Math.max(0, pcView.indexForValue(model, page.hostPreferenceSnapshot.desktopScale))
                                    onActivated: page.setDesktopScale(model[index].value)
                                }
                            }
                        }

                        Item { width: 1; height: Theme.spacingL }

                        // Mouse and keyboard
                        SLSectionHeader { text: qsTr("Mouse and keyboard") }
                        Item { width: 1; height: 7 }
                        SLGroup {
                            width: parent.width

                            SLRow {
                                topBorder: false
                                label: qsTr("Use this computer’s keyboard layout for typing")
                                sublabel: qsTr("Type with accents and input methods from this computer. Turn off for games.")
                                SLToggle {
                                    objectName: "starlight.hosts.local-text-input"
                                    checked: page.hostPreferenceSnapshot.localTextInput === true
                                    onSwitched: (wanted) => page.setLocalTextInput(wanted)
                                }
                            }

                            SLRow {
                                label: qsTr("Instant cursor")
                                // Host-adaptive (Part 2 of the adaptive-settings
                                // design): describes what THIS host actually does,
                                // not just what the toggle's live value is. Unknown/
                                // future hosts (currentHostKind === "unknown") fall
                                // through to the same conservative stock wording as
                                // a non-Skylight host -- never over-promise.
                                sublabel: pcView.currentHostKind === "skylight"
                                          ? qsTr("Drawn on this machine for responsive movement. Moonwalk Host hides its own to match.") + " " + qsTr("Reconnect to switch between the host cursor and a local cursor.")
                                          : qsTr("May show a second cursor on this host — Moonwalk Host coordinates this automatically, other hosts don't.")

                                SLToggle {
                                    checked: page.hostPreferenceSnapshot.localCursor
                                    onSwitched: (wanted) => page.setLocalCursorEnabled(wanted)
                                }
                            }

                            SLRow {
                                label: qsTr("Optimize mouse for remote desktop")
                                sublabel: qsTr("This enables seamless mouse control without capturing the client's mouse cursor. It is ideal for remote desktop usage but will not work in most apps.") + " " +
                                          qsTr("You can toggle this while streaming using Ctrl+Alt+Shift+M.") + " " +
                                          qsTr("Note: due to a bug in GeForce Experience, this option may not work properly if the host has multiple monitors.")

                                SLToggle {
                                    checked: page.hostPreferenceSnapshot.absoluteMouseMode
                                    onSwitched: (wanted) => page.setAbsoluteMouseMode(wanted)
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 24 }
                }
            }

            // ── Condensing sticky header (macOS large-title pattern): once the
            // full host card scrolls out of view, a compact identity bar fades
            // in pinned to the top — which host you're editing plus the primary
            // action, so connecting after tweaking a deep setting no longer
            // means scrolling all the way back up. Opaque ground + hairline so
            // content scrolls beneath it cleanly. ──
            Rectangle {
                id: stickyHostBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 60
                color: Theme.ground
                z: 10

                readonly property bool shown: contentFlick.contentY > hostCard.height + Theme.pageInsetTop
                visible: opacity > 0.01
                opacity: shown ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.hairline
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.pageInsetX
                    anchors.rightMargin: Theme.pageInsetX
                    spacing: 10

                    SLIcon {
                        name: pcView.osIconName(contentColumn.cur ? contentColumn.cur.hostOs : "")
                        size: Theme.iconSmall
                        opacity: 0.8
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: contentColumn.cur ? contentColumn.cur.hostName : ""
                        color: Theme.label
                        font.pixelSize: Theme.sizeBody
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.maximumWidth: 260
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: pcView.statusText(contentColumn.cur)
                        color: Theme.label2
                        font.pixelSize: Theme.sizeFootnote
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Deliberately the SAME standard SLButton as the host
                    // card's — a shrunken variant here read as a different,
                    // less trustworthy control ("why does Connect look
                    // different up here?"). One action, one shape, everywhere.
                    SLButton {
                        visible: contentColumn.cur && contentColumn.cur.hostOnline && contentColumn.cur.hostPaired
                        text: (contentColumn.cur && contentColumn.cur.hostBusy) ? qsTr("Resume") : qsTr("Connect")
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: pcView.connectToCurrentHost()
                    }

                    SLButton {
                        visible: contentColumn.cur && !contentColumn.cur.hostStatusUnknown
                                 && !contentColumn.cur.hostOnline && contentColumn.cur.hostWakeable
                        text: qsTr("Wake host")
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: pcView.wakeHostAtIndex(sidebarList.currentIndex,
                                                          contentColumn.cur ? contentColumn.cur.hostName : "",
                                                          contentColumn.cur ? contentColumn.cur.hostUuid : "")
                    }

                    // Completes the state coverage above: the full host card
                    // also offers Pair for an online-but-unpaired host, so the
                    // sticky bar needs the same affordance once that card has
                    // scrolled away -- otherwise an unpaired host loses its
                    // only actionable control mid-scroll.
                    SLButton {
                        visible: contentColumn.cur && contentColumn.cur.hostOnline && !contentColumn.cur.hostPaired
                        text: qsTr("Pair")
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: pcView.beginPairing()
                    }
                }
            }
        }
    }

    // ── Pairing sheet: big PIN, per the mockup ──
    NavigableDialog {
        id: pairSheet
        objectName: "starlight.hosts.pairing-dialog"
        property string pin: "0000"
        property string hostName: ""

        modal: true
        closePolicy: Popup.CloseOnEscape
        width: Theme.dialogWidth

        // Interaction M5: the one dialog in the app with no explicit initial
        // focus assignment -- every sibling NavigableDialog forces focus onto
        // its primary control in onOpened, this one didn't.
        onOpened: cancelButton.forceActiveFocus()

        background: SLSheet { shadow: true }

        contentItem: ColumnLayout {
            id: pairSemanticContent
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Pair with %1").arg(pairSheet.hostName)
            Accessible.description: qsTr("Enter the displayed PIN on the host to finish pairing")

            AccessibleIdentity {
                target: pairSemanticContent
                stableId: pairSheet.objectName
            }

            spacing: 4

            // Item 13: a key/lock icon, so the sheet reads as a pairing
            // prompt at a glance instead of just a wall of text -- reuses
            // the existing lock asset (already used for the "not paired"
            // sidebar/status glyph) rather than adding a near-duplicate.
            SLIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 2
                name: "lock"
                tone: "accent"
                size: 26
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Pair with %1").arg(pairSheet.hostName)
                font.pixelSize: Theme.sizeHeadline
                font.weight: Font.DemiBold
                color: Theme.label
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.bottomMargin: 16
                text: qsTr("Please enter %1 on your host. This dialog will close when pairing is completed.").arg(pairSheet.pin) + "\n\n" +
                      qsTr("If your host is running Sunshine, navigate to the Sunshine web UI to enter the PIN.")
                font.pixelSize: Theme.sizeCaption
                color: Theme.label2
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Repeater {
                    model: pairSheet.pin.split("")
                    delegate: Rectangle {
                        width: 46
                        height: 56
                        radius: 9
                        color: Theme.segBg
                        border.width: 1
                        border.color: Theme.hairline
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 26
                            font.weight: Font.Bold
                            font.family: "Menlo, Consolas, monospace"
                            color: Theme.label
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                Layout.bottomMargin: 16
                spacing: 8

                SLBusy {
                    running: pairSheet.visible
                    implicitWidth: 16
                    implicitHeight: 16
                }
                Label {
                    text: qsTr("Waiting for the host…")
                    color: Theme.label2
                    font.pixelSize: Theme.sizeCaption
                }
            }

            SLButton {
                id: cancelButton
                objectName: "starlight.hosts.pairing-cancel"
                Layout.alignment: Qt.AlignHCenter
                quiet: true
                text: qsTr("Cancel")
                accessibleDescription: qsTr("Cancels the current pairing attempt")
                onClicked: pairSheet.reject()
            }
        }

        onRejected: {
            // Cancel this exact backend attempt. Late completions remain
            // ignored without affecting a subsequent pairing attempt.
            page.cancelPairing(page.baseOverlay.payload.attemptId)
        }
    }

    NavigableDialog {
        id: customResDialog
        objectName: "starlight.hosts.custom-resolution-dialog"
        property int operationId: -1
        property int currentWidth: 0
        property int currentHeight: 0
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Theme.dialogWidth

        onOpened: resWidthField.forceActiveFocus()
        onClosed: {
            resWidthField.clear()
            resHeightField.clear()
        }
        onAccepted: {
            var w = parseInt(resWidthField.text || resWidthField.placeholderText)
            var h = parseInt(resHeightField.text || resHeightField.placeholderText)
            if (w > 0 && h > 0) {
                page.submitCustomResolution(customResDialog.operationId, w, h)
            }
        }
        onRejected: page.dismissOverlay(operationId)

        // Bound as contentItem (not a bare child), matching addPcDialog's
        // fix in main.qml (layout H1): a bare ColumnLayout child's own
        // implicit width is driven by its unconstrained children instead of
        // resolving against the dialog's fixed width, so longer/localized
        // strings blow past width: 340.
        contentItem: ColumnLayout {
            id: customResSemanticContent
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Custom resolution")
            Accessible.description: qsTr("Enter a custom streaming resolution")

            AccessibleIdentity {
                target: customResSemanticContent
                stableId: customResDialog.objectName
            }

            spacing: 4

            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                text: qsTr("Some hosts (including GeForce Experience) won't automatically match a custom resolution — you may need to set the display resolution manually while streaming.")
                font.pixelSize: Theme.sizeCaption
                color: Theme.label2
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Enter a custom resolution:")
                font.pixelSize: Theme.sizeHeadline
                font.weight: Font.DemiBold
                color: Theme.label
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4

                SLTextField {
                    id: resWidthField
                    objectName: "starlight.hosts.custom-resolution-width"
                    Layout.fillWidth: true
                    maximumLength: 5
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: "" + customResDialog.currentWidth
                    accessibleName: qsTr("Custom resolution width")
                    accessibleDescription: qsTr("Enter a width from 256 to 8192 pixels")
                    validator: IntValidator { bottom: 256; top: 8192 }
                    focus: true
                    onReturnPressed: customResDialog.accept()
                }
                Label { text: "×"; font.bold: true; color: Theme.label }
                SLTextField {
                    id: resHeightField
                    objectName: "starlight.hosts.custom-resolution-height"
                    Layout.fillWidth: true
                    maximumLength: 5
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: "" + customResDialog.currentHeight
                    accessibleName: qsTr("Custom resolution height")
                    accessibleDescription: qsTr("Enter a height from 256 to 8192 pixels")
                    validator: IntValidator { bottom: 256; top: 8192 }
                    onReturnPressed: customResDialog.accept()
                }
            }
        }
    }

    NavigableDialog {
        id: customFpsDialog
        objectName: "starlight.hosts.custom-frame-rate-dialog"
        property int operationId: -1
        property int currentFps: 0
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Theme.dialogWidth

        onOpened: fpsField.forceActiveFocus()
        onClosed: fpsField.clear()
        onAccepted: {
            var f = parseInt(fpsField.text || fpsField.placeholderText)
            if (f > 0) {
                page.submitCustomFrameRate(customFpsDialog.operationId, f)
            }
        }
        onRejected: page.dismissOverlay(operationId)

        contentItem: ColumnLayout {
            id: customFpsSemanticContent
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Custom frame rate")
            Accessible.description: qsTr("Enter a custom streaming frame rate")

            AccessibleIdentity {
                target: customFpsSemanticContent
                stableId: customFpsDialog.objectName
            }

            spacing: 4

            Label {
                Layout.fillWidth: true
                text: qsTr("Enter a custom frame rate:")
                font.pixelSize: Theme.sizeHeadline
                font.weight: Font.DemiBold
                color: Theme.label
            }
            SLTextField {
                id: fpsField
                objectName: "starlight.hosts.custom-frame-rate"
                Layout.fillWidth: true
                Layout.topMargin: 4
                maximumLength: 4
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: "" + customFpsDialog.currentFps
                accessibleName: qsTr("Custom frame rate")
                accessibleDescription: qsTr("Enter a frame rate from 10 to 9999 frames per second")
                validator: IntValidator { bottom: 10; top: 9999 }
                focus: true
                onReturnPressed: customFpsDialog.accept()
            }
        }
    }

    ErrorMessageDialog {
        id: errorDialog
        objectName: "starlight.hosts.error-dialog"
        title: qsTr("Error")
        accessibleName: qsTr("Error")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
        onClosed: page.dismissOverlay(page.interruptOverlay.operationId)
    }

    // Quit-then-connect gate. The context owns the decision and session;
    // this unchanged dialog only accepts or rejects its operation ID.
    NavigableMessageDialog {
        id: quitAppDialog
        property int operationId: -1
        property string appName: ""
        property bool endOnly: false
        text: qsTr("Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        destructiveAccept: true
        onOpened: Qt.callLater(function() {
            var cancel = quitAppDialog.footer.standardButton(Dialog.No)
            if (cancel) cancel.forceActiveFocus()
        })
        onAccepted: { if (endOnly) page.confirmEndSession(operationId); else page.confirmQuitAndConnect(operationId) }
        onRejected: page.dismissOverlay(operationId)
    }

    NavigableMessageDialog {
        id: deletePcDialog
        property int operationId: -1
        property string pcName: ""
        text: qsTr("Are you sure you want to remove %1?").arg(pcName)
        standardButtons: Dialog.Yes | Dialog.No
        // Interaction H7: irreversible action gets the red destructive accept
        // button instead of rendering pixel-identical to a benign OK. Wave C
        // adds this property to NavigableMessageDialog -- set it here
        // regardless so the wiring lands the moment both waves are merged.
        destructiveAccept: true
        onAccepted: page.confirmDelete(operationId)
        onRejected: page.dismissOverlay(operationId)
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        property int operationId: -1
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = qsTr("Moonwalk is testing your network connection to determine if any required ports are blocked.") + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts) {
            if (result === -1) {
                text = qsTr("The network test could not be performed because Moonwalk's connection-testing servers weren't reachable. Check your internet connection or try again later.")
                iconName = "triangle-alert"
            }
            else if (result === 0) {
                text = qsTr("No Internet streaming ports were blocked by this network. This does not test your host or local network connection.") + "\n\n" + qsTr("If you're streaming over the internet, check your host's port-forwarding configuration.")
                iconName = "circle-check"
            }
            else {
                text = qsTr("Required Internet streaming ports are blocked by this network. Streaming over the Internet may not work here.") + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                iconName = "circle-x"
            }

            showSpinner = false
        }
        onClosed: page.dismissOverlay(operationId)
    }

    NavigableDialog {
        id: renamePcDialog
        property int operationId: -1
        property string originalName

        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Theme.dialogWidth

        onOpened: editText.forceActiveFocus()
        onClosed: editText.clear()
        onAccepted: {
            if (editText.text) {
                page.submitRename(operationId, editText.text)
            }
        }
        onRejected: page.dismissOverlay(operationId)

        contentItem: ColumnLayout {
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: qsTr("Enter the new name for this host:")
                font.pixelSize: Theme.sizeHeadline
                font.weight: Font.DemiBold
                color: Theme.label
            }
            SLTextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                Layout.topMargin: 4
                focus: true
                onReturnPressed: renamePcDialog.accept()
            }
        }
    }

    NavigableDialog {
        id: showPcDetailsDialog
        objectName: "starlight.host-details-dialog"
        property int operationId: -1
        property string pcDetails: ""
        property string addressStatus: ""
        title: qsTr("Computer details")
        width: Math.min(480, parent ? parent.width - 32 : 480)
        standardButtons: Dialog.Close
        function saveAddress(value) {
            if (!page.submitConnectionAddress(operationId, value)) {
                addressStatus = qsTr("Address not saved. Check the address or reopen this computer’s details.")
                return
            }
            detailsAddress.text = value.trim()
            addressStatus = value.trim().length > 0 ? qsTr("Address saved.") : qsTr("Automatic connection enabled.")
        }
        onClosed: page.dismissOverlay(operationId)
        contentItem: ColumnLayout {
            spacing: 10
            ScrollView {
                id: legacyDetailsScroll
                objectName: "starlight.host-details-dialog.scroll"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(legacyDetailsLabel.implicitHeight,
                    Math.max(48, (showPcDetailsDialog.parent ? showPcDetailsDialog.parent.height : 700)
                             - legacyAddressEditor.implicitHeight - 200))
                contentWidth: availableWidth
                clip: true
                Label {
                    id: legacyDetailsLabel
                    width: legacyDetailsScroll.availableWidth
                    text: showPcDetailsDialog.pcDetails
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.label
                }
            }
            ColumnLayout {
                id: legacyAddressEditor
                Layout.fillWidth: true
                spacing: 8
                Label { text: qsTr("Connection address"); color: Theme.label }
                SLTextField {
                    id: detailsAddress
                    objectName: "starlight.hosts.connection-address"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Automatic")
                    accessibleName: qsTr("Connection address")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    onTextChanged: showPcDetailsDialog.addressStatus = ""
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Leave blank to use the discovered address, or enter a host address and optional port.")
                    wrapMode: Text.WordWrap
                    color: Theme.label2
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    SLButton {
                        objectName: "legacy-host-details-save-address"
                        text: qsTr("Save address")
                        onClicked: showPcDetailsDialog.saveAddress(detailsAddress.text)
                    }
                    SLButton {
                        objectName: "legacy-host-details-automatic-address"
                        text: qsTr("Use automatic")
                        onClicked: showPcDetailsDialog.saveAddress("")
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: showPcDetailsDialog.addressStatus
                    wrapMode: Text.WordWrap
                    color: Theme.label2
                    Accessible.name: text
                }
            }
        }
    }
}
