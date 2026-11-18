import QtQuick 2.15
import QtQuick.Window 2.15
import Starlight 1.0
import Starlight.Test 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import ComputerManager 1.0

// Drives the production window through named scenes. Each scene is a set-up
// function; the driver lets it settle, saves the frame, then undoes overlays.
QtObject {
    id: driver
    property var win: null
    property var stack: null
    property var host: TestHostSource { id: hostSource }
    property int sceneIndex: -1
    property var sceneList: []
    property bool failed: false
    readonly property string desktop: previewDesktop

    function find(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        var kids = item.children || []
        for (var i = 0; i < kids.length; ++i) {
            var hit = find(kids[i], name)
            if (hit) return hit
        }
        return null
    }
    function findOverlay(name) { return win ? find(win.toastItem.parent, name) : null }
    function route() { return stack ? stack.currentItem : null }
    function page() { var r = route(); return r ? r.page : null }
    function hosts() {
        return [
            { uuid: "mac", name: "Studio Mac", paired: true, online: true, serverSupported: true,
              serverSoftware: "Skylight", serverSoftwareLabel: "Moonwalk Host (0.4.0-alpha.15)",
              hostOs: "macos", address: "192.0.2.10", wakeable: true,
              desktopScaleSupported: true, adaptiveBitrateSupported: true,
              details: "Name: Studio Mac\nStatus: Online\nActive Address: 192.0.2.10\nUUID: 7C1A…\nMAC Address: 02:00:00:00:00:01\nPair State: Paired\nRunning Game: None\nHTTPS Port: 47984" },
            { uuid: "gaming-pc", name: "Gaming PC", paired: true, online: true, serverSupported: true,
              serverSoftware: "Apollo", serverSoftwareLabel: "Apollo", hostOs: "windows", address: "192.0.2.11", wakeable: true,
              apolloCompatible: true, virtualDisplaySupported: true, virtualDisplayReady: true,
              details: "Name: Gaming PC\nStatus: Online" },
            { uuid: "studio", name: "Studio PC", paired: true, online: false, serverSupported: true,
              serverSoftware: "Sunshine", hostOs: "windows", address: "192.0.2.12", wakeable: true,
              details: "Name: Studio PC\nStatus: Offline" },
            { uuid: "lounge", name: "Lounge", paired: false, online: true, serverSupported: true,
              serverSoftware: "Sunshine", hostOs: "linux", address: "192.0.2.13", wakeable: false,
              details: "Name: Lounge\nStatus: Online" }
        ]
    }
    function resetState() {
        hostSource.replaceHosts(hosts())
        hostSource.setRunningApp("mac", 0, "")
        hostSource.setDesktopAppId(1)
        StreamingPreferences.automaticResolution = true
        StreamingPreferences.width = 2880
        StreamingPreferences.height = 1800
        StreamingPreferences.bitrateKbps = 53000
        StreamingPreferences.automaticFrameRate = true
        StreamingPreferences.autoAdjustBitrate = true
        StreamingPreferences.adaptiveBitrate = false
        StreamingPreferences.manualStream = false
        StreamingPreferences.manualDisplay = false
        StreamingPreferences.manualInput = false
        StreamingPreferences.videoCodecConfig = 0
        StreamingPreferences.fps = 60
        if (route() && route().context) route().context.streamPlanner = null
        SystemProperties.nativeWidth = 2880
        SystemProperties.nativeHeight = 1800
        win.width = previewWidth > 0 ? previewWidth : 880
        win.height = previewHeight > 0 ? previewHeight : 600
    }
    // Menus and dialogs a scene opened are not children of what is left visible.
    function closePopups(item) {
        if (!item) return
        var kids = item.data || item.children || []
        for (var i = 0; i < kids.length; ++i) {
            var kid = kids[i]
            if (kid && kid.opened === true && kid.close) kid.close()
            if (kid && kid.data !== undefined) closePopups(kid)
            if (kid && kid.contentItem && kid.contentItem !== kid && kid.close) closePopups(kid.contentItem)
        }
    }
    function closeCombos(item) {
        if (!item) return
        if (item.popup && item.popup.opened) item.popup.close()
        var kids = item.children || []
        for (var i = 0; i < kids.length; ++i) closeCombos(kids[i])
    }
    function select(uuid) { page().selectHostByUuid(uuid) }
    function findDialog(name) {
        function walk(item) {
            if (!item) return null
            if (item.objectName === name) return item
            var kids = item.data || []
            for (var i = 0; i < kids.length; ++i) {
                var hit = walk(kids[i])
                if (hit) return hit
            }
            return null
        }
        return walk(win.contentItem)
    }
    property var laterAction: null
    property Timer laterTimer: Timer { interval: 300; onTriggered: driver.laterAction() }
    function later(action) { laterAction = action; laterTimer.restart() }
    function narrowDetails() {
        var item = find(win.contentItem, "starlight.hosts.item.mac")
        if (item) item.clicked()
    }
    function searchSettings(text) {
        var dialogRoute = win.preferencesPopup.settingsRoute
        var field = find(dialogRoute ? dialogRoute.legacySurface : win.contentItem, "starlight.settings.search")
                 || find(win.contentItem, "starlight.settings.search")
        if (field) field.text = text
        else console.error("no settings search field")
    }
    function clickByName(name) { var item = find(win.contentItem, name); if (item) item.clicked(); else console.error("no item", name) }
    // Scrolls the host page so the first of these controls that exists sits near the top.
    function scrollToControl(names) {
        var flick = find(win.contentItem, desktop === "plasma" ? "plasma-hosts-viewport" : "gnome-hosts-content-flickable")
        for (var i = 0; flick && i < names.length; ++i) {
            var item = find(flick.contentItem, names[i])
            if (!item || !item.visible) continue
            var y = item.mapToItem(flick.contentItem, 0, 0).y - 96
            flick.contentY = Math.max(0, Math.min(y, flick.contentHeight - flick.height))
            return
        }
        console.error("no control to scroll to", names)
    }
    function showDisplayAndInput(uuid) {
        select(uuid); win.height = 760
        later(function() { scrollToControl(["starlight.hosts.display-mode", "starlight.hosts.desktop-scale"]) })
    }

    property var currentSegue: null
    property var controlsPanel: null
    function openControls(tab) {
        var component = Qt.createComponent(previewControlsUrl)
        if (component.status !== Component.Ready) { console.error(component.errorString()); return }
        controlsPanel = component.createObject(null, { session: fakeControls, hostSeed: "mac", hostIcon: "os_macos" })
        driver.find(controlsPanel.contentItem, "starlight.controls.tabs").currentIndex = tab
    }
    property QtObject fakeSession: FakeSession {}
    // Answers in the shape of app/streaming/streamplan.h, for a host that can't
    // keep up with this display.
    property QtObject fakePlanner: QtObject {
        function plan(request) {
            var manualSize = request.width > 0
            return ({ width: request.width || 1920, height: request.height || 1200, fps: request.fps || 60,
                      codec: request.codec || "H.264", startBitrateKbps: 20000, ceilingKbps: 25000, floorKbps: 12000,
                      limitedByHost: !manualSize, limitReason: manualSize ? "" : "Gaming PC sustains 1920×1200 at 60 fps",
                      resolutionWarning: manualSize && request.width > 1920 ? "Gaming PC managed about 25 fps at this size last time" : "",
                      fpsWarning: "",
                      codecWarning: request.codec === "HEVC" ? "This computer can’t decode HEVC in hardware, so the picture will stutter" : "",
                      bitrateWarning: "" })
        }
    }
    property QtObject fakeControls: QtObject {
        property bool controlsOpen: true
        property var fileTransfer: null
        property var controlState: ({ host: "Studio Mac", app: "Desktop", format: "2880 × 1800 · 60 fps · 53.0 Mbps",
            viewScale: "fit", muted: false, capture: true, absolute: true, systemKeys: true, fullscreen: true,
            localText: false, clipboardDirection: 1, imageClipboard: true, clipboardReadAllowed: true,
            clipboardWriteAllowed: true, liveBitrateSupported: true, liveBitrateKbps: 53000, safeTrial: false })
        function liveStats() { return { rttMs: 8, presentedFrames: Date.now() * 0.06, networkPoor: false } }
        function prepareControlsClipboard() {}
        function closeControls() {}
        function controlAction(action, value) {}
    }
    readonly property var scenes: ({
        "hosts-ready": function() { select("mac") },
        "hosts-virtual-display": function() { select("gaming-pc") },
        "hosts-offline": function() { select("studio") },
        "hosts-unpaired": function() { select("lounge") },
        "hosts-running": function() { hostSource.setRunningApp("mac", 7, "Blender"); select("mac") },
        "hosts-tall": function() { win.height = 1000; select("mac") },
        "hosts-narrow": function() { win.width = 460; win.height = 700; select("mac") },
        "hosts-unsupported": function() { hostSource.updateHost("mac", { serverSupported: false }); select("mac") },
        "first-run-searching": function() { hostSource.replaceHosts([]) },
        "first-run-empty": function() { hostSource.replaceHosts([]); route().context.initialDiscoveryElapsed = true },
        "pairing": function() { select("lounge"); page().beginPairing() },
        "add-host": function() { page().requestAddHostOverlay() },
        "details": function() { select("mac"); page().requestDetails("mac") },
        "confirm-delete": function() { select("mac"); page().requestDelete("mac") },
        "rename": function() { select("mac"); page().requestRename("mac") },
        "error": function() { select("mac"); route().context.presentError("Could not reach this host.", "mac") },
        "settings": function() { page().requestOpenSettings() },
        "presets-filled": function() { select("mac"); StreamingPreferences.presetList = [{ id: "p1", name: "Travel" }, { id: "p2", name: "Office" }]; page().requestPresets("mac") },
        "message-dialog": function() {
            var component = Qt.createComponent(previewErrorUrl)
            if (component.status !== Component.Ready) { console.error(component.errorString()); return }
            var dialog = component.createObject(win.contentItem, { text: "No functioning hardware accelerated video decoder was detected by Moonwalk. Your streaming performance may be severely degraded in this configuration." })
            dialog.open()
            currentSegue = dialog
        },
        "toast": function() { select("mac"); win.showToast("Paired with Lounge") },
        "app-menu": function() { select("mac"); clickByName(desktop === "plasma" ? "plasma-app-menu-button" : "gnome-hosts-app-menu-button") },
        "host-menu": function() { select("mac"); clickByName(desktop === "plasma" ? "plasma-host-menu-button" : "gnome-hosts-host-menu-button") },
        "dropdown": function() { select("mac"); var c = find(win.contentItem, "starlight.hosts.resolution"); c.popup.open() },
        "automatic-limited": function() { route().context.streamPlanner = fakePlanner; select("gaming-pc"); win.height = 760 },
        "manual-stream": function() { select("mac"); win.height = 900; StreamingPreferences.manualStream = true; StreamingPreferences.autoAdjustBitrate = false; StreamingPreferences.bitrateKbps = 30000; find(win.contentItem, (desktop === "plasma" ? "plasma-hosts-viewport" : "gnome-hosts-content-flickable")).contentY = 260 },
        "manual-warnings": function() {
            route().context.streamPlanner = fakePlanner; select("gaming-pc"); win.height = 900
            StreamingPreferences.manualStream = true; StreamingPreferences.automaticResolution = false
            StreamingPreferences.automaticFrameRate = false; StreamingPreferences.videoCodecConfig = 2
            StreamingPreferences.autoAdjustBitrate = false; StreamingPreferences.bitrateKbps = 30000
            find(win.contentItem, (desktop === "plasma" ? "plasma-hosts-viewport" : "gnome-hosts-content-flickable")).contentY = 260
        },
        "display-input": function() { showDisplayAndInput("mac") },
        "display-input-apollo": function() { showDisplayAndInput("gaming-pc") },
        "display-input-manual": function() {
            StreamingPreferences.manualDisplay = true; StreamingPreferences.manualInput = true
            showDisplayAndInput("gaming-pc")
        },
        "about": function() { select("mac"); findDialog(desktop === "plasma" ? "plasma-about-dialog" : "gnome-about-dialog").open() },
        "shortcuts": function() { select("mac"); findDialog(desktop === "plasma" ? "plasma-keyboard-shortcuts-dialog" : "gnome-keyboard-shortcuts-dialog").open() },
        "narrow-shortcuts": function() { win.width = 440; win.height = 720; select("mac"); later(function() { findDialog(desktop === "plasma" ? "plasma-keyboard-shortcuts-dialog" : "gnome-keyboard-shortcuts-dialog").open() }) },
        "narrow-about": function() { win.width = 440; win.height = 720; select("mac"); later(function() { findDialog(desktop === "plasma" ? "plasma-about-dialog" : "gnome-about-dialog").open() }) },
        "left-buttons-first-host": function() {
            Appearance.windowButtonLayout = "close,minimize,maximize:"
            hostSource.replaceHosts([])
            later(function() {
                hostSource.replaceHosts(hosts())
                select("mac")
            })
        },
        "custom-resolution": function() { select("mac"); page().requestCustomResolution("mac") },
        "hosts-waking": function() { select("studio"); page().wakeHost("studio", true) },
        "hosts-long-name": function() { hostSource.updateHost("mac", { name: "A Remarkably Long Workstation Name In The Studio" }); select("mac") },
        "narrow-details": function() { win.width = 440; win.height = 720; later(narrowDetails) },
        "narrow-dialog": function() { win.width = 440; win.height = 720; select("mac"); page().requestAddHostOverlay() },
        "settings-search": function() { page().requestOpenSettings(); later(function() { searchSettings("mouse") }) },
        "stream-connecting": function() {
            var segue = win.createStreamSegue(fakeSession, false, "mac", "Desktop")
            stack.push(segue, 1)
            segue.spinnerShown = true
            segue.stageText = "Starting Desktop…"
            currentSegue = segue
        },
        "stream-failure": function() {
            var segue = win.createStreamSegue(fakeSession, false, "mac", "Desktop")
            stack.push(segue, 1)
            segue.lastFailure = "Video couldn't start. Check that the host is capturing a display, then try Safe Settings."
            segue.safeRetryRelevant = true
            segue.technicalFailure = "video stream establishment (error 11)"
            segue.showTerminalFailure()
            currentSegue = segue
        },
        "stream-connecting-stages": function() {
            var segue = win.createStreamSegue(fakeSession, false, "mac", "Desktop")
            stack.push(segue, 1)
            segue.spinnerShown = true
            for (var stage = 0; stage < 5; ++stage) segue.stageStarting("RTSP handshake")
            currentSegue = segue
        },
        "toast-action": function() { select("mac"); win.showToast("Paired with Lounge", "Connect", function() {}) },
        "controls-session": function() { openControls(0) },
        "controls-input": function() { openControls(1) },
        "controls-clipboard": function() { openControls(2) },
        "controls-files": function() { openControls(3) },
        "presets": function() { select("mac"); page().requestPresets("mac") }
    })
    property string pendingCleanup: ""
    function next() {
        if (win.visible === false) win.show()
        sceneIndex += 1
        if (sceneIndex >= sceneList.length) { Qt.exit(failed ? 1 : 0); return }
        settle.stop()
        // Back to a pristine host route before every scene.
        if (currentSegue) {
            if (currentSegue.close) { currentSegue.close(); currentSegue.destroy() }
            currentSegue = null
        }
        closePopups(win.contentItem)
        closeCombos(win.contentItem)
        while (stack.depth > 1) stack.pop(null, 1)
        win.preferencesPopup.close()
        var p = page()
        if (p && p.baseOverlay) {
            if (p.interruptOverlay.kind !== "none") p.dismissOverlay(p.interruptOverlay.operationId)
            if (p.baseOverlay.kind !== "none") p.dismissOverlay(p.baseOverlay.operationId)
        }
        resetState()
        prepare.restart()
    }
    function shoot() {
        var name = sceneList[sceneIndex]
        if (controlsPanel) {
            if (!supportInfo.grab(controlsPanel, previewOut + "/" + name + ".png")) {
                failed = true
                console.error("preview grab failed", name)
            }
            controlsPanel.destroy()
            controlsPanel = null
            Qt.callLater(driver.next)
            return
        }
        if (!supportInfo.grab(win, previewOut + "/" + name + ".png")) {
            failed = true
            console.error("preview grab failed", name)
        }
        Qt.callLater(driver.next)
    }
    property Timer prepare: Timer {
        interval: 120
        onTriggered: {
            // A scene that throws still has to reach the next one.
            try { driver.scenes[driver.sceneList[driver.sceneIndex]]() }
            catch (error) {
                driver.failed = true
                console.error("preview scene failed", driver.sceneList[driver.sceneIndex], error)
                Qt.callLater(driver.next)
                return
            }
            settle.restart()
        }
    }
    property Timer settle: Timer { id: settle; interval: 800; onTriggered: driver.shoot() }
    property Timer boot: Timer {
        interval: 500; running: true
        onTriggered: {
            driver.stack = driver.find(driver.win.contentItem, "starlight.shell.stack")
            driver.next()
        }
    }
    Component.onCompleted: {
        Appearance.colorScheme = previewScheme === "dark" ? 2 : 1
        Appearance.highContrast = previewContrast
        if (previewAccent.length > 0) Appearance.accentColor = previewAccent
        sceneList = previewScenes.length > 0 ? previewScenes.split(",") : Object.keys(scenes)
        var component = Qt.createComponent(previewMainUrl)
        if (component.status !== Component.Ready) { console.error(component.errorString()); Qt.exit(4); return }
        win = component.createObject(null, { hostSourceOverride: hostSource })
        if (!win) Qt.exit(5)
    }
}
