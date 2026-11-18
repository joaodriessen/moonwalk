import QtQml 2.15

// Renderer-neutral load adapter for the segue roots. The style-bootstrap
// contract never starts a real stream; it only needs the production QML type
// and signal surface to be present while the roots are instantiated.
QtObject {
    property var launchWarnings: []
    property int cancelPendingStartCalls: 0
    property bool initializeResult: true
    property int connectivityTestResult: -1
    property int cancelConnectivityTestCalls: 0
    property bool cleanupComplete: true
    property bool connectionWasEstablished: false
    property bool retryableTermination: false
    property bool adjustingForHost: false
    property bool safeSettingsTrial: false
    property bool hasVideoFrame: false
    property int retryCreateCalls: 0
    property int retainedCount: 0
    property int releasedCount: 0
    property int stableMilliseconds: 0
    property bool controlsOpen: false
    property var fileTransfer: null
    property var controlState: ({ host: "Test host", app: "Desktop", format: "" })
    function openControls() { controlsOpen = true }
    function closeControls() { controlsOpen = false }
    function controlAction(action, value) { }
    property var nextRetrySession: null
    property bool lastResumeOnly: false
    property bool lastSafeSettings: false
    property bool lastManualRetry: false
    property bool lastSuccessfulAvailable: false
    property bool lastSuccessfulRequested: false
    function stableVideoMilliseconds() { return stableMilliseconds }
    function retainForRetry() { ++retainedCount }
    function releaseRetryRetention() { ++releasedCount }
    function createRetrySession(safeSettings, resumeOnly, manualRetry, lastSuccessful) { ++retryCreateCalls; lastSafeSettings = safeSettings; lastResumeOnly = resumeOnly; lastManualRetry = manualRetry; lastSuccessfulRequested = lastSuccessful === true; return nextRetrySession }
    function hasLastSuccessfulSettings() { return lastSuccessfulAvailable }
    function keepSafeSettings() { return safeSettingsTrial && hasVideoFrame }

    signal stageStarting(string stage)
    signal stageFailed(string stage, int errorCode, string failingPorts)
    signal connectionStarted()
    signal displayLaunchError(string text)
    signal hostCleanupFailed(string message)
    signal quitStarting()
    signal sessionFinished(int portTestResult)
    signal readyForDeletion()
    signal connectionDiagnosticReady(int result)

    function initialize(window) {
        return initializeResult
    }

    function start() {
    }

    function cancelPendingStart() {
        ++cancelPendingStartCalls
    }

    function cancelConnectivityTest() {
        ++cancelConnectivityTestCalls
    }
}
