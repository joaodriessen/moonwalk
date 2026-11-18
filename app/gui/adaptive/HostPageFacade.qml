import QtQml 2.15

QtObject {
    readonly property var hosts: hostList
    readonly property string selectedHostUuid: root.selectedHostUuid
    readonly property int selectedHostIndex: root.selectedHostIndex
    readonly property var selectedHostSnapshot: root.frozenDetachedValue(root.selectedHostSnapshot)
    readonly property string selectedHostKind: root.selectedHostKind
    readonly property var resolutionOptions: root.frozenDetachedValue(root.resolutionOptions)
    readonly property var frameRateOptions: root.frozenDetachedValue(root.frameRateOptions)
    readonly property int displayRefreshRate: root.displayRefreshRate
    readonly property var hostPreferenceSnapshot: root.frozenDetachedValue({
        width: root.preferences ? root.preferences.width : 0,
        automaticResolution: root.preferences ? root.preferences.automaticResolution === true : false,
        automaticFrameRate: root.preferences ? root.preferences.automaticFrameRate === true : false,
        automaticPointer: root.preferences ? root.preferences.automaticPointer === true : false,
        displayTarget: root.preferences ? root.preferences.displayTarget : "host",
        height: root.preferences ? root.preferences.height : 0,
        fps: root.preferences ? root.preferences.fps : 0,
        bitrateKbps: root.preferences ? root.preferences.bitrateKbps : 0,
        autoAdjustBitrate: root.preferences ? root.preferences.autoAdjustBitrate : true,
        localCursor: root.preferences ? root.preferences.localCursor : false,
        videoCodecConfig: root.preferences ? root.preferences.videoCodecConfig : 0,
        absoluteMouseMode: root.preferences ? root.preferences.absoluteMouseMode : false,
        connectionAddress: root.preferences ? root.preferences.connectionAddress : "",
        viewScale: root.preferences ? root.preferences.viewScale : "fit",
        desktopScale: root.preferences ? root.preferences.desktopScale : "auto",
        adaptiveBitrate: root.preferences ? root.preferences.adaptiveBitrate : false,
        manualStream: root.preferences ? root.preferences.manualStream === true : false,
        manualDisplay: root.preferences ? root.preferences.manualDisplay === true : false,
        manualInput: root.preferences ? root.preferences.manualInput === true : false,
        localTextInput: root.preferences ? root.preferences.localTextInput : false,
        activePresetName: root.preferences ? root.preferences.activePresetName : "",
        lastConnectedAt: root.preferences && root.preferences.lastConnectedAt ? root.preferences.lastConnectedAt : 0,
        enableMdns: root.preferences ? root.preferences.enableMdns : true,
        starlightVersionString: root.systemProperties
                                ? root.systemProperties.starlightVersionString : ""
    })
    // What the next stream will be and what each Manual choice costs.
    readonly property var streamPlan: root.frozenDetachedValue(root.streamPlan)
    readonly property var baseOverlay: root.frozenDetachedValue(root.baseOverlay)
    readonly property var interruptOverlay: root.frozenDetachedValue(root.interruptOverlay)
    readonly property bool busy: baseOverlay.kind !== "none"
    readonly property bool routeActive: root.routeActive
    readonly property bool initialDiscoveryElapsed: root.initialDiscoveryElapsed
    readonly property bool waitingForWake: root.waitingForWake
    readonly property var presets: root.frozenDetachedValue(root.presets)

    function selectHostByUuid(hostUuid) { root.selectHostByUuid(hostUuid) }
    function selectHostAtIndex(index) { return root.selectHostAtIndex(index) }
    function requestDetails(hostUuid) { root.requestDetails(hostUuid) }
    function submitConnectionAddress(operationId, value) { return root.submitConnectionAddress(operationId, value) }
    function requestOpenSettings() { root.requestOpenSettings() }
    function requestAddHostOverlay() { return root.requestAddHostOverlay() }
    function beginPairing() { root.beginPairing() }
    function cancelPairing(attemptId) { root.cancelPairing(attemptId) }
    function submitAddHost(address) { root.submitAddHost(address) }
    function testConnection() { root.testConnection() }
    function wakeHost(hostUuid, connectWhenReady) { root.wakeHost(hostUuid, connectWhenReady) }
    function cancelWakeConnect() { root.cancelWakeConnect() }
    function requestConnect(hostUuid, safeSettings) { root.requestConnect(hostUuid, safeSettings) }
    function requestResume(hostUuid) { root.requestResume(hostUuid) }
    function requestEndSession(hostUuid) { root.requestEndSession(hostUuid) }
    function confirmEndSession(operationId) { root.confirmEndSession(operationId) }
    function setWakeTimeout(seconds) { if (seconds >= 10 && seconds <= 300) root.wakeTimeoutSeconds = seconds }
    function savePreset(name) { return root.savePreset(name) }
    function applyPreset(id) { return root.applyPreset(id) }
    function renamePreset(id, name) { return root.renamePreset(id, name) }
    function duplicatePreset(id, name) { return root.duplicatePreset(id, name) }
    function removePreset(id) { return root.removePreset(id) }
    function resetLearnedPerformance() { return root.resetLearnedPerformance() }
    function restoreSuccessfulSettings() { root.restoreSuccessfulSettings() }
    function requestPresets(hostUuid) { root.requestPresets(hostUuid) }
    function setConnectionAddress(value) { return root.setConnectionAddress(value) }
    function setViewScale(value) { root.setViewScale(value) }
    function setDesktopScale(value) { root.setDesktopScale(value) }
    function setAdaptiveBitrate(value) { root.setAdaptiveBitrate(value) }
    function setQualityMode(mode) { root.setQualityMode(mode) }
    function setLocalTextInput(value) { root.setLocalTextInput(value) }
    function confirmQuitAndConnect(operationId) { root.confirmQuitAndConnect(operationId) }
    function quitRunningAppForOperation(operationId) { root.quitRunningAppForOperation(operationId) }
    function rejectQuit(operationId) { root.rejectQuit(operationId) }
    function dismissOverlay(operationId) { root.dismissOverlay(operationId) }
    function requestRename(hostUuid) { root.requestRename(hostUuid) }
    function submitRename(operationId, name) { root.submitRename(operationId, name) }
    function requestDelete(hostUuid) { root.requestDelete(hostUuid) }
    function confirmDelete(operationId) { root.confirmDelete(operationId) }
    function requestCustomResolution(hostUuid) { root.requestCustomResolution(hostUuid) }
    function submitCustomResolution(operationId, width, height) { root.submitCustomResolution(operationId, width, height) }
    function requestCustomFrameRate(hostUuid) { root.requestCustomFrameRate(hostUuid) }
    function submitCustomFrameRate(operationId, fps) { root.submitCustomFrameRate(operationId, fps) }
    function setResolution(width, height) { root.setResolution(width, height) }
    function setAutomaticResolution() { root.setAutomaticResolution() }
    function setDisplayTarget(value) { root.setDisplayTarget(value) }
    function setAutomaticPointer(value) { root.setAutomaticPointer(value) }
    function setFrameRate(fps) { root.setFrameRate(fps) }
    function setAutomaticFrameRate() { root.setAutomaticFrameRate() }
    function setBitrateKbps(value) { root.setBitrateKbps(value) }
    function setAutoAdjustBitrate(value) { root.setAutoAdjustBitrate(value) }
    function setLocalCursorEnabled(value) { root.setLocalCursorEnabled(value) }
    function setVideoCodec(value) { root.setVideoCodec(value) }
    function setStreamMode(mode) { root.setStreamMode(mode) }
    function setDisplayMode(mode) { root.setDisplayMode(mode) }
    function setInputMode(mode) { root.setInputMode(mode) }
    function setBandwidthLimit(enabled) { root.setBandwidthLimit(enabled) }
    function setAbsoluteMouseMode(value) { root.setAbsoluteMouseMode(value) }
}
