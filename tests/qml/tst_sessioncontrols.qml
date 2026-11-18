import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3

TestCase {
    id: testCase
    name: "SessionControls"
    when: windowShown
    property var panel: null
    property var session: null
    property var transfer: null
    property var controlsComponent: null
    property var paneComponent: null
    Component { id: launcherComponent; Window { width: 640; height: 480 } }

    Component {
        id: fakeTransfer
        QtObject {
            property bool supported: true
            property bool busy: false
            property bool queueActive: false
            property var queueEntries: []
            property string collisionName: ""
            property string completedLocalFile: ""
            property var remoteEntries: [{ name: "report.txt", relativePath: "work/report.txt", isDirectory: false }]
            property string error: ""
            property double progress: 0
            property double total: 0
            property int listCalls: 0
            property int sendCalls: 0
            property int receiveCalls: 0
            property int cancelCalls: 0
            property var sent: []
            property var received: []
            property var localUrlInputs: []
            signal pickerRequested(string action, string folder, string relativeFile)
            function listRemote(folder, directory) { ++listCalls }
            function localFileUrl(input) { localUrlInputs = localUrlInputs.concat([input]); return input }
            function sendFile(source, folder, directory, name, replace) { ++sendCalls; sent = [source, folder, directory, name, replace] }
            function receiveFile(folder, file, destination, replace) { ++receiveCalls; received = [folder, file, destination, replace] }
            function sendFiles(urls, folder, directory) {
                ++sendCalls
                var name = decodeURIComponent(urls[0].toString().split("/").pop())
                sent = [urls[0].toString(), folder, directory, name, false]
            }
            function resolveCollision(action) {}
            function revealReceivedFile() {}
            function cancel() { ++cancelCalls }
        }
    }
    Component {
        id: fakeSession
        QtObject {
            property bool controlsOpen: false
            property var fileTransfer: null
            property var controlState: ({ host: "Test host", app: "Desktop", format: "1920×1080", capture: true, muted: false, clipboardDirection: 1, safeTrial: true, hasVideo: false })
            property int closeCalls: 0
            property int clipboardPreparationCalls: 0
            property var actions: []
            signal fileDropRequested(url file)
            signal filesDropRequested(var files)
            function closeControls() { ++closeCalls; controlsOpen = false }
            function prepareControlsClipboard() { ++clipboardPreparationCalls }
            function controlAction(action, value) {
                actions = actions.concat([{ action: action, value: value }])
                if (action === "capture") controlState = Object.assign({}, controlState, { capture: value })
            }
        }
    }

    function initTestCase() {
        controlsComponent = Qt.createComponent(Qt.resolvedUrl("../../app/gui/SessionControls.qml"))
        paneComponent = Qt.createComponent(Qt.resolvedUrl("../../app/gui/FileTransferPane.qml"))
        verify(controlsComponent.status === Component.Ready, controlsComponent.errorString())
        verify(paneComponent.status === Component.Ready, paneComponent.errorString())
    }
    function init() {
        transfer = fakeTransfer.createObject(testCase)
        session = fakeSession.createObject(testCase, { fileTransfer: transfer })
        panel = controlsComponent.createObject(null, { session: session })
        verify(panel !== null, controlsComponent.errorString())
    }
    function cleanup() {
        if (panel) panel.destroy()
        panel = null
        if (session) session.destroy()
        if (transfer) transfer.destroy()
        session = null; transfer = null
        wait(0)
    }
    function named(name) {
        var child = findChild(panel.contentItem, name)
        verify(child !== null, "missing production control " + name)
        return child
    }
    function pane() { return named("starlight.controls.files") }
    function test_optimization_is_quiet_and_only_applies_on_explicit_action() {
        session.controlsOpen = true
        var apply = findChild(panel, "starlight.controls.apply-optimization")
        verify(apply !== null)
        verify(!apply.visible)
        session.controlState = Object.assign({}, session.controlState, {
            optimizationAvailable: true,
            optimizationNotice: "Try a smaller picture on the next connection. Apply now briefly reconnects."
        })
        tryCompare(apply, "visible", true)
        compare(session.actions.length, 0)
        wait(50)
        compare(session.actions.length, 0)
        mouseClick(apply)
        compare(session.actions.length, 1)
        compare(session.actions[0].action, "applyOptimization")
        session.controlState = Object.assign({}, session.controlState, { optimizationAvailable: false })
        tryCompare(apply, "visible", false)
    }

    function test_controls_follow_desktop_and_keep_readable_chrome() {
        session.controlsOpen = true
        tryCompare(panel, "visible", true)
        verify(panel.palette.window.hslLightness > .9) // Test desktop announces light mode.
        verify(panel.palette.windowText.hslLightness < .3)
        verify(panel.font.pixelSize >= 14)
        verify(named("starlight.controls.return").height >= 44)
        verify(named("starlight.controls.disconnect").height >= 44)
    }
    function openControls(tab) {
        session.controlsOpen = true
        tryCompare(panel, "visible", true)
        named("starlight.controls.tabs").currentIndex = tab === undefined ? 0 : tab
        tryCompare(pane(), "active", tab === 3)
        waitForRendering(panel.contentItem)
    }

    function test_multi_drop_copies_to_named_destination_and_release_is_explicit() {
        openControls()
        session.filesDropRequested(["file:///private/one.txt", "file:///private/two.txt"])
        compare(transfer.sendCalls, 1)
        compare(transfer.sent[1], "Downloads")
        compare(named("starlight.controls.tabs").currentIndex, 3)
        named("starlight.controls.release").clicked()
        compare(session.actions[session.actions.length-1], {action:"capture",value:false})
        compare(session.controlsOpen, false)
    }

    function test_default_session_view_keeps_file_pickers_inactive_and_return_visible() {
        openControls()
        compare(named("starlight.controls.tabs").currentIndex, 0)
        compare(transfer.listCalls, 0)
        compare(pane().createPicker(false, ""), null)
        var back = named("starlight.controls.return")
        verify(back.visible)
        verify(back.y >= 0)
        verify(back.mapToItem(panel.contentItem, 0, back.height).y <= panel.height)
        named("starlight.controls.tabs").currentIndex = 3
        tryCompare(pane(), "active", true)
        compare(transfer.listCalls, 1)
        var dialog = pane().createPicker(false, "")
        verify(dialog !== null)
        named("starlight.controls.tabs").currentIndex = 0
        tryCompare(pane(), "active", false)
        compare(pane().currentPicker, null)
    }

    function test_control_text_has_readable_contrast_against_the_window() {
        openControls()
        function luminance(color) {
            function linear(value) { return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4) }
            return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b)
        }
        var ground = panel.color
        var ink = named("starlight.controls.mute").contentItem.color
        var blended = Qt.rgba(ink.r * ink.a + ground.r * (1 - ink.a),
                              ink.g * ink.a + ground.g * (1 - ink.a),
                              ink.b * ink.a + ground.b * (1 - ink.a), 1)
        var a = luminance(ground), b = luminance(blended)
        verify((Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05) >= 4.5,
               "Control text must remain readable when the system palette and Material defaults differ")
    }

    function test_window_opens_dispatches_capture_choice_and_closes_without_overriding_intent() {
        openControls(1)
        var capture = named("starlight.controls.capture")
        verify(capture.checked)
        mouseClick(capture)
        compare(session.actions.length, 1)
        compare(session.actions[0].action, "capture")
        compare(session.actions[0].value, false)
        verify(!session.controlState.capture)
        mouseClick(named("starlight.controls.return"))
        compare(session.closeCalls, 1)
        tryCompare(panel, "visible", false)
        verify(!session.controlState.capture)
        compare(session.actions.length, 1, "closing must not silently recapture input")
        session.controlsOpen = true
        tryCompare(panel, "visible", true)
        panel.close()
        compare(session.closeCalls, 2)
        verify(!session.controlState.capture)
    }

    function test_ended_or_null_session_hidden_panel_accepts_close() {
        openControls()
        session.controlsOpen = false
        tryCompare(panel, "visible", false)
        verify(panel.close(), "a hidden ended-session window must not veto application quit")
        compare(session.closeCalls, 1)
        panel.session = null
        verify(panel.close(), "a hidden window with no session must not veto application quit")
        compare(panel.session, null)
    }

    function test_window_close_preserves_launcher_session_and_panel_reopens() {
        var launcher = launcherComponent.createObject(null)
        try {
            launcher.visible = true
            openControls()
            var originalSession = session
            verify(panel.close(), "normal tool-window close must be accepted")
            tryCompare(panel, "visible", false)
            compare(session.closeCalls, 1)
            verify(launcher.visible, "closing controls must leave the launcher alive")
            compare(panel.session, originalSession)
            compare(session.actions.length, 0, "closing controls must not change input choices")
            session.controlsOpen = true
            tryCompare(panel, "visible", true)
            compare(panel.session, originalSession)
        } finally {
            launcher.destroy()
        }
    }

    function test_controls_open_while_the_containing_stream_launcher_is_hidden() {
        panel.destroy()
        panel = null
        var launcher = launcherComponent.createObject(null)
        try {
            launcher.visible = true
            verify(waitForRendering(launcher.contentItem))
            panel = controlsComponent.createObject(launcher.contentItem, { session: session })
            verify(panel !== null, controlsComponent.errorString())
            launcher.visible = false
            session.controlsOpen = true
            tryCompare(panel, "visible", true)
            verify(waitForRendering(panel.contentItem))
            verify(!launcher.visible, "opening controls must preserve the hidden stream launcher")
            mouseClick(named("starlight.controls.return"))
            compare(session.closeCalls, 1)
            tryCompare(panel, "visible", false)
            verify(!launcher.visible)
        } finally {
            if (panel) panel.destroy()
            panel = null
            launcher.destroy()
        }
    }

    function test_live_controls_and_safe_keep_dispatch_the_actual_action() {
        openControls()
        var mute = named("starlight.controls.mute")
        mouseClick(mute)
        compare(session.actions[0].action, "mute")
        compare(session.actions[0].value, true)
        var direction = named("starlight.controls.clipboard-direction")
        direction.currentIndex = 3
        direction.activated(3)
        compare(session.actions[1].action, "clipboardDirection")
        compare(session.actions[1].value, 3)
        var keep = named("starlight.controls.keep-safe")
        verify(!keep.enabled)
        session.controlState = Object.assign({}, session.controlState, { hasVideo: true })
        tryCompare(keep, "enabled", true)
        keep.clicked()
        compare(session.actions[2].action, "keepSafeSettings")
    }

    function test_health_reads_live_figures_only_when_the_session_offers_them() {
        const health = findChild(panel, "starlight.controls.health")
        verify(health !== null)
        compare(health.visible, false, "a session without figures shows no invented ones")
        panel.liveHealth = { rttMs: 8, fps: 60, networkPoor: false }
        compare(panel.healthText, "8 ms · 60 FPS")
        compare(health.visible, true)
        panel.liveHealth = { rttMs: 80, fps: -1, networkPoor: true }
        verify(health.Accessible.name.indexOf("Slow") === 0)
        panel.liveHealth = { rttMs: -1, fps: -1, networkPoor: false }
    }

    function test_bitrate_stays_on_the_request_until_the_host_answers() {
        openControls()
        session.controlState = Object.assign({}, session.controlState, { liveBitrateSupported: true, liveBitrateKbps: 20000 })
        var slider = named("starlight.controls.bitrate")
        tryCompare(slider, "value", 20000)
        slider.stepBy(10)
        compare(slider.kbps, 30000, "keyboard and wheel steps move on every desktop style")
        tryVerify(function() { return session.actions.some(function(entry) { return entry.action === "bitrate" && entry.value === 30000 }) })
        // Any state change before the answer must not take the slider back.
        session.controlState = Object.assign({}, session.controlState, { liveBitrateKbps: 20000 })
        compare(slider.kbps, 30000)
        session.controlState = Object.assign({}, session.controlState, { liveBitrateKbps: 30000 })
        compare(slider.pendingKbps, -1)
        // A refusal explains itself and returns to what is really running.
        slider.stepBy(10)
        tryCompare(slider, "pendingKbps", 40000)
        session.controlState = Object.assign({}, session.controlState, { liveBitrateNotice: "The host refused." })
        compare(slider.kbps, 30000)
    }

    function test_bitrate_on_a_rebuilding_host_is_sent_once_per_release() {
        openControls()
        session.controlState = Object.assign({}, session.controlState,
            { liveBitrateSupported: true, liveBitrateKbps: 20000, liveBitrateRebuilds: true })
        var slider = named("starlight.controls.bitrate")
        tryCompare(slider, "value", 20000)
        waitForRendering(panel.contentItem)
        function sent() { return session.actions.filter(function(entry) { return entry.action === "bitrate" }).length }
        // Each value costs this host a frozen picture, so pausing mid-drag sends nothing.
        var y = slider.height / 2
        mousePress(slider, slider.width * 0.3, y)
        verify(slider.pressed)
        mouseMove(slider, slider.width * 0.4, y)
        wait(700)
        mouseMove(slider, slider.width * 0.5, y)
        wait(700)
        compare(sent(), 0)
        mouseRelease(slider, slider.width * 0.5, y)
        tryVerify(function() { return sent() === 1 }, 3000)
        wait(1500)
        compare(sent(), 1)
        // Key steps in a row settle into one request too.
        slider.stepBy(1); slider.stepBy(1); slider.stepBy(1)
        tryVerify(function() { return sent() === 2 }, 3000)
        wait(1500)
        compare(sent(), 2)
    }

    function test_drop_and_picker_stage_a_source_and_send_requires_explicit_confirmation() {
        openControls()
        session.fileDropRequested("file:///private/Test%20file.txt")
        var source = named("starlight.files.source")
        var remoteName = named("starlight.files.remote-name")
        // QUrl.toString() pretty-decodes spaces on some supported Qt versions.
        compare(decodeURI(source.text), "file:///private/Test file.txt")
        compare(remoteName.text, "Test file.txt")
        compare(transfer.sendCalls, 0)
        var dialog = pane().createPicker(false, "")
        verify(dialog !== null, pane().pickerError)
        verify(!dialog.visible, "construct native picker without opening any desktop browser")
        dialog.picked("file:///private/chosen.txt")
        compare(source.text, "file:///private/chosen.txt")
        compare(transfer.sendCalls, 0, "choosing a source must not start a transfer")
        var folder = named("starlight.files.remote-folder")
        folder.currentIndex = 1
        folder.activated(1)
        pane().remoteDirectory = "work"
        named("starlight.files.send").clicked()
        compare(transfer.sendCalls, 1)
        compare(transfer.localUrlInputs, ["file:///private/chosen.txt"])
        compare(transfer.sent, ["file:///private/chosen.txt", "Documents", "work", "chosen.txt", false])
    }

    function test_receive_requires_remote_selection_and_an_explicit_local_destination() {
        openControls(3)
        var receive = named("starlight.files.receive")
        verify(!receive.enabled)
        pane().selectedEntry = transfer.remoteEntries[0]
        verify(!receive.enabled)
        var dialog = pane().createPicker(true, "report.txt")
        verify(dialog !== null, pane().pickerError)
        verify(!dialog.visible)
        dialog.picked("file:///private/received.txt")
        compare(transfer.receiveCalls, 0)
        verify(receive.enabled)
        receive.clicked()
        compare(transfer.receiveCalls, 1)
        compare(transfer.localUrlInputs, ["file:///private/received.txt"])
        compare(transfer.received, ["Downloads", "work/report.txt", "file:///private/received.txt", false])
        transfer.busy = true
        verify(!receive.enabled)
        named("starlight.files.cancel").clicked()
        compare(transfer.cancelCalls, 1)
    }

    function test_closed_or_changed_owner_rejects_late_picker_results_and_requests() {
        openControls(3)
        var source = named("starlight.files.source")
        pane().useSource("file:///private/original.txt")
        var dialog = pane().createPicker(false, "")
        verify(dialog !== null)
        session.controlsOpen = false
        dialog.picked("file:///private/late.txt")
        compare(source.text, "file:///private/original.txt")
        compare(pane().currentPicker, null)
        transfer.pickerRequested("send", "Downloads", "")
        compare(pane().currentPicker, null)
        session.controlsOpen = true
        var second = pane().createPicker(false, "")
        var replacement = fakeTransfer.createObject(testCase)
        session.fileTransfer = replacement
        second.picked("file:///private/wrong-session.txt")
        compare(source.text, "")
        compare(pane().currentPicker, null)
        replacement.destroy()
    }

    function test_picker_invalidated_when_transfer_becomes_busy_or_unavailable() {
        openControls(3)
        var dialog = pane().createPicker(false, "")
        verify(dialog !== null)
        transfer.busy = true
        dialog.picked("file:///private/busy.txt")
        compare(named("starlight.files.source").text, "")
        compare(pane().currentPicker, null)
        compare(pane().createPicker(false, ""), null)
        transfer.busy = false
        var second = pane().createPicker(false, "")
        verify(second !== null)
        transfer.supported = false
        second.picked("file:///private/unsupported.txt")
        compare(named("starlight.files.source").text, "")
        compare(pane().currentPicker, null)
        compare(pane().createPicker(false, ""), null)
    }
}
