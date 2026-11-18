import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15

import SdlGamepadKeyNavigation 1.0
import Session 1.0
import SystemProperties 1.0

import Starlight 1.0
import "starlight"

Item {
    id: root
    SessionControls { session: root.session; hostSeed: root.hostSeed; hostIcon: root.hostIcon }
    property Session session
    property string appName
    property string stageText : isResume ? qsTr("Resuming %1…").arg(appName) :
                                           qsTr("Starting %1…").arg(appName)
    // The host being reached, when the launcher knows it.
    property string hostSeed: ""
    property string hostIcon: ""
    readonly property string hostName: session && session.controlState ? (session.controlState.host || "") : ""
    // Every connection walks the same stages, so they can be counted.
    property int stagesDone: 0
    readonly property int stageCount: 11
    readonly property real stageProgress: stagesDone > 0 ? Math.min(1, stagesDone / stageCount) : -1
    property bool isResume : false
    property bool spinnerShown: false
    property bool stageShown: true
    property string streamHint: ""
    property bool quitAfter : false
    // GUI segue lifetime is owned by main.qml. CLI callers leave this unset
    // and retain the historical local fallback below.
    property var quitSegueRequestedFn
    property var retainLaunchWarningsFn
    property bool activated: false
    property bool abandoned: false
    property bool terminalDelivered: false
    property bool retryPending: false
    property bool retrying: false
    property bool failed: false
    property bool trialOutcome: false
    property bool trialSaved: false
    property bool safeRetryRelevant: false
    property bool everEstablished: false
    property int retryAttempts: 0
    property int retryGeneration: 0
    property int scheduledGeneration: -1
    property double retryDeadline: 0
    property string lastFailure: ""
    property bool diagnosticAdded: false
    property bool decoderCheckFailed: false
    property string technicalFailure: ""
    property bool showTechnicalFailure: false
    readonly property var retryDelays: [1000, 2000, 4000, 8000, 10000]

    function retainSession() {
        if (!quitAfter && session && session.retainForRetry) session.retainForRetry()
    }
    function stopRetries() {
        retryGeneration += 1
        retryPending = false
        retryTimer.stop()
        cleanupWait.stop()
        retryBudget.stop()
    }
    function showTerminalFailure() {
        stopRetries()
        if ((everEstablished || retrying) && session && session.cancelPendingStart) session.cancelPendingStart()
        terminalDelivered = true
        failed = true
        retrying = false
        spinnerShown = false
        stageShown = true
        stageText = trialOutcome ? qsTr("Safe settings test finished")
                                 : everEstablished || !hostName ? qsTr("Stream ended")
                                 : qsTr("Could Not Connect to %1").arg(hostName)
        window.visible = true
    }
    function scheduleAfterCleanup() {
        if (!retryPending || abandoned || !activated) return
        if (Date.now() >= retryDeadline || retryAttempts >= retryDelays.length) {
            showTerminalFailure()
            return
        }
        if (!session || session.cleanupComplete !== true) {
            cleanupWait.start()
            return
        }
        if (!retryTimer.running) {
            scheduledGeneration = retryGeneration
            retryTimer.interval = retryDelays[retryAttempts]
            retryTimer.start()
        }
    }
    function retryWithSettings(safeSettings, automatic, lastSuccessful) {
        if (abandoned || !activated || !session || session.cleanupComplete !== true) return false
        if (automatic && (!retryPending || scheduledGeneration !== retryGeneration || Date.now() >= retryDeadline)) return false
        var previous = session
        var next = previous.createRetrySession(safeSettings === true, automatic || everEstablished, !automatic, lastSuccessful === true)
        if (!next) {
            lastFailure = qsTr("The remote session changed or ended. Return to the host page and connect again.")
            showTerminalFailure()
            return false
        }
        if (automatic) retryAttempts += 1
        else stopRetries()
        retryPending = false
        session = next
        retainSession()
        if (previous.releaseRetryRetention) previous.releaseRetryRetention()
        failed = false
        lastFailure = ""
        trialOutcome = false
        trialSaved = false
        safeRetryRelevant = false
        diagnosticAdded = false
        technicalFailure = ""
        showTechnicalFailure = false
        terminalDelivered = false
        streamSegueErrorDialog.text = ""
        streamLoader.active = false
        stagesDone = 0
        spinnerShown = true
        stageShown = true
        stageText = automatic ? qsTr("Reconnecting… attempt %1 of 5").arg(retryAttempts) : qsTr("Trying %1…").arg(appName)
        startStreamWhenReady()
        return true
    }

    function recordLaunchWarnings(warnings) {
        if (abandoned || terminalDelivered)
            return
        if (retainLaunchWarningsFn)
            retainLaunchWarningsFn(warnings)
    }

    function retainCurrentLaunchWarnings() {
        if (session)
            recordLaunchWarnings(session.launchWarnings)
    }

    property bool videoCheckRequested: false
    function startStreamWhenReady() {
        if (!activated || abandoned || terminalDelivered || streamLoader.active) return
        if (SystemProperties.decoderProbeError) {
            decoderCheckFailed = true
            safeRetryRelevant = false
            lastFailure = SystemProperties.decoderProbeError
            showTerminalFailure()
            stageText = qsTr("Video check failed")
            return
        }
        if (!SystemProperties.decoderProbeReady) {
            stageText = qsTr("Checking video on this device…")
            // A command-line launch skips the launcher, which normally starts this.
            if (!SystemProperties.decoderProbeRunning && !videoCheckRequested) {
                videoCheckRequested = true
                SystemProperties.startAsyncLoad()
            }
            return
        }
        decoderCheckFailed = false
        streamLoader.active = true
    }

    function retryVideoCheck() {
        if (abandoned || !activated || SystemProperties.decoderProbeRunning) return
        failed = false
        terminalDelivered = false
        decoderCheckFailed = false
        lastFailure = ""
        spinnerShown = true
        SystemProperties.startAsyncLoad()
        startStreamWhenReady()
    }

    Connections {
        target: SystemProperties
        function onDecoderProbeReadyChanged() { startStreamWhenReady() }
        function onDecoderProbeStateChanged() { startStreamWhenReady() }
    }

    // The protocol's stage names, in words that mean something to the person waiting.
    function describeStage(stage) {
        if (stage === "platform initialization") return qsTr("Preparing this device…")
        if (stage === "name resolution") return qsTr("Finding the host…")
        if (stage.indexOf("audio") >= 0) return qsTr("Starting audio…")
        if (stage.indexOf("video") >= 0) return qsTr("Starting video…")
        if (stage.indexOf("input") >= 0) return qsTr("Connecting keyboard and mouse…")
        return qsTr("Setting up the stream…")
    }
    function stageStarting(stage)
    {
        stagesDone += 1
        stageText = describeStage(stage)
    }

    function stageFailed(stage, errorCode, failingPorts)
    {
        if (abandoned || terminalDelivered) return
        safeRetryRelevant = stage.indexOf("video") >= 0 || stage === "platform initialization"
        // Display the error dialog after Session::exec() returns
        var message = formatStageFailure(stage, errorCode, failingPorts)
        if (quitAfter) streamSegueErrorDialog.text = message
        else lastFailure = message
    }

    function formatStageFailure(stage, errorCode, failingPorts)
    {
        var advice
        if (stage === "name resolution") {
            advice = qsTr("Check the host address and make sure both computers can reach each other.")
        } else if (stage === "video stream establishment") {
            advice = qsTr("Video couldn't start. Check that the host is capturing a display, then try Safe Settings.")
        } else if (stage === "audio stream initialization" || stage === "audio stream establishment") {
            advice = qsTr("The audio stream could not start. Check the audio device and try again.")
        } else if (stage === "platform initialization") {
            advice = qsTr("Moonwalk could not prepare this device for streaming. Check the app log for details and try again.")
        } else {
            advice = qsTr("Check that the host is awake and its streaming service is running, then try again.")
        }

        var message = advice
        if (failingPorts)
            message += "\n\n" + qsTr("Check the host firewall for port(s): %1").arg(failingPorts)
        technicalFailure = qsTr("%1 (error %2)").arg(stage).arg(errorCode)
        showTechnicalFailure = false
        return message
    }

    function connectionStarted()
    {
        if (abandoned || terminalDelivered)
            return
        // Carry the attempt/deadline across short, repeatedly failing streams.
        // A successful window alone must not create an endless reconnect loop.
        retryBudget.stop()
        retryPending = false
        retrying = false
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        spinnerShown = false
        stageShown = false
        streamHint = ""

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        if (abandoned || terminalDelivered) return
        if (/codec|decoder|video|resolution|HDR|4:4:4/i.test(text)) safeRetryRelevant = true
        // Display the error dialog after Session::exec() returns
        if (quitAfter) streamSegueErrorDialog.text = text
        else lastFailure = text
        console.error(text)
    }
    function preserveCleanupFeedback(previous) {
        // Connect to the surviving window method before this route drops its
        // Session. A late failure must remain visible after Back or cancellation.
        if (previous && previous.hostCleanupFailed && window.reportStreamCleanupFailure)
            previous.hostCleanupFailed.connect(window.reportStreamCleanupFailure)
    }
    Component.onDestruction: {
        stopRetries()
        var previous = session
        preserveCleanupFeedback(previous)
        session = null
        if (previous && previous.cancelPendingStart) previous.cancelPendingStart()
        if (previous && previous.releaseRetryRetention) previous.releaseRetryRetention()
    }

    function quitStarting()
    {
        // Local video/input are already released. Keep the remote end in the
        // Session worker; the launcher owns any later failure notification.
        if (abandoned || terminalDelivered || !session || session.connectionWasEstablished !== true)
            return
        if (quitAfter) return // CLI exits only after its background cleanup completes.
        // An explicit Safe Settings trial still needs its keep/discard choice.
        // Present that choice now; it does not wait for remote cleanup either.
        if (session.safeSettingsTrial === true) {
            sessionFinished(0)
            return
        }
        terminalDelivered = true
        SdlGamepadKeyNavigation.enable()
        // Immediate pop can destroy this handler's scope. Map the surviving
        // window first, then remove the stream route in the same event turn.
        window.visible = true
        window.requestActivate()
        stackView.pop(StackView.Immediate)
    }

    function sessionFinished(portTestResult)
    {
        if (abandoned || terminalDelivered)
            return
        terminalDelivered = true

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()

        if (!quitAfter) {
            if (streamSegueErrorDialog.text) lastFailure = streamSegueErrorDialog.text
            streamSegueErrorDialog.text = ""
            everEstablished = everEstablished || session.connectionWasEstablished === true
            // Only an explicitly applied recommendation gets a new handoff deadline.
            // Each explicit mode handoff gets its own connection deadline; the
            // network failure budget still spans short failed streams.
            if (session.adjustingForHost === true ||
                    (session.stableVideoMilliseconds && session.stableVideoMilliseconds() >= 30000)) {
                retryAttempts = 0
                retryDeadline = 0
            }
            if (session.retryableTermination === true && (everEstablished || retrying)
                    && retryAttempts < retryDelays.length) {
                retrying = true
                retryPending = true
                if (!retryDeadline) {
                    retryDeadline = Date.now() + 60000
                }
                retryBudget.interval = Math.max(1, retryDeadline - Date.now())
                retryBudget.restart()
                stageText = session.adjustingForHost === true
                        ? qsTr("Applying your stream change…")
                        : qsTr("Connection interrupted. Reconnecting…")
                stagesDone = 0
                spinnerShown = true
                stageShown = true
                window.visible = true
                scheduleAfterCleanup()
                return
            }
            if (lastFailure || retrying || session.retryableTermination === true) {
                if (!lastFailure) lastFailure = qsTr("Moonwalk couldn't reconnect. Check that the host is running and both computers are online, then try again.")
                showTerminalFailure()
                return
            }
            if (session.safeSettingsTrial === true && session.hasVideoFrame === true) {
                trialOutcome = true
                showTerminalFailure()
                return
            }
            stackView.pop()
            window.visible = true
            return
        }

        if (streamSegueErrorDialog.text)
            streamSegueErrorDialog.diagnosticSession = session

        // Pop the StreamSegue off the stack if this is a GUI-based app launch
        if (!quitAfter) {
            stackView.pop()
        }

        if (quitAfter && !streamSegueErrorDialog.text) {
            // If this was a CLI launch without errors, exit now
            Qt.quit()
        }
        else {
            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    function sessionReadyForDeletion()
    {
        // Garbage collect the Session object since it's pretty heavyweight
        // and keeps other libraries (like SDL_TTF) around until it is deleted.
        if (quitAfter) { session = null; gc() }
    }

    // The legacy blue ToolBar (and its imperative toolBar.visible toggling)
    // is gone -- this page draws full-bleed with no header chrome at all, so
    // there's nothing to hide/show here any more. Escape still needs to work
    // to back out of a connecting/streaming segue without that toolbar's
    // back-arrow button, though: see Keys.onEscapePressed below.
    Keys.onEscapePressed: (event) => {
        if (stackView.depth > 1) {
            window.goBack()
        } else {
            event.accepted = false
        }
    }

    StackView.onDeactivating: {
        activated = false
        abandoned = true
        startSessionTimer.stop()
        spinnerTimer.stop()
        stopRetries()
        var previous = session
        preserveCleanupFeedback(previous)
        session = null
        if (previous && previous.cancelPendingStart)
            previous.cancelPendingStart()
        if (previous && previous.releaseRetryRetention) previous.releaseRetryRetention()
        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        activated = true
        retainSession()

        // The decoder probe releases its SDL test window before publishing ready.
        // Keep the Qt transition responsive while it finishes.
        spinnerTimer.start()
        startStreamWhenReady()
    }

    Connections {
        target: session
        enabled: root.activated
        ignoreUnknownSignals: true
        function onStageStarting(stage) { root.stageStarting(stage) }
        function onStageFailed(stage, errorCode, failingPorts) { root.stageFailed(stage, errorCode, failingPorts) }
        function onConnectionStarted() { root.connectionStarted() }
        function onDisplayLaunchError(text) { root.displayLaunchError(text) }
        function onHostCleanupFailed(message) {
            if (root.terminalDelivered && !root.abandoned)
                root.lastFailure += (root.lastFailure ? "\n\n" : "") + message
            else root.displayLaunchError(message)
        }
        function onQuitStarting() { root.quitStarting() }
        function onSessionFinished(result) { root.sessionFinished(result) }
        function onReadyForDeletion() { root.sessionReadyForDeletion() }
        function onLaunchWarningsChanged() { root.retainCurrentLaunchWarnings() }
        function onCleanupCompleteChanged() { root.scheduleAfterCleanup() }
        function onConnectionDiagnosticReady(result) {
            if (root.failed && !root.diagnosticAdded && result !== 0 && result !== -1) {
                root.diagnosticAdded = true
                root.lastFailure += "\n\n" + qsTr("The port check found blocked ports for internet streaming.")
            }
        }
    }
    Timer { id: cleanupWait; interval: 100; onTriggered: scheduleAfterCleanup() }
    Timer { id: retryTimer; onTriggered: retryWithSettings(false, true) }
    Timer {
        id: retryBudget
        objectName: "starlight.stream-retry-budget"
        interval: 60000
        onTriggered: {
            root.lastFailure = qsTr("Reconnection timed out. Check the host and network, then retry.")
            root.showTerminalFailure()
        }
    }

    Timer {
        id: spinnerTimer

        // Display the spinner appearance a bit to allow us to reach
        // the code in Session.exec() that pumps the event loop.
        // If we display it immediately, it will briefly hang in the
        // middle of the animation on Windows, which looks very
        // obviously broken.
        interval: 100
        onTriggered: spinnerShown = true
    }

    Timer {
        id: startSessionTimer
        onTriggered: {
            if (!activated || abandoned || terminalDelivered)
                return
            // Garbage collect QML stuff before we start streaming,
            // since we'll probably be streaming for a while and we
            // won't be able to GC during the stream.
            gc()

            // Run the streaming session to completion
            session.start()
        }
    }

    Loader {
        id: streamLoader
        active: false
        asynchronous: true

        onLoaded: {
            if (!activated || abandoned || terminalDelivered)
                return
            // Set the hint text. We do this here rather than
            // in the hintText control itself to synchronize
            // with Session.exec() which requires no concurrent
            // gamepad usage.
            streamHint = qsTr("Tip:") + " " + qsTr("Press %1 to disconnect").arg(SdlGamepadKeyNavigation.getConnectedGamepads() > 0 ?
                                                  qsTr("Start+Select+L1+R1") : qsTr("Ctrl+Alt+Shift+Q"))

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Initialize the session and probe for host/client capabilities
            if (!session.initialize(window)) {
                if (!streamSegueErrorDialog.text && !lastFailure)
                    displayLaunchError(qsTr("Moonwalk couldn't prepare video on this computer. Check the graphics driver and run the video check again."))
                sessionFinished(0);
                sessionReadyForDeletion();
                return;
            }

            // session.start() must never be gated on the toast below.
            // Launch warnings are advisory only; connecting to the host is
            // the time-critical path. showToast() self-dismisses on its own
            // timer, so nothing here needs to wait for that to happen - the
            // timer always fires on the next event-loop iteration.
            startSessionTimer.interval = 0

            // Route launch warnings through the app's one themed toast
            // mechanism (main.qml's showToast()) instead of hand-rolling raw
            // ToolTips. Joined into a single multi-line toast rather than
            // stacked separately; it's rendered over the now-connecting
            // screen and doesn't delay session.start() below.
            recordLaunchWarnings(session.launchWarnings)
            if (session.launchWarnings.length > 0) {
                for (var i = 0; i < session.launchWarnings.length; i++) {
                    console.warn(session.launchWarnings[i])
                }
                window.showToast((retainLaunchWarningsFn ? qsTr("See Host Details for the adjustments made when this stream started.") + "\n" : "")
                                 + session.launchWarnings.join("\n"))
            }

            // Start the session immediately regardless of any toast above
            startSessionTimer.start()
        }

        sourceComponent: Item {}
    }

    // Opaque ground, from the window's live desktop-following palette.
    Rectangle {
        anchors.fill: parent
        radius: typeof window !== "undefined" && window.csdRadius !== undefined
                ? window.csdRadius : 0
        color: typeof window !== "undefined"
               && window.transitionGround !== undefined
               ? window.transitionGround : Theme.ground
    }

    SessionStatusView {
        id: stageView
        objectName: "starlight.stream.progress"
        visible: !root.failed
        anchors.centerIn: parent
        width: Math.max(200, Math.min(parent.width - 48, 480))
        busy: root.spinnerShown
        heading: root.stageShown && root.hostName.length > 0
                 ? (root.isResume ? qsTr("Resuming on %1").arg(root.hostName) : qsTr("Connecting to %1").arg(root.hostName)) : ""
        hostSeed: root.stageShown ? root.hostSeed : ""
        hostIcon: root.hostIcon
        progress: root.stageProgress
        title: root.stageShown ? root.stageText : ""
        hint: root.streamHint
        Button {
            objectName: "starlight.stream.cancel"
            visible: root.spinnerShown
            text: qsTr("Cancel")
            onClicked: window.goBack()
        }
    }

    SessionStatusView {
        id: failureView
        objectName: "starlight.stream.failure"
        anchors.centerIn: parent
        width: Math.max(200, Math.min(parent.width - 48, 560))
        visible: root.failed
        hostSeed: root.hostSeed
        hostIcon: root.hostIcon
        hostDimmed: !root.trialOutcome
        title: root.stageText
        message: root.trialOutcome ? qsTr("This test used 1920×1080 at 60 FPS and 10 Mbps, with stereo, SDR, and automatic codec and decoder selection. Choose Keep Safe Settings to use them next time.") : root.lastFailure

        Button { objectName: "starlight.stream.retry-video-check"; text: SystemProperties.decoderProbeReady ? qsTr("Continue") : qsTr("Try Video Check Again"); highlighted: true; visible: root.decoderCheckFailed; enabled: !SystemProperties.decoderProbeRunning; onClicked: root.retryVideoCheck() }
        Button { objectName: "starlight.stream.retry"; text: qsTr("Retry"); highlighted: true; visible: !root.trialOutcome && !root.decoderCheckFailed; enabled: root.session && root.session.cleanupComplete === true; onClicked: root.retryWithSettings(false, false) }
        Button { objectName: "starlight.stream.retry-safe"; text: qsTr("Retry with Safe Settings"); visible: root.safeRetryRelevant && !root.trialOutcome && !root.decoderCheckFailed; enabled: root.session && root.session.cleanupComplete === true; onClicked: root.retryWithSettings(true, false) }
        Button { objectName: "starlight.stream.retry-last-working"; text: qsTr("Try Last Working Settings"); visible: !root.trialOutcome && !root.decoderCheckFailed && !!root.session && root.session.hasLastSuccessfulSettings(); enabled: root.session && root.session.cleanupComplete === true; onClicked: root.retryWithSettings(false, false, true) }
        Button { objectName: "starlight.stream.keep-safe"; text: qsTr("Keep Safe Settings"); highlighted: true; visible: root.trialOutcome && !root.trialSaved; onClicked: { if (root.session && root.session.keepSafeSettings()) root.trialSaved = true } }
        Button { objectName: "starlight.stream.return"; text: qsTr("Return to Hosts"); onClicked: window.goBack() }
        secondaryActions: Button { objectName: "starlight.stream.show-technical"; text: root.showTechnicalFailure ? qsTr("Hide Technical Details") : qsTr("Show Technical Details"); flat: true; visible: root.technicalFailure !== ""; onClicked: root.showTechnicalFailure = !root.showTechnicalFailure }
    }
    Label {
        objectName: "starlight.stream.technical"
        anchors.top: failureView.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: failureView.width
        text: root.trialSaved ? qsTr("Safe settings saved for this host.") : root.technicalFailure
        visible: root.failed && text !== "" && (root.trialSaved || root.showTechnicalFailure)
        opacity: 0.7
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }
}
