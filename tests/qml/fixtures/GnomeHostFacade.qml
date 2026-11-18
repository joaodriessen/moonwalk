import QtQml 2.15
import QtQuick 2.15

QtObject {
    property var hosts: ListModel {
        ListElement {
            uuid: "host-a"
            name: "Apollo"
            online: true
            paired: true
            busy: false
            wakeable: true
            statusUnknown: false
            serverSupported: true
            details: "A trusted host"
            address: "192.0.2.10"
            hostOs: "Linux"
            serverSoftware: "Sunshine"
        }
    }
    property string selectedHostUuid: "host-a"
    property int selectedHostIndex: 0
    property int displayRefreshRate: 60
    property var selectedHostSnapshot: ({ uuid: "host-a", name: "Apollo", online: true, paired: true, busy: false, wakeable: true, statusUnknown: false, serverSupported: true, details: "A trusted host", address: "192.0.2.10", hostOs: "Linux", serverSoftware: "Sunshine" })
    property string selectedHostKind: "stock"
    property var resolutionOptions: [{ text: "1920×1080", w: 1920, h: 1080 }, { text: "Custom…", w: 0, h: 0 }]
    property var frameRateOptions: [{ text: "60 fps", value: 60 }, { text: "Custom…", value: -1 }]
    property var hostPreferenceSnapshot: ({ width: 1920, height: 1080, fps: 60, bitrateKbps: 20000, autoAdjustBitrate: true, localCursor: false, videoCodecConfig: 0, absoluteMouseMode: false, desktopScale: "auto", viewScale: "fit", adaptiveBitrate: false, manualStream: false, manualDisplay: false, manualInput: false, localTextInput: false })
    // The stream plan double: the exact StreamPlan shape from app/streaming/streamplan.h.
    property var streamPlan: ({ width: 1920, height: 1080, fps: 60, codec: "", startBitrateKbps: 20000, ceilingKbps: 24000, floorKbps: 4000,
                                limitedByHost: false, limitReason: "",
                                resolutionWarning: "", fpsWarning: "", codecWarning: "", bitrateWarning: "" })
    property var baseOverlay: ({ kind: "none", operationId: -1, hostUuid: "", modelGeneration: 0, payload: ({}) })
    property var interruptOverlay: ({ kind: "none", operationId: -1, hostUuid: "", modelGeneration: 0, payload: ({}) })
    property bool busy: false
    property bool routeActive: true
    property bool initialDiscoveryElapsed: false
    property int commandCount: 0
    property string lastCommand: ""
    property var lastArguments: []
    property bool connectionAddressAccepted: true
    property var addressSubmissions: []
    function submitConnectionAddress(operationId, value) {
        if (!connectionAddressAccepted || !routeActive
                || interruptOverlay.kind !== "none"
                || baseOverlay.kind !== "details"
                || baseOverlay.operationId !== operationId
                || baseOverlay.hostUuid !== selectedHostUuid) return false
        value = value.trim()
        addressSubmissions = addressSubmissions.concat([{ operationId: operationId, hostUuid: baseOverlay.hostUuid, value: value }])
        var payload = Object.assign({}, baseOverlay.payload, { connectionAddress: value })
        baseOverlay = Object.assign({}, baseOverlay, { payload: payload })
        record("submitConnectionAddress", [operationId, value])
        return true
    }

    function record(command, arguments) { lastCommand = command; lastArguments = arguments; ++commandCount }
    function overlay(kind, uuid, payload) { baseOverlay = ({ kind: kind, operationId: 41, hostUuid: uuid || "host-a", modelGeneration: 0, payload: payload || ({ attemptId: 41, pin: "1234", message: "Failure", runningAppLabel: "Game", blockedPorts: "47984" }) }) }
    property var presets: []
    function requestPresets(hostUuid) { record("requestPresets", [hostUuid]); overlay("presets", hostUuid, ({})) }
    function resetLearnedPerformance() { record("resetLearnedPerformance", []) }
    function restoreSuccessfulSettings() { record("restoreSuccessfulSettings", []) }
    function savePreset(name) { record("savePreset", [name]); return true }
    function applyPreset(id) { record("applyPreset", [id]); return true }
    function renamePreset(id, name) { record("renamePreset", [id, name]); return true }
    function duplicatePreset(id, name) { record("duplicatePreset", [id, name]); return true }
    function removePreset(id) { record("removePreset", [id]); return true }
    function selectHostByUuid(value) { selectedHostUuid = value; ++commandCount }
    function selectHostAtIndex(index) {
        if (index < 0 || index >= hosts.count) return false
        selectedHostIndex = index
        selectHostByUuid(hosts.get(index).uuid)
        return true
    }
    function requestDetails(value) { overlay("details", value); ++commandCount }
    function requestOpenSettings() { ++commandCount }
    function requestAddHostOverlay() { overlay("add-host", ""); ++commandCount }
    function beginPairing() { overlay("pairing", "host-a"); ++commandCount }
    function cancelPairing(value) { dismissOverlay(value) }
    function submitAddHost(value) { dismissOverlay(baseOverlay.operationId); record("submitAddHost", [value]) }
    function testConnection() { overlay("connection-test", ""); ++commandCount }
    function wakeHost(value) { ++commandCount }
    function requestConnect(value) { overlay("confirm-quit", value); record("requestConnect", [value]) }
    function confirmQuitAndConnect(value) { dismissOverlay(value); record("confirmQuitAndConnect", [value]) }
    function quitRunningAppForOperation(value) { ++commandCount }
    function rejectQuit(value) { dismissOverlay(value); record("rejectQuit", [value]) }
    function dismissOverlay(value) {
        var none = ({ kind: "none", operationId: -1, hostUuid: "", modelGeneration: 0, payload: ({}) })
        if (interruptOverlay.kind !== "none" && interruptOverlay.operationId === value) {
            interruptOverlay = none
            record("dismissOverlay", [value])
        }
        else if (baseOverlay.kind !== "none" && baseOverlay.operationId === value) {
            baseOverlay = none
            record("dismissOverlay", [value])
        }
    }
    function requestRename(value) { overlay("rename-host", value); ++commandCount }
    function submitRename(operationId, name) { dismissOverlay(operationId); record("submitRename", [operationId, name]) }
    function requestDelete(value) { overlay("confirm-delete", value); ++commandCount }
    function confirmDelete(value) { dismissOverlay(value); record("confirmDelete", [value]) }
    function requestCustomResolution(value) { overlay("custom-resolution", value); ++commandCount }
    function submitCustomResolution(operationId, width, height) { dismissOverlay(operationId); record("submitCustomResolution", [operationId, width, height]) }
    function requestCustomFrameRate(value) { overlay("custom-frame-rate", value); ++commandCount }
    function submitCustomFrameRate(operationId, fps) { dismissOverlay(operationId); record("submitCustomFrameRate", [operationId, fps]) }
    function setResolution(width, height) { record("setResolution", [width, height]) }
    function setFrameRate(value) { record("setFrameRate", [value]) }
    function setAutomaticFrameRate() { record("setAutomaticFrameRate", []) }
    function setBitrateKbps(value) { record("setBitrateKbps", [value]) }
    function setQualityMode(value) { record("setQualityMode", [value]) }
    function setViewScale(value) { record("setViewScale", [value]) }
    function setDesktopScale(value) { record("setDesktopScale", [value]) }
    function setLocalTextInput(value) { record("setLocalTextInput", [value]) }
    function requestResume(value) { record("requestResume", [value]) }
    function requestEndSession(value) { record("requestEndSession", [value]) }
    function setAutoAdjustBitrate(value) { record("setAutoAdjustBitrate", [value]) }
    function setStreamMode(value) { record("setStreamMode", [value]) }
    function setDisplayMode(value) { record("setDisplayMode", [value]) }
    function setInputMode(value) { record("setInputMode", [value]) }
    function setBandwidthLimit(value) { record("setBandwidthLimit", [value]) }
    function setAutomaticResolution() { record("setAutomaticResolution", []) }
    function setLocalCursorEnabled(value) { record("setLocalCursorEnabled", [value]) }
    function setVideoCodec(value) { record("setVideoCodec", [value]) }
    function setAbsoluteMouseMode(value) { record("setAbsoluteMouseMode", [value]) }
    function setAutomaticPointer(value) { record("setAutomaticPointer", [value]) }
}
