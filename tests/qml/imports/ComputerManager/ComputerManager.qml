pragma Singleton
import QtQml 2.15

// Boundary adapter: records calls from the real launcher window without
// attempting discovery, polling, or networking.
QtObject {
    property var cancelledPairingAttempts: []
    property int startPollingCalls: 0
    property int stopPollingCalls: 0
    property var pollingStateSource: null
    property var startPollingStatesAtDispatch: []
    property var stopPollingStatesAtDispatch: []

    property int nextManualAttempt: 1
    function reserveManualAddAttemptId() { return String(nextManualAttempt++) }
    function cancelManualAdd(attemptId) {}
    signal manualHostAddCompleted(string attemptId, bool success, bool blocked)
    signal computerAddCompleted(bool success, bool detectedPortBlocking)
    signal quitAppCompleted(var error)

    function cancelPairing(attemptId) {
        cancelledPairingAttempts = cancelledPairingAttempts.concat([attemptId])
    }

    function reset() {
        cancelledPairingAttempts = []
        startPollingCalls = 0
        stopPollingCalls = 0
        pollingStateSource = null
        startPollingStatesAtDispatch = []
        stopPollingStatesAtDispatch = []
    }

    function startPolling() {
        startPollingStatesAtDispatch.push(
                    pollingStateSource ? pollingStateSource.pollingActive : undefined)
        startPollingCalls += 1
    }

    function stopPollingAsync() {
        stopPollingStatesAtDispatch.push(
                    pollingStateSource ? pollingStateSource.pollingActive : undefined)
        stopPollingCalls += 1
    }

    function addNewHostManually(address) {
    }

    function quitRunningApp() {
    }
}
