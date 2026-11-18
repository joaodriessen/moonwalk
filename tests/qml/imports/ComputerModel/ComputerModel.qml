import QtQuick 2.15

// Boundary adapter for loading the real PcView in qmltestrunner. It records
// backend calls but deliberately contains no pairing state machine.
ListModel {
    id: root

    property int pairComputerCalls: 0
    property int nextPairingRequestId: 1
    property var pairingAttempts: []
    property int initializeCalls: 0
    property int lastPairComputerIndex: -1
    property string lastPairComputerPin: ""
    property var pairingStateSource: null
    property var pendingPairingAttemptsAtDispatch: []

    signal pairingCompleted(int attemptId, var error)
    property int nextConnectionAttempt: 1
    function reserveConnectionTestAttemptId() { return nextConnectionAttempt++ }
    function cancelConnectionTest(attemptId) {}
    signal connectionDiagnosticCompleted(int attemptId, int result, string blockedPorts)
    signal connectionTestCompleted(var result)
    signal wakeCompleted(var result)

    function initialize(manager) {
        initializeCalls += 1
    }

    function generatePinString() {
        return "1234"
    }

    function reservePairingAttemptId() { return nextPairingRequestId++ }

    function pairComputer(index, pin, attemptId) {
        pendingPairingAttemptsAtDispatch =
                pendingPairingAttemptsAtDispatch.concat([
                    pairingStateSource
                        ? JSON.stringify(pairingStateSource.pendingPairingAttempts)
                        : "missing"
                ])
        pairComputerCalls += 1
        lastPairComputerIndex = index
        lastPairComputerPin = pin
        pairingAttempts = pairingAttempts.concat([attemptId])
    }

    function nameForIndex(index) {
        return index >= 0 && index < count ? get(index).name : ""
    }

    function uuidForIndex(index) {
        return index >= 0 && index < count ? get(index).uuid : ""
    }

    function serverSoftwareForIndex(index) {
        return index >= 0 && index < count ? get(index).serverSoftware : ""
    }

    function getDesktopAppId(index) {
        return 0
    }

    function getRunningAppId(index) {
        return 0
    }

    function getRunningAppName(index) {
        return ""
    }

    function createSessionForDesktop(index, safeSettings, desktopTakeover) {
        return null
    }

    function quitRunningApp(index) {
    }

    function wakeComputer(index) {
        return ""
    }

    function deleteComputer(index) {
    }

    function renameComputer(index, name) {
    }

    function testConnectionForComputer(index) {
    }
}
