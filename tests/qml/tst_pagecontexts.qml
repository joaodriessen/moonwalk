import QtQuick 2.15
import QtTest 1.3
import Starlight.Test 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "../../app/gui/adaptive" as Adaptive

TestCase {
    id: testCase
    name: "PageContexts"

    function test_launch_reselects_the_last_host_once_it_appears() {
        StreamingPreferences.setLastSelectedHost("second")
        var context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "first", name: "First" }])
        wait(0)
        compare(context.selectedHostUuid, "first", "something is selected while hosts arrive")
        context.hostSource.appendHost({ uuid: "second", name: "Second" })
        tryCompare(context, "selectedHostUuid", "second")
        // Choosing a host wins from then on, and is what the next launch remembers.
        context.selectHostByUuid("first")
        compare(StreamingPreferences.lastSelectedHost(), "first")
        context.hostSource.appendHost({ uuid: "third", name: "Third" })
        wait(0)
        compare(context.selectedHostUuid, "first")
    }

    function test_mainDesktopPreservesTheRunningGameConfirmation() {
        var context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "main", name: "Host", online: true, paired: true}])
        context.hostSource.setDesktopAppId(101)
        context.hostSource.setRunningApp("main", 202, "Running game")
        wait(0)
        context.requestConnect("main", false)
        compare(context.hostSource.sessionDispatchCount, 0)
        compare(context.hostSource.quitDispatchCount, 0)
        compare(context.facade.baseOverlay.kind, "confirm-quit")
    }

    function test_retainedDesktopRequiresApprovalAndUsesLaunchInsteadOfGlobalQuit() {
        var context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "retained", name: "Host", online: true, paired: true,
            foreignDesktopRetained: true, desktopTakeoverAvailable: true}])
        context.hostSource.setDesktopAppId(101)
        wait(0)
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects.push(effect) })
        context.requestConnect("retained", false)
        compare(context.facade.baseOverlay.kind, "confirm-quit")
        verify(context.facade.baseOverlay.payload.retainedDesktopTakeover)
        compare(context.hostSource.sessionDispatchCount, 0)
        context.confirmQuitAndConnect(context.facade.baseOverlay.operationId)
        compare(context.hostSource.sessionDispatchCount, 1)
        verify(context.hostSource.lastDesktopTakeover)
        compare(context.hostSource.quitDispatchCount, 0)
        compare(effects[effects.length-1].kind, "launch")
    }

    function test_decliningRetainedDesktopDoesNotCreateOrEndASession() {
        var context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "retained", name: "Host", online: true, paired: true,
            foreignDesktopRetained: true, desktopTakeoverAvailable: true}])
        context.hostSource.setDesktopAppId(101)
        wait(0)
        context.requestConnect("retained", false)
        context.dismissOverlay(context.facade.baseOverlay.operationId)
        compare(context.hostSource.sessionDispatchCount, 0)
        compare(context.hostSource.quitDispatchCount, 0)
        verify(!context.hostSource.lastDesktopTakeover)
    }

    function test_retainedDesktopWithoutAControlIsNotSilentlyJoined() {
        var context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "retained", name: "Host", online: true, paired: true,
            foreignDesktopRetained: true, desktopTakeoverAvailable: false}])
        context.hostSource.setDesktopAppId(101)
        wait(0)
        context.requestConnect("retained", false)
        compare(context.hostSource.sessionDispatchCount, 0)
        compare(context.hostSource.quitDispatchCount, 0)
        compare(context.interruptOverlay.kind, "error")
    }

    function cleanup() {
        StreamingPreferences.reset()
        SystemProperties.currentDisplay = 0
        SystemProperties.displays = []
        SystemProperties.nativeWidth = 1920
        SystemProperties.nativeHeight = 1080
        SystemProperties.nativeRefreshRate = 60
    }

    Component {
        id: hostContextComponent
        Adaptive.HostPageContext {
            hostSource: TestHostSource { }
        }
    }

    Component {
        id: prepopulatedHostContextComponent
        Adaptive.HostPageContext {
            hostSource: TestHostSource {
                Component.onCompleted: replaceHosts([
                    { uuid: "ready", name: "Ready", serverSoftware: "Skylight" }
                ])
            }
        }
    }

    Component {
        id: refreshedSystemPropertiesComponent
        QtObject {
            property var displayState: ({ width: 1920, height: 1080 })
            property bool hasHwHevcOrAv1Decode: true
            property int refreshDisplayCalls: 0

            function refreshDisplays() {
                refreshDisplayCalls += 1
                // Mutate the opaque display cache exactly as the production
                // C++ invokable does: there is no QML NOTIFY dependency.
                displayState.width = 2880
                displayState.height = 1800
            }

            function getNativeResolution() {
                return Qt.size(displayState.width, displayState.height)
            }
        }
    }

    Component {
        id: computerManagerComponent
        QtObject {
            property int addDispatchCount: 0
            property int nextAttempt: 1
            function reserveManualAddAttemptId() { return String(nextAttempt++) }
            function cancelManualAdd(attemptId) {}
            function isValidHostAddress(address) { return address !== "not valid host" }
            function addNewHostManually(address) { addDispatchCount += 1 }
        }
    }

    function test_registered_test_source_replaces_the_default_host_model() {
        const injected = createTemporaryObject(hostContextComponent, testCase)
        verify(injected.hostSource instanceof TestHostSource)
        compare(injected.activeHostSource, injected.hostSource)
        injected.destroy()
    }

    function test_automatic_selection_initializes_each_host_before_preference_use() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.preferences.reset()
        context.systemProperties.nativeWidth = 2560
        context.systemProperties.nativeHeight = 1600
        context.systemProperties.hasHwHevcOrAv1Decode = true
        const refreshesBefore = context.systemProperties.refreshDisplayCalls
        context.hostSource.replaceHosts([{ uuid: "skylight", name: "Sky", serverSoftware: "Skylight" }])
        compare(context.facade.selectedHostUuid, "skylight")
        compare(context.preferences.currentHostHistory[context.preferences.currentHostHistory.length - 1], "skylight")
        compare(context.preferences.width, 2560)
        compare(context.preferences.height, 1600)
        verify(context.preferences.localCursor)
        verify(context.preferences.absoluteMouseMode)
        verify(context.systemProperties.refreshDisplayCalls > refreshesBefore)

        context.hostSource.replaceHosts([{ uuid: "stock", name: "Stock", serverSoftware: "Sunshine" }])
        compare(context.facade.selectedHostUuid, "stock")
        compare(context.preferences.currentHostHistory[context.preferences.currentHostHistory.length - 1], "stock")
        compare(context.preferences.width, 2560)
        compare(context.preferences.height, 1600)
        verify(!context.preferences.localCursor)
        verify(!context.preferences.absoluteMouseMode)

        context.hostSource.replaceHosts([])
        context.hostSource.appendHost({ uuid: "inserted", name: "Inserted", serverSoftware: "Skylight" })
        compare(context.facade.selectedHostUuid, "inserted")
        compare(context.preferences.currentHostHistory[context.preferences.currentHostHistory.length - 1], "inserted")
        verify(context.preferences.hasStoredResolution("inserted"))
        context.systemProperties.nativeWidth = 1920
        context.systemProperties.nativeHeight = 1080
        context.systemProperties.hasHwHevcOrAv1Decode = true
        context.destroy()
    }

    function test_fresh_skylight_host_initializes_native_mode_before_cursor_save() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.preferences.reset()
        context.systemProperties.nativeWidth = 2880
        context.systemProperties.nativeHeight = 1800
        context.systemProperties.nativeRefreshRate = 60

        context.hostSource.replaceHosts([
            { uuid: "fresh", name: "Fresh", serverSoftware: "Skylight" }
        ])

        compare(context.preferences.initializationHistory.length, 1)
        compare(context.preferences.initializationHistory[0].uuid, "fresh")
        compare(context.preferences.initializationHistory[0].width, 2880)
        compare(context.preferences.initializationHistory[0].height, 1800)
        compare(context.preferences.initializationHistory[0].fps, 60)
        compare(context.preferences.width, 2880)
        compare(context.preferences.height, 1800)
        compare(context.preferences.fps, 60)
        const initializeIndex = context.preferences.operationHistory.indexOf("initialize:fresh")
        const saveIndex = context.preferences.operationHistory.indexOf("save:fresh")
        verify(initializeIndex >= 0)
        verify(saveIndex > initializeIndex)
        context.preferences.reset()
        context.systemProperties.nativeWidth = 1920
        context.systemProperties.nativeHeight = 1080
        context.systemProperties.nativeRefreshRate = 60
        context.destroy()
    }

    function test_prepopulated_source_is_reconciled_after_host_list_loads() {
        StreamingPreferences.reset()
        const context = createTemporaryObject(prepopulatedHostContextComponent, testCase)
        compare(context.facade.hosts.rowCount(), 1)
        compare(context.facade.selectedHostUuid, "ready")
        compare(context.facade.selectedHostIndex, 0)
        compare(context.preferences.currentHostHistory[
                    context.preferences.currentHostHistory.length - 1], "ready")
        verify(context.preferences.localCursor)
        verify(context.preferences.absoluteMouseMode)
        compare(context.preferences.saveCalls, 1)
        context.destroy()
    }

    function test_resolution_options_rebuild_after_display_refresh() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const displays = createTemporaryObject(refreshedSystemPropertiesComponent, context)
        context.systemProperties = displays
        compare(context.facade.resolutionOptions[0].w, 1920)

        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])

        compare(displays.getNativeResolution().width, 2880)
        compare(context.facade.resolutionOptions[0].w, 2880)
        compare(context.facade.resolutionOptions[0].h, 1800)
        context.destroy()
    }

    function test_inactive_model_reset_defers_selection_and_preference_initialization() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        compare(context.facade.selectedHostUuid, "a")
        context.routeActive = false
        context.preferences.reset()
        const refreshesBefore = context.systemProperties.refreshDisplayCalls

        context.hostSource.replaceHosts([{ uuid: "b", name: "Beta" }])

        compare(context.facade.selectedHostUuid, "a")
        compare(context.preferences.currentHostHistory.length, 0)
        compare(context.systemProperties.refreshDisplayCalls, refreshesBefore)

        context.routeActive = true
        compare(context.facade.selectedHostUuid, "b")
        compare(context.preferences.currentHostHistory[
                    context.preferences.currentHostHistory.length - 1], "b")
        verify(context.systemProperties.refreshDisplayCalls > refreshesBefore)
        context.destroy()
    }

    function test_reset_preserves_uuid_or_selects_first() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        verify(context !== null)
        context.hostSource.replaceHosts([
                    { uuid: "a", name: "Alpha" },
                    { uuid: "b", name: "Beta" }])
        context.facade.selectHostByUuid("b")
        compare(context.facade.selectedHostUuid, "b")
        context.hostSource.replaceHosts([
                    { uuid: "c", name: "Charlie" },
                    { uuid: "b", name: "Beta" }])
        compare(context.facade.selectedHostUuid, "b")
        context.hostSource.replaceHosts([{ uuid: "c", name: "Charlie" }])
        compare(context.facade.selectedHostUuid, "c")
        context.destroy()
    }

    function test_pairing_cancel_uses_backend_attempt_identity_and_deactivation_cancels_once() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.computerManager.reset()
        context.nextOperationId = 50000
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        const attempt = context.facade.baseOverlay.payload.attemptId
        const operation = context.facade.baseOverlay.operationId
        verify(attempt !== operation)
        context.facade.cancelPairing(operation)
        compare(context.facade.baseOverlay.kind, "pairing",
                "an unrelated overlay identity must not cancel a backend attempt")
        compare(context.computerManager.cancelledPairingAttempts, [])
        context.facade.cancelPairing(attempt)
        compare(context.facade.baseOverlay.kind, "none")
        compare(context.computerManager.cancelledPairingAttempts, [attempt])
        context.facade.cancelPairing(attempt)
        compare(context.computerManager.cancelledPairingAttempts, [attempt])
        context.facade.beginPairing()
        const next = context.facade.baseOverlay.payload.attemptId
        context.facade.testConnection()
        context.routeActive = false
        compare(context.computerManager.cancelledPairingAttempts, [attempt, next])
        compare(context.hostSource.connectionTestDispatchCount, 0,
                "deactivation must not promote queued work while cancelling")
        context.hostSource.completePairingAttempt(next, "late failure")
        compare(context.facade.interruptOverlay.kind, "none")
        context.computerManager.reset()
        context.destroy()
    }

    function test_pairing_cancelled_completion_cannot_close_newer_pairing() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        const first = context.facade.baseOverlay.payload.attemptId
        const firstOperation = context.facade.baseOverlay.operationId
        compare(context.hostSource.pairDispatchCount, 1)
        context.facade.cancelPairing(first)
        context.facade.beginPairing()
        const second = context.facade.baseOverlay.operationId
        verify(second !== firstOperation)
        context.hostSource.completePairing("late failure")
        compare(context.facade.baseOverlay.operationId, second)
        context.hostSource.completePairing()
        compare(context.facade.baseOverlay.kind, "none")
        context.destroy()
    }

    function test_pairing_retry_completion_can_arrive_before_cancelled_attempt() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        const firstAttempt = context.facade.baseOverlay.payload.attemptId
        context.facade.cancelPairing(firstAttempt)
        context.facade.beginPairing()
        const secondAttempt = context.facade.baseOverlay.payload.attemptId
        verify(secondAttempt !== firstAttempt)

        context.hostSource.completePairingAttempt(secondAttempt)
        compare(context.facade.baseOverlay.kind, "none")
        context.hostSource.completePairingAttempt(firstAttempt, "old failure")
        compare(context.facade.baseOverlay.kind, "none")
        context.destroy()
    }

    function test_connection_test_is_single_flight_and_retains_result_overlay() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.testConnection()
        context.facade.testConnection()
        compare(context.hostSource.connectionTestDispatchCount, 1)
        context.hostSource.completeConnectionTest(0, "47984")
        compare(context.facade.baseOverlay.kind, "connection-test")
        compare(context.facade.baseOverlay.payload.blockedPorts, "47984")
        context.destroy()
    }

    function test_native_and_fresh_host_mode_follow_current_monitor() {
        SystemProperties.displays = [{width: 1920, height: 1080, rate: 60}, {width: 2560, height: 1440, rate: 120}]
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "a", name: "First"}])
        context.facade.setResolution(1600, 900)
        SystemProperties.currentDisplay = 1
        SystemProperties.displayConfigurationChanged()
        compare(context.facade.resolutionOptions[0].w, 2560)
        compare(context.preferences.width, 1600)
        compare(context.preferences.height, 900)
        context.hostSource.replaceHosts([{uuid: "b", name: "New"}])
        compare(context.preferences.width, 2560)
        compare(context.preferences.height, 1440)
        compare(context.preferences.fps, 120)
    }

    function test_route_recreation_rejects_previous_add_and_test_completions() {
        const manager = createTemporaryObject(computerManagerComponent, testCase)
        var context = hostContextComponent.createObject(testCase, {computerManager: manager})
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("old")
        const oldAdd = context.facade.baseOverlay.payload.attemptId
        context.destroy()
        context = createTemporaryObject(hostContextComponent, testCase, {computerManager: manager})
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("new")
        context.completeAddHost(oldAdd, true, false)
        compare(context.facade.baseOverlay.payload.address, "new")
        context.facade.dismissOverlay(context.facade.baseOverlay.operationId)
        context.facade.testConnection()
        const oldTest = context.connectionTestAttemptId
        context.routeActive = false
        context.routeActive = true
        context.facade.testConnection()
        context.completeConnectionTest(oldTest, 0, "old")
        verify(context.connectionTestInFlight)
        compare(context.facade.baseOverlay.payload.blockedPorts, "")
    }

    function test_recommendations_and_lock_changes_use_effective_bitrate() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{uuid: "a", name: "First"}])
        context.preferences.stubDefaultBitrate = 900000
        context.preferences.width = 8192
        context.preferences.height = 8192
        context.preferences.fps = 9999
        context.preferences.enableYUV444 = true
        context.facade.setAutoAdjustBitrate(false)
        context.facade.setAutoAdjustBitrate(true)
        compare(context.preferences.bitrateKbps, 150000)
    }

    function test_cancelled_connection_test_does_not_complete_retry() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.facade.testConnection()
        const first = context.connectionTestAttemptId
        context.facade.dismissOverlay(context.facade.baseOverlay.operationId)
        context.facade.testConnection()
        const second = context.connectionTestAttemptId
        verify(second !== first)
        compare(context.hostSource.connectionTestDispatchCount, 2)
        context.completeConnectionTest(first, 0, "old")
        verify(context.connectionTestInFlight)
        context.completeConnectionTest(second, 0, "new")
        compare(context.facade.baseOverlay.payload.blockedPorts, "new")
    }

    function test_cancelled_add_host_does_not_change_retry() {
        const manager = createTemporaryObject(computerManagerComponent, testCase)
        const context = createTemporaryObject(hostContextComponent, testCase, {computerManager: manager})
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("first")
        const first = context.facade.baseOverlay.payload.attemptId
        context.facade.dismissOverlay(context.facade.baseOverlay.operationId)
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("second")
        const second = context.facade.baseOverlay.payload.attemptId
        verify(first !== second)
        context.completeAddHost(first, true, false)
        compare(context.facade.baseOverlay.payload.address, "second")
        context.completeAddHost(first, false, true)
        compare(context.facade.baseOverlay.payload.error, undefined)
        context.completeAddHost(second, true, false)
        compare(context.facade.baseOverlay.kind, "none")
    }

    function test_manual_bitrate_can_return_to_automatic_default() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.preferences.reset()
        context.facade.setBitrateKbps(12345)
        compare(context.facade.hostPreferenceSnapshot.autoAdjustBitrate, false)
        context.facade.setAutoAdjustBitrate(true)
        compare(context.facade.hostPreferenceSnapshot.autoAdjustBitrate, true)
        compare(context.facade.hostPreferenceSnapshot.bitrateKbps,
                context.preferences.getDefaultBitrate(context.preferences.width,
                    context.preferences.height, context.preferences.fps,
                    context.preferences.enableYUV444))
        context.destroy()
    }

    function test_quality_modes_save_one_coherent_choice_and_reject_unsupported_adaptation() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", adaptiveBitrateSupported: true }])
        context.preferences.reset()
        context.facade.setBitrateKbps(12345)
        compare(context.preferences.saveCalls, 1)
        context.facade.setQualityMode("adaptive")
        verify(context.preferences.adaptiveBitrate)
        verify(context.preferences.autoAdjustBitrate)
        compare(context.preferences.saveCalls, 2)
        const ceiling = context.preferences.bitrateKbps
        context.facade.setQualityMode("manual")
        verify(!context.preferences.adaptiveBitrate)
        verify(!context.preferences.autoAdjustBitrate)
        compare(context.preferences.bitrateKbps, ceiling)
        compare(context.preferences.saveCalls, 3)
        context.facade.setQualityMode("recommended")
        verify(!context.preferences.adaptiveBitrate)
        verify(context.preferences.autoAdjustBitrate)
        compare(context.preferences.saveCalls, 4)
        context.facade.setQualityMode("recommended")
        context.facade.setQualityMode("unknown")
        compare(context.preferences.saveCalls, 4)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", adaptiveBitrateSupported: false }])
        context.facade.setQualityMode("adaptive")
        verify(!context.preferences.adaptiveBitrate)
        compare(context.preferences.saveCalls, 4)
        context.destroy()
    }

    function test_view_scale_is_local_per_host_and_rejects_unrecognized_modes() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.preferences.reset()
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }, { uuid: "b", name: "Beta" }])
        context.facade.selectHostByUuid("a")
        const savesBefore = context.preferences.saveCalls
        const resolution = [context.preferences.width, context.preferences.height]
        context.facade.setViewScale("actual")
        compare(context.facade.hostPreferenceSnapshot.viewScale, "actual")
        compare(context.preferences.saveCalls, savesBefore + 1)
        context.facade.setViewScale("actual")
        context.facade.setViewScale("match-window")
        compare(context.preferences.saveCalls, savesBefore + 1)
        compare([context.preferences.width, context.preferences.height], resolution)
        context.facade.selectHostByUuid("b")
        compare(context.facade.hostPreferenceSnapshot.viewScale, "fit")
        context.facade.selectHostByUuid("a")
        compare(context.facade.hostPreferenceSnapshot.viewScale, "actual")
        context.hostSource.replaceHosts([])
        context.facade.setViewScale("fit")
        compare(context.preferences.saveCalls, savesBefore + 1, "no preference write without the selected host")
        context.destroy()
    }

    function test_details_address_save_targets_only_its_captured_host_and_operation() {
        const manager = createTemporaryObject(computerManagerComponent, testCase)
        const context = createTemporaryObject(hostContextComponent, testCase, { computerManager: manager })
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }, { uuid: "b", name: "Beta" }])
        context.facade.selectHostByUuid("a")
        context.facade.requestDetails("a")
        const operation = context.facade.baseOverlay.operationId
        verify(!context.facade.submitConnectionAddress(operation + 1, "wrong.example"))
        compare(context.preferences.saveCalls, 0)
        verify(context.facade.submitConnectionAddress(operation, "  mac.example:47989  "))
        compare(context.preferences.connectionAddress, "mac.example:47989")
        compare(context.facade.baseOverlay.payload.connectionAddress, "mac.example:47989")
        compare(context.facade.baseOverlay.kind, "details")
        compare(context.preferences.saveCalls, 1)
        context.facade.selectHostByUuid("b")
        verify(!context.facade.submitConnectionAddress(operation, "wrong.example"))
        compare(context.preferences.saveCalls, 1)
        context.destroy()
    }

    function test_error_dismissal_restores_only_the_current_base_overlay() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        context.hostSource.emitAsyncError("wake failed", "a")
        context.hostSource.completePairing()
        compare(context.facade.interruptOverlay.kind, "error")
        context.facade.dismissOverlay(context.facade.interruptOverlay.operationId)
        compare(context.facade.baseOverlay.kind, "none")
        context.destroy()
    }

    function test_manual_choices_make_the_host_manual_and_automatic_resets_them_in_one_save() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", adaptiveBitrateSupported: true }])
        context.preferences.reset()
        context.facade.setBitrateKbps(20000)
        verify(context.preferences.manualStream)
        compare(context.preferences.autoAdjustBitrate, false)
        compare(context.preferences.saveCalls, 1)
        context.facade.setVideoCodec(context.preferences.VCC_FORCE_HEVC)
        context.facade.setFrameRate(30)
        compare(context.preferences.saveCalls, 3)
        context.facade.setBandwidthLimit(false)
        verify(context.preferences.autoAdjustBitrate && context.preferences.adaptiveBitrate)
        verify(context.preferences.manualStream, "Limit bandwidth off is still Manual")
        compare(context.preferences.saveCalls, 4)
        context.facade.setStreamMode("automatic")
        compare(context.preferences.saveCalls, 5)
        verify(!context.preferences.manualStream)
        verify(context.preferences.automaticResolution && context.preferences.automaticFrameRate)
        compare(context.preferences.fps, 60)
        compare(context.preferences.videoCodecConfig, context.preferences.VCC_AUTO)
        compare(context.facade.hostPreferenceSnapshot.manualStream, false)
        context.facade.setStreamMode("automatic")
        context.facade.setStreamMode("custom")
        compare(context.preferences.saveCalls, 5)
        context.facade.setStreamMode("manual")
        verify(context.preferences.manualStream)
        compare(context.preferences.saveCalls, 6)
        context.destroy()
    }

    // Automatic keeps the Manual choices for later; any choice of the person's
    // own makes its group Manual in the same, single save.
    function test_display_and_input_modes_save_once_and_keep_manual_choices() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", desktopScaleSupported: true }])
        context.preferences.reset()
        const saves = context.preferences.saveCalls
        context.facade.setDisplayMode("automatic")
        context.facade.setInputMode("automatic")
        compare(context.preferences.saveCalls, saves, "already Automatic")
        context.facade.setDisplayMode("manual")
        verify(context.preferences.manualDisplay)
        compare(context.facade.hostPreferenceSnapshot.manualDisplay, true)
        compare(context.preferences.saveCalls, saves + 1)
        context.facade.setDisplayMode("manual")
        context.facade.setDisplayMode("custom")
        compare(context.preferences.saveCalls, saves + 1)
        context.facade.setViewScale("actual")
        compare(context.preferences.saveCalls, saves + 2)
        context.facade.setDisplayMode("automatic")
        verify(!context.preferences.manualDisplay)
        compare(context.preferences.viewScale, "actual", "kept for when Manual comes back")
        compare(context.preferences.saveCalls, saves + 3)
        context.facade.setDesktopScale("2")
        verify(context.preferences.manualDisplay)
        compare(context.preferences.saveCalls, saves + 4)

        context.facade.setLocalCursorEnabled(true)
        verify(context.preferences.manualInput && context.preferences.localCursor)
        compare(context.preferences.saveCalls, saves + 5)
        context.facade.setInputMode("automatic")
        verify(!context.preferences.manualInput)
        compare(context.preferences.saveCalls, saves + 6)
        // Already the saved choice, but now a Manual one: still one save.
        context.facade.setLocalCursorEnabled(true)
        verify(context.preferences.manualInput)
        compare(context.preferences.saveCalls, saves + 7)
        context.facade.setInputMode("automatic")
        context.facade.setAbsoluteMouseMode(true)
        verify(context.preferences.manualInput && !context.preferences.automaticPointer)
        compare(context.preferences.saveCalls, saves + 9)
        context.facade.setInputMode("manual")
        compare(context.preferences.saveCalls, saves + 9)
        context.facade.setLocalTextInput(true)
        compare(context.preferences.saveCalls, saves + 10)
        context.facade.setInputMode("automatic")
        context.facade.setLocalTextInput(false)
        verify(!context.preferences.manualInput, "the keyboard layout isn't part of Automatic")
        context.destroy()
    }

    function test_choices_stop_at_what_this_display_can_show() {
        SystemProperties.nativeWidth = 2880
        SystemProperties.nativeHeight = 1800
        SystemProperties.nativeRefreshRate = 60
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        const sizes = context.facade.resolutionOptions.filter(function(option) { return option.w > 0 })
        sizes.forEach(function(option) {
            verify(option.w <= 2880 && option.h <= 1800, option.text + " fits this display")
        })
        verify(sizes.some(function(option) { return option.w === 1920 && option.h === 1200 }),
               "the same shape, smaller")
        const rates = context.facade.frameRateOptions.filter(function(option) { return option.value > 0 })
        compare(rates.map(function(option) { return option.value }), [30, 60])
        const saves = context.preferences.saveCalls
        context.facade.setResolution(3840, 2160)
        context.facade.setFrameRate(120)
        compare(context.preferences.saveCalls, saves, "beyond the display is refused")
        context.facade.requestCustomFrameRate("a")
        compare(context.facade.baseOverlay.payload.maximum, 60)
        context.facade.submitCustomFrameRate(context.facade.baseOverlay.operationId, 120)
        compare(context.preferences.fps, 60)
        context.facade.dismissOverlay(context.facade.baseOverlay.operationId)

        // A faster display offers its own rate.
        SystemProperties.nativeRefreshRate = 144
        context.refreshDisplayMetadata()
        const fast = context.facade.frameRateOptions.filter(function(option) { return option.value > 0 })
        compare(fast.map(function(option) { return option.value }), [30, 60, 90, 120, 144])
        context.destroy()
    }

    function test_saved_setups_load_inside_this_display() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.preferences.presetValues = ({ big: { width: 3840, height: 2160, fps: 240, manualStream: true,
                                                     automaticResolution: false, automaticFrameRate: false } })
        verify(context.facade.applyPreset("big"))
        verify(context.preferences.automaticFrameRate, "240 fps on a 60 Hz display loads as Automatic")
        compare(context.preferences.fps, 60)
        context.destroy()
    }

    // The stream plan is bound in one place; its answers reach the facade and
    // follow every choice as it changes.
    function test_stream_plan_follows_each_choice() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const planner = createTemporaryObject(plannerComponent, testCase)
        context.streamPlanner = planner
        context.hostSource.replaceHosts([{ uuid: "mini", name: "Mini PC" }])
        context.preferences.reset()
        compare(planner.lastRequest.hostUuid, "mini")
        compare(planner.lastRequest.width, 0, "Automatic asks with no size of its own")
        compare(context.facade.streamPlan.limitReason, "Mini PC sustains 1920×1200 at 60 fps")
        compare(context.facade.streamPlan.fpsWarning, "")
        context.facade.setFrameRate(30)
        compare(planner.lastRequest.fps, 30)
        compare(context.facade.streamPlan.fpsWarning, "Mini PC managed about 25 fps at this size last time")
        context.facade.setVideoCodec(context.preferences.VCC_FORCE_HEVC)
        compare(planner.lastRequest.codec, "HEVC")
        verify(context.facade.streamPlan.codecWarning.indexOf("HEVC") >= 0)
        compare(context.facade.streamPlan.floorKbps, 12000)
        context.facade.setBitrateKbps(5000)
        compare(context.preferences.bitrateKbps, 12000, "the limit never goes below the plan's floor")
        context.facade.setStreamMode("automatic")
        compare(planner.lastRequest.fps, 0)
        compare(context.facade.streamPlan.fpsWarning, "")
        context.destroy()
    }

    Component {
        id: plannerComponent
        // The shape of StreamPlan in app/streaming/streamplan.h.
        QtObject {
            property var lastRequest: ({})
            function plan(request) {
                lastRequest = request
                return ({ width: request.width || 1920, height: request.height || 1200, fps: request.fps || 60,
                          codec: request.codec || "H.264", startBitrateKbps: 20000, ceilingKbps: 25000, floorKbps: 12000,
                          limitedByHost: !request.width, limitReason: request.width ? "" : "Mini PC sustains 1920×1200 at 60 fps",
                          resolutionWarning: "",
                          fpsWarning: request.fps === 30 ? "Mini PC managed about 25 fps at this size last time" : "",
                          codecWarning: request.codec === "HEVC" ? "This computer can't decode HEVC in hardware, so the picture will stutter" : "",
                          bitrateWarning: "" })
            }
        }
    }

    function test_vibepollo_capabilities_enable_quality_and_explain_launch_permissions() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.preferences.reset()
        context.hostSource.replaceHosts([{ uuid: "pc", name: "Windows", serverSoftware: "Apollo",
            apolloCompatible: true, virtualDisplaySupported: true, virtualDisplayReady: true,
            launchAllowed: false }])
        verify(context.facade.selectedHostSnapshot.adaptiveBitrateSupported)
        verify(context.facade.selectedHostSnapshot.virtualDisplayReady)
        context.facade.setDisplayTarget("virtual")
        compare(context.preferences.displayTarget, "virtual")
        context.facade.setAutomaticPointer(true)
        verify(context.preferences.automaticPointer)
        context.facade.setAbsoluteMouseMode(false)
        verify(!context.preferences.automaticPointer)
        verify(!context.preferences.absoluteMouseMode)
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects = effects.concat([effect]) })
        context.facade.requestConnect("pc")
        compare(effects.length, 0)
        compare(context.facade.interruptOverlay.kind, "error")
        verify(context.facade.interruptOverlay.payload.message.indexOf("Launch applications") >= 0)
        context.preferences.reset()
        context.destroy()
    }

    function test_connect_requires_active_route_and_exposes_scalar_snapshot_options() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", serverSoftware: "Skylight" }])
        compare(context.facade.selectedHostSnapshot.uuid, "a")
        compare(context.facade.selectedHostSnapshot.name, "Alpha")
        compare(context.facade.selectedHostKind, "skylight")
        compare(context.facade.resolutionOptions.length, 5)
        compare(context.facade.frameRateOptions.length, 4)
        verify(context.facade.frameRateOptions[0].automatic)
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects = effects.concat([effect]) })
        context.routeActive = false
        context.facade.requestConnect("a")
        compare(effects.length, 0)
        context.routeActive = true
        context.facade.requestConnect("a")
        compare(effects.length, 1)
        compare(effects[0].kind, "launch")
        compare(effects[0].payload.hostUuid, "a")
        compare(effects[0].payload.desktop, true)
        compare(effects[0].payload.resume, false)
        compare(effects[0].payload.session.hostUuid, "a")
        context.destroy()
    }

    function test_quit_operation_is_one_shot_and_re_resolves_original_uuid() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }, { uuid: "b", name: "Beta" }])
        context.hostSource.setRunningApp("a", 77, "Runner")
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects = effects.concat([effect]) })
        context.facade.requestConnect("a")
        const confirmation = context.facade.baseOverlay.operationId
        compare(context.facade.baseOverlay.kind, "confirm-quit")
        context.facade.selectHostByUuid("b")
        context.facade.confirmQuitAndConnect(confirmation)
        compare(effects.length, 1)
        compare(effects[0].kind, "quit-then-launch")
        compare(effects[0].payload.hostUuid, "a")
        compare(effects[0].payload.runningAppLabel, "Runner")
        compare(effects[0].payload.session.hostUuid, "a")
        context.facade.quitRunningAppForOperation(effects[0].payload.operationId)
        compare(context.hostSource.lastQuitIndex, 0)
        compare(context.hostSource.quitDispatchCount, 1)
        context.facade.quitRunningAppForOperation(effects[0].payload.operationId)
        compare(context.hostSource.quitDispatchCount, 1)
        context.destroy()
    }

    function test_null_desktop_session_is_actionable_and_never_emits_effect() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.hostSource.setSessionAvailable(false)
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects = effects.concat([effect]) })
        context.facade.requestConnect("a")
        compare(context.hostSource.sessionDispatchCount, 1)
        compare(effects.length, 0)
        compare(context.facade.interruptOverlay.kind, "error")
        compare(context.facade.interruptOverlay.hostUuid, "a")
        context.destroy()
    }

    function test_wake_and_connect_waits_for_same_host_then_launches_once() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha", paired: true, online: false, serverSupported: true }])
        context.hostSource.setDesktopAppId(1)
        var launches = 0
        context.windowEffectRequested.connect(function(effect) { if (effect.kind === "launch") ++launches })
        context.facade.wakeHost("a", true)
        context.hostSource.completeWake(0, "")
        verify(context.facade.waitingForWake)
        compare(launches, 0)
        context.hostSource.updateHost("a", { online: true })
        context.checkWakeReadiness()
        compare(launches, 1)
        verify(!context.facade.waitingForWake)
        context.checkWakeReadiness()
        compare(launches, 1)
        context.destroy()
    }

    function test_wake_cancel_selection_change_and_timeout_do_not_launch() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", paired: true, online: false }, { uuid: "b" }])
        context.facade.wakeHost("a", true)
        context.facade.selectHostByUuid("b")
        verify(!context.facade.waitingForWake)
        context.hostSource.completeWake(0, "")
        compare(context.hostSource.sessionDispatchCount, 0)
        context.facade.wakeHost("a", true)
        context.wakeOperation.deadline = Date.now() - 1
        context.checkWakeReadiness()
        verify(!context.facade.waitingForWake)
        compare(context.facade.interruptOverlay.kind, "error")
        context.destroy()
    }

    function test_resume_does_not_quit_and_end_rechecks_running_target() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", online: true, paired: true, serverSupported: true }])
        context.hostSource.setRunningApp("a", 77, "Editor")
        context.facade.requestResume("a")
        compare(context.hostSource.sessionDispatchCount, 1)
        compare(context.hostSource.quitDispatchCount, 0)
        context.facade.requestEndSession("a")
        const operation = context.facade.baseOverlay.operationId
        context.hostSource.setRunningApp("a", 88, "Another app")
        context.facade.confirmEndSession(operation)
        compare(context.hostSource.quitDispatchCount, 0)
        context.destroy()
    }

    function test_wake_completion_uses_original_uuid_after_model_reorder() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }, { uuid: "b", name: "Beta" }])
        context.facade.wakeHost("a")
        compare(context.hostSource.wakeDispatchCount, 1)
        context.hostSource.replaceHosts([{ uuid: "b", name: "Beta" }, { uuid: "a", name: "Alpha" }])
        context.hostSource.completeWake(0, "wake failed")
        compare(context.facade.interruptOverlay.kind, "error")
        compare(context.facade.interruptOverlay.hostUuid, "a")
        context.destroy()
    }

    function test_queued_connection_test_defers_dispatch_until_promoted() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        context.facade.testConnection()
        compare(context.hostSource.connectionTestDispatchCount, 0)
        context.hostSource.completePairing()
        compare(context.facade.baseOverlay.kind, "connection-test")
        compare(context.hostSource.connectionTestDispatchCount, 1)
        context.destroy()
    }

    function test_add_host_request_queues_once_behind_an_active_base_overlay() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        context.facade.requestAddHostOverlay()
        context.facade.requestAddHostOverlay()

        compare(context.facade.baseOverlay.kind, "pairing")
        compare(context.queuedBaseOverlays.length, 1,
                "duplicate Ctrl+N delivery must coalesce behind the live base operation")
        compare(context.queuedBaseOverlays[0].kind, "add-host")

        context.facade.cancelPairing(context.facade.baseOverlay.payload.attemptId)
        compare(context.facade.baseOverlay.kind, "add-host",
                "the queued add-host operation must promote in FIFO order")
        compare(context.queuedBaseOverlays.length, 0)
        context.destroy()
    }

    function test_effect_acknowledgement_is_one_time_and_invalidated_by_route_change() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        var effects = []
        context.windowEffectRequested.connect(function(effect) { effects = effects.concat([effect]) })
        context.facade.requestConnect("a")
        compare(effects.length, 1)
        verify(context.acknowledgeWindowEffect(effects[0].effectId,
                                               effects[0].routeGeneration, "consumed"))
        verify(!context.acknowledgeWindowEffect(effects[0].effectId,
                                                effects[0].routeGeneration, "consumed"))
        context.facade.requestConnect("a")
        compare(effects.length, 2)
        context.routeActive = false
        verify(!context.acknowledgeWindowEffect(effects[1].effectId,
                                                effects[1].routeGeneration, "consumed"))
        context.destroy()
    }

    function test_quit_session_is_transferred_or_released_once() {
        const acknowledged = createTemporaryObject(hostContextComponent, testCase)
        acknowledged.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        acknowledged.hostSource.setRunningApp("a", 77, "Runner")
        var acknowledgedEffect
        acknowledged.windowEffectRequested.connect(function(effect) { acknowledgedEffect = effect })
        acknowledged.facade.requestConnect("a")
        acknowledged.facade.confirmQuitAndConnect(acknowledged.facade.baseOverlay.operationId)
        verify(acknowledged.acknowledgeWindowEffect(acknowledgedEffect.effectId,
                                                    acknowledgedEffect.routeGeneration, "consumed"))
        acknowledged.routeActive = false
        compare(acknowledged.hostSource.sessionReleaseCount, 0)
        acknowledged.destroy()

        const invalidated = createTemporaryObject(hostContextComponent, testCase)
        invalidated.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        invalidated.hostSource.setRunningApp("a", 77, "Runner")
        invalidated.facade.requestConnect("a")
        invalidated.facade.confirmQuitAndConnect(invalidated.facade.baseOverlay.operationId)
        invalidated.routeActive = false
        compare(invalidated.hostSource.sessionReleaseCount, 1)
        invalidated.destroy()
    }

    function test_deactivated_route_discards_async_state_and_ignores_late_signals() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.beginPairing()
        context.facade.testConnection()
        context.facade.wakeHost("a")
        context.routeActive = false
        compare(context.facade.baseOverlay.kind, "none")
        compare(context.facade.interruptOverlay.kind, "none")
        compare(context.hostSource.connectionTestDispatchCount, 0)
        context.hostSource.completePairing("late pairing failure")
        context.hostSource.completeConnectionTest(0, "47984")
        context.hostSource.completeWake(0, "late wake failure")
        context.hostSource.emitAsyncError("late async failure", "a")
        compare(context.facade.baseOverlay.kind, "none")
        compare(context.facade.interruptOverlay.kind, "none")
        compare(context.hostSource.connectionTestDispatchCount, 0)
        context.destroy()
    }

    function test_duplicate_add_submission_preserves_dispatch_marker_and_dispatches_once() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const manager = createTemporaryObject(computerManagerComponent, context)
        context.computerManager = manager
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("192.168.1.10")
        const operationId = context.facade.baseOverlay.operationId
        compare(manager.addDispatchCount, 1)
        verify(context.facade.baseOverlay.payload.dispatched)
        context.facade.submitAddHost("192.168.1.10")
        compare(context.facade.baseOverlay.operationId, operationId)
        compare(manager.addDispatchCount, 1)
        verify(context.facade.baseOverlay.payload.dispatched)
        context.destroy()
    }

    function test_failed_add_keeps_address_and_allows_retry() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const manager = createTemporaryObject(computerManagerComponent, context)
        context.computerManager = manager
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("192.0.2.10")
        compare(manager.addDispatchCount, 1)
        context.completeAddHost(context.facade.baseOverlay.payload.attemptId, false, false)
        compare(context.facade.baseOverlay.kind, "add-host")
        compare(context.facade.baseOverlay.payload.address, "192.0.2.10")
        verify(context.facade.baseOverlay.payload.error)
        compare(context.facade.interruptOverlay.kind, "none")
        context.facade.submitAddHost("192.0.2.11")
        compare(manager.addDispatchCount, 2)
        context.destroy()
    }

    function test_invalid_add_address_stays_in_dialog_without_dispatch() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const manager = createTemporaryObject(computerManagerComponent, context)
        context.computerManager = manager
        context.facade.requestAddHostOverlay()
        context.facade.submitAddHost("not valid host")
        compare(manager.addDispatchCount, 0)
        compare(context.facade.baseOverlay.kind, "add-host")
        verify(context.facade.baseOverlay.payload.error)
        context.destroy()
    }

    function test_facade_values_and_emitted_effects_are_detached_from_internal_state() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        const snapshot = context.facade.selectedHostSnapshot
        snapshot.name = "Corrupted"
        compare(context.facade.selectedHostSnapshot.name, "Alpha")
        const options = context.facade.resolutionOptions
        options[0].text = "Corrupted"
        verify(context.facade.resolutionOptions[0].text !== "Corrupted")
        context.facade.requestDetails("a")
        const overlay = context.facade.baseOverlay
        overlay.kind = "Corrupted"
        compare(context.facade.baseOverlay.kind, "details")
        const preferenceSnapshot = context.facade.hostPreferenceSnapshot
        preferenceSnapshot.width = 1
        compare(context.facade.hostPreferenceSnapshot.width, context.preferences.width)
        var emitted
        context.windowEffectRequested.connect(function(effect) { emitted = effect })
        context.facade.requestConnect("a")
        emitted.payload.hostUuid = "Corrupted"
        compare(context.pendingEffects[emitted.effectId].payload.hostUuid, "a")
        context.destroy()
    }

    function test_stale_custom_operation_and_non_boolean_preference_input_are_rejected() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.preferences.reset()
        context.facade.requestCustomResolution("a")
        const operationId = context.facade.baseOverlay.operationId
        context.hostSource.replaceHosts([{ uuid: "a", name: "Alpha" }])
        context.facade.submitCustomResolution(operationId, 2560, 1440)
        compare(context.preferences.width, 1920)
        compare(context.preferences.height, 1080)
        context.facade.setBandwidthLimit(1)
        compare(context.preferences.autoAdjustBitrate, true)
        compare(context.preferences.saveCalls, 0)
        context.destroy()
    }
    function test_selected_snapshot_describes_desktop_resume_or_app_switch() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        context.hostSource.setDesktopAppId(42)
        context.hostSource.setRunningApp("a", 77, "Editor")
        context.hostSource.replaceHosts([{uuid:"a", name:"Alpha", online:true, paired:true, busy:true}])
        compare(context.facade.selectedHostSnapshot.desktopAppId, 42)
        compare(context.facade.selectedHostSnapshot.runningAppId, 77)
        compare(context.facade.selectedHostSnapshot.runningAppName, "Editor")
        context.facade.requestConnect("a")
        compare(context.facade.baseOverlay.kind, "confirm-quit")
        compare(context.facade.baseOverlay.payload.runningAppLabel, "Editor")
        context.facade.rejectQuit(context.facade.baseOverlay.operationId)
        context.hostSource.setRunningApp("a", 42, "Desktop")
        context.hostSource.replaceHosts([{uuid:"a", name:"Alpha", online:true, paired:true, busy:true}])
        compare(context.facade.selectedHostSnapshot.runningAppId, 42)
    }

    function test_empty_discovery_guidance_waits_then_resets_for_a_new_search() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const timer = findChild(context, "initial-discovery-timer")
        verify(timer !== null)
        timer.interval = 1
        tryCompare(context.facade, "initialDiscoveryElapsed", true)
        context.preferences.enableMdns = false
        compare(context.facade.initialDiscoveryElapsed, false)
        compare(timer.running, false)
        context.preferences.enableMdns = true
        tryCompare(context.facade, "initialDiscoveryElapsed", true)
        context.hostSource.replaceHosts([{uuid:"found",name:"Found"}])
        compare(context.facade.initialDiscoveryElapsed, false)
        compare(timer.running, false)
        context.hostSource.replaceHosts([])
        context.routeActive = false
        compare(timer.running, false)
        context.routeActive = true
        tryCompare(context.facade, "initialDiscoveryElapsed", true)
    }


    function test_dialog_targets_survive_reorder_but_not_delete_and_readd() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        const source = context.hostSource
        source.replaceHosts([{uuid: "a", name: "Alpha"}, {uuid: "b", name: "Beta"}])
        context.facade.requestCustomFrameRate("a")
        const frameOperation = context.facade.baseOverlay.operationId
        source.reorderHosts(["b", "a"])
        context.facade.submitCustomFrameRate(frameOperation, 50)
        compare(context.facade.baseOverlay.kind, "none")
        compare(context.preferences.fps, 50)
        context.facade.requestDelete("a")
        const deleteOperation = context.facade.baseOverlay.operationId
        source.deleteComputer(1)
        source.appendHost({uuid: "a", name: "New Alpha"})
        context.facade.confirmDelete(deleteOperation)
        compare(source.rowCount(), 2)
        compare(source.nameForIndex(1), "New Alpha")
        compare(context.facade.baseOverlay.kind, "none")
        compare(context.facade.interruptOverlay.kind, "error")
    }
}
