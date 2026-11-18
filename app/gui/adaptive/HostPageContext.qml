import QtQuick 2.15
import QtQuick.Window 2.15
import ComputerManager 1.0
import ComputerModel 1.0
import HostListModel 1.0
import Starlight.Application 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0

// Non-painted owner for host-page behavior.  The only object handed to a
// surface is facadeObject below; the model and workflow remain lexical ids.
Item {
    id: root
    width: 0
    height: 0

    // Production owns exactly one initialized model. Tests can replace the
    // source with their registered double without changing this contract.
    // Optional test/route override. The active default is always the owned
    // production ComputerModel below.
    property var hostSource: null
    readonly property var activeHostSource: hostSource
    property var hostList: null
    property var computerManager: ComputerManager
    property var preferences: StreamingPreferences
    property var systemProperties: SystemProperties
    property var sessionWarningsByHost: ({})
    property bool initialDiscoveryElapsed: false
    readonly property bool initialDiscoveryRunning: routeActive && selectedHostUuid.length === 0
                                                   && preferences && preferences.enableMdns !== false
    onInitialDiscoveryRunningChanged: initialDiscoveryElapsed = false
    property bool routeActive: true
    property string selectedHostUuid: ""
    property int selectedHostIndex: -1
    property bool hostReconciliationPending: false
    property bool explicitSelection: false
    property int modelGeneration: 0
    property int presentationRevision: 0
    property int nextOperationId: 1
    property var baseOverlay: emptyOverlay()
    property var interruptOverlay: emptyOverlay()
    property var queuedBaseOverlays: []
    property var queuedErrors: []
    property var wakeOperation: null
    property int wakeTimeoutSeconds: 60
    readonly property bool waitingForWake: wakeOperation !== null && wakeOperation.connectWhenReady === true
    property var pendingQuitOperations: ({})
    property var pendingEffects: ({})
    // A quit-then-launch effect keeps the inactive route's context alive until
    // the window-owned QuitSegue either hands the session to StreamSegue or
    // returns through the existing actionable recovery path.
    property bool quitRoutePinned: false
    property bool connectionTestInFlight: false
    property int connectionTestOperationId: -1
    property int connectionTestAttemptId: -1
    readonly property var launcherWindow: root.Window.window || null
    property int currentDisplayIndex: 0
    property int routeGeneration: 0
    property int nextEffectId: 1
    readonly property var selectedHostSnapshot: buildSelectedHostSnapshot()
    readonly property string selectedHostKind: deriveSelectedHostKind()
    property var resolutionOptionsState: buildResolutionOptions()
    readonly property var resolutionOptions: resolutionOptionsState
    // The display's refresh rate caps every frame rate on offer: more frames
    // than it shows are thrown away after costing the host and the network.
    property int displayRefreshRate: 60
    readonly property var frameRateOptions: buildFrameRateOptions(displayRefreshRate)
    // Bumped whenever the display is measured again; the plan depends on it.
    property int displayRevision: 0

    // The stream plan (app/streaming/streamplan.h) for the selected host: what
    // Automatic chooses and, for each Manual choice, what it is expected to
    // cost. This is the one place the plan is bound. The planner's
    // plan(request) takes { hostUuid, width, height, fps, bitrateKbps, codec },
    // 0 or "" meaning Automatic, and returns the StreamPlan fields by name.
    // Until a planner is registered the page shows this display's own size and
    // refresh rate, with no warnings.
    property var streamPlanner: null
    readonly property var streamPlan: buildStreamPlan()
    readonly property var facade: facadeObject
    readonly property var presets: {
        var revision = preferences.activePresetName
        return preferences.hostPresets ? preferences.hostPresets() : []
    }

    signal windowEffectRequested(var effect)

    Timer {
        interval: 250
        running: root.waitingForWake
        repeat: true
        onTriggered: root.checkWakeReadiness()
    }

    // Discovery continues in the backend; this timer only replaces the initial
    // loading presentation with useful no-results guidance.
    Timer {
        objectName: "initial-discovery-timer"
        interval: 8000
        running: root.initialDiscoveryRunning && !root.initialDiscoveryElapsed
        onTriggered: root.initialDiscoveryElapsed = true
    }

    HostListModel {
        id: hostListModel
        sourceModel: root.activeHostSource
        onModelReset: {
            root.modelGeneration += 1
            root.presentationRevision += 1
            root.requestHostReconciliation()
        }
        onRowsInserted: {
            root.presentationRevision += 1
            root.requestHostReconciliation()
        }
        onRowsRemoved: {
            root.presentationRevision += 1
            root.requestHostReconciliation()
        }
        onRowsMoved: {
            root.presentationRevision += 1
            root.requestHostReconciliation()
        }
        onDataChanged: root.presentationRevision += 1
        Component.onCompleted: {
            root.hostList = hostListModel
            root.requestHostReconciliation()
        }
    }
    HostPageFacade { id: facadeObject }
    HostWorkflow {
        id: pairingWorkflow
        objectName: "starlight.workflow.host"
    }

    Connections {
        target: root.activeHostSource
        ignoreUnknownSignals: true
        function onPairingCompleted(attemptId, error) {
            if (root.routeActive) root.completePairing(attemptId, error)
        }
        function onConnectionDiagnosticCompleted(attemptId, result, blockedPorts) {
            if (root.routeActive) root.completeConnectionTest(attemptId, result, blockedPorts)
        }
        function onWakeCompleted(index, error) {
            if (root.routeActive) root.completeWake(index, error)
        }
        function onAsyncError(message, uuid) {
            if (root.routeActive) root.presentError(message, uuid)
        }
    }

    Connections {
        target: root.computerManager
        ignoreUnknownSignals: true
        function onManualHostAddCompleted(attemptId, success, blocked) {
            if (root.routeActive) root.completeAddHost(attemptId, success, blocked)
        }
    }

    onLauncherWindowChanged: if (launcherWindow) refreshDisplayMetadata()

    Connections {
        target: root.launcherWindow
        ignoreUnknownSignals: true
        function onScreenChanged() { root.refreshDisplayMetadata() }
    }
    Connections {
        target: root.systemProperties
        ignoreUnknownSignals: true
        function onDisplayConfigurationChanged() { root.refreshDisplayMetadata() }
    }
    function refreshDisplayMetadata() {
        if (systemProperties && systemProperties.refreshDisplays) systemProperties.refreshDisplays()
        currentDisplayIndex = systemProperties && systemProperties.displayIndexForWindow
                ? systemProperties.displayIndexForWindow(launcherWindow) : 0
        var refresh = systemProperties && systemProperties.getRefreshRate
                ? systemProperties.getRefreshRate(currentDisplayIndex) : 0
        displayRefreshRate = refresh > 0 ? refresh : 60
        rebuildResolutionOptions()
        displayRevision += 1
    }

    Component.onDestruction: cancelBaseOperation(baseOverlay)

    onRouteActiveChanged: {
        if (!routeActive) {
            routeGeneration += 1
            invalidatePendingEffects()
            if (!quitRoutePinned)
                disposeRouteState()
        } else if (hostReconciliationPending) {
            requestHostReconciliation()
        }
    }

    function emptyOverlay() {
        return ({ kind: "none", operationId: -1, hostUuid: "",
                    modelGeneration: modelGeneration, payload: ({}) })
    }
    function makeOverlay(kind, hostUuid, payload) {
        return ({ kind: kind, operationId: allocateOperationId(), hostUuid: hostUuid || "",
                    modelGeneration: modelGeneration, hostIdentity: hostIdentity(hostUuid),
                    payload: payload || ({}) })
    }
    function hostIdentity(uuid) {
        var index = indexForUuid(uuid)
        if (index < 0) return ""
        return activeHostSource.hostIdentityForIndex
                ? activeHostSource.hostIdentityForIndex(index) : "generation:" + modelGeneration
    }
    function overlayHostIsCurrent(overlay) {
        return overlay.hostIdentity && hostIdentity(overlay.hostUuid) === overlay.hostIdentity
    }
    function resolveOverlayHost(kind, operationId) {
        if (baseOverlay.kind !== kind || baseOverlay.operationId !== operationId) return -1
        if (overlayHostIsCurrent(baseOverlay)) return indexForUuid(baseOverlay.hostUuid)
        var uuid = baseOverlay.hostUuid
        dismissOverlay(operationId)
        presentError(qsTranslate("PcView", "This host changed or was removed. Reopen this action to try again."), uuid)
        return -1
    }
    function allocateOperationId() { return nextOperationId++ }
    function indexForUuid(uuid) {
        if (!activeHostSource || !activeHostSource.uuidForIndex)
            return -1
        for (var index = 0; index < hostList.rowCount(); ++index) {
            if (activeHostSource.uuidForIndex(index) === uuid)
                return index
        }
        return -1
    }
    function nativeResolution() {
        var resolution = systemProperties && systemProperties.getNativeResolution
                ? systemProperties.getNativeResolution(currentDisplayIndex) : ({ width: 0, height: 0 })
        return ({ width: resolution.width > 0 ? resolution.width : 1920,
                    height: resolution.height > 0 ? resolution.height : 1080 })
    }
    function maximumStreamSize() {
        var size = nativeResolution()
        var decoder = systemProperties && systemProperties.maximumResolution
                ? systemProperties.maximumResolution : null
        return ({ width: decoder && decoder.width > 0 ? Math.min(size.width, decoder.width) : size.width,
                    height: decoder && decoder.height > 0 ? Math.min(size.height, decoder.height) : size.height })
    }
    // A 59 Hz report is a 59.94 Hz display, which shows 60 fps; likewise 119.
    function maximumFrameRate() { return frameRateLimit(displayRefreshRate) }
    function frameRateLimit(refresh) { return refresh % 10 === 9 ? refresh + 1 : refresh }
    function buildResolutionOptions() {
        var nativeSize = nativeResolution()
        var limit = maximumStreamSize()
        var sized = []
        function add(width, height) {
            width -= width % 2; height -= height % 2
            if (width < 256 || height < 256 || width > limit.width || height > limit.height) return
            for (var index = 0; index < sized.length; ++index)
                if (sized[index].w === width && sized[index].h === height) return
            sized.push({ text: width + "×" + height, w: width, h: height })
        }
        add(nativeSize.width, nativeSize.height)
        var nativeFits = sized.length > 0
        if (nativeFits) sized[0].text = qsTranslate("SettingsView", "Native") + " (" + nativeSize.width + "×" + nativeSize.height + ")"
        // The same shape, smaller, then the common sizes that fit.
        if (nativeSize.width >= 1280) {
            add(Math.round(nativeSize.width * 2 / 3), Math.round(nativeSize.height * 2 / 3))
            add(Math.round(nativeSize.width / 2), Math.round(nativeSize.height / 2))
        }
        add(3840, 2160); add(2560, 1440); add(1920, 1080); add(1280, 720)
        var rest = sized.slice(nativeFits ? 1 : 0)
        rest.sort(function(a, b) { return b.w * b.h - a.w * a.h })
        return [{ text: qsTranslate("SettingsView", "Automatic"), w: nativeSize.width, h: nativeSize.height, automatic: true }]
            .concat(nativeFits ? [sized[0]] : [], rest,
                    [{ text: qsTranslate("SettingsView", "Custom") + "…", w: 0, h: 0 }])
    }
    function buildFrameRateOptions(refresh) {
        var options = [{ text: qsTranslate("PcView", "Automatic (match display)"), value: 0, automatic: true }]
        var limit = frameRateLimit(refresh)
        var rates = [30, 60, 90, 120, 144, 165, 240]
        if (rates.indexOf(refresh) < 0 && refresh > 30) rates.push(refresh)
        rates.sort(function(a, b) { return a - b })
        for (var index = 0; index < rates.length; ++index)
            if (rates[index] <= limit) options.push({ text: qsTranslate("PcView", "%1 FPS").arg(rates[index]), value: rates[index] })
        options.push({ text: qsTranslate("SettingsView", "Custom") + "…", value: -1 })
        return options
    }
    function codecName(config) {
        return config === 1 ? "H.264" : config === 2 ? "HEVC" : config === 4 ? "AV1" : ""
    }
    // Every field the plan has, so a planner may leave out what it doesn't know.
    function buildStreamPlan() {
        var revision = displayRevision
        var prefs = preferences
        var nativeSize = nativeResolution()
        var manual = !!prefs && prefs.manualStream === true
        var request = ({
            hostUuid: selectedHostUuid,
            width: manual && prefs.automaticResolution !== true ? prefs.width : 0,
            height: manual && prefs.automaticResolution !== true ? prefs.height : 0,
            fps: manual && prefs.automaticFrameRate !== true ? prefs.fps : 0,
            bitrateKbps: manual && prefs.autoAdjustBitrate === false ? prefs.bitrateKbps : 0,
            codec: manual ? codecName(prefs.videoCodecConfig) : ""
        })
        var plan = ({
            width: request.width || nativeSize.width, height: request.height || nativeSize.height,
            fps: request.fps || displayRefreshRate, codec: request.codec,
            startBitrateKbps: 0, ceilingKbps: 0, floorKbps: 500,
            limitedByHost: false, limitReason: "",
            resolutionWarning: "", fpsWarning: "", codecWarning: "", bitrateWarning: ""
        })
        var planned = !selectedHostUuid ? null
            : streamPlanner && streamPlanner.plan ? streamPlanner.plan(request)
            : prefs && prefs.streamPlan ? prefs.streamPlan(selectedHostSnapshot.name || "", nativeSize.width, nativeSize.height,
                                                           displayRefreshRate > 0 ? displayRefreshRate : 60,
                                                           selectedHostSnapshot.serverCodecModeSupport || 0,
                                                           request.width, request.height, request.fps,
                                                           request.bitrateKbps, request.codec,
                                                           launcherWindow && launcherWindow.screen ? launcherWindow.screen.devicePixelRatio : 0,
                                                           selectedHostSnapshot.desktopScaleSupported === true,
                                                           selectedHostSnapshot.performanceContext || "",
                                                           launcherWindow && launcherWindow.screen ? launcherWindow.screen.name : "")
            : null
        if (planned) {
            for (var key in plan)
                if (planned[key] !== undefined && planned[key] !== null) plan[key] = planned[key]
        }
        return plan
    }
    function rebuildResolutionOptions() {
        resolutionOptionsState = buildResolutionOptions()
    }
    function buildSelectedHostSnapshot() {
        // Explicit revision dependency: invokable scalar reads do not carry
        // QML NOTIFY metadata, while the roles-only proxy does.
        var revision = presentationRevision
        var index = selectedHostIndex
        if (index < 0 || !activeHostSource || activeHostSource.uuidForIndex(index) !== selectedHostUuid)
            return ({ uuid: "", name: "", serverSoftware: "", serverSoftwareLabel: "", online: false,
                        paired: false, busy: false, wakeable: false,
                        statusUnknown: false, serverSupported: true,
                        details: "", address: "", hostOs: "",
                        desktopAppId: -1, runningAppId: 0, runningAppName: "" })
        var snapshot = activeHostSource.presentationForIndex
                ? activeHostSource.presentationForIndex(index) : ({})
        return ({ uuid: selectedHostUuid,
                    name: snapshot.name || "",
                    serverSoftware: snapshot.serverSoftware || "",
                    serverSoftwareLabel: snapshot.serverSoftwareLabel || "",
                    performanceContext: snapshot.performanceContext || "",
                    online: snapshot.online === true,
                    paired: snapshot.paired === true,
                    busy: snapshot.busy === true,
                    desktopAppId: activeHostSource.getDesktopAppId
                                  ? activeHostSource.getDesktopAppId(index) : -1,
                    runningAppId: activeHostSource.getRunningAppId
                                  ? activeHostSource.getRunningAppId(index) : 0,
                    runningAppName: activeHostSource.getRunningAppName
                                    ? activeHostSource.getRunningAppName(index) : "",
                    wakeable: snapshot.wakeable === true,
                    statusUnknown: snapshot.statusUnknown === true,
                    serverSupported: snapshot.serverSupported !== false,
                    details: snapshot.details || "",
                    desktopScaleSupported: snapshot.desktopScaleSupported === true,
                    adaptiveBitrateSupported: snapshot.adaptiveBitrateSupported === true || snapshot.apolloCompatible === true,
                    runtimeBitrateCandidate: snapshot.apolloCompatible === true,
                    virtualDisplaySupported: snapshot.virtualDisplaySupported === true,
                    virtualDisplayReady: snapshot.virtualDisplayReady === true,
                    clipboardReadAllowed: snapshot.clipboardReadAllowed !== false,
                    clipboardWriteAllowed: snapshot.clipboardWriteAllowed !== false,
                    launchAllowed: snapshot.launchAllowed !== false,
                    serverCodecModeSupport: snapshot.serverCodecModeSupport || 0,
                    address: snapshot.address || "",
                    hostOs: snapshot.hostOs || "" })
    }
    function deriveSelectedHostKind() {
        var software = selectedHostSnapshot.serverSoftware
        if (software === "Skylight") return "skylight"
        return software ? "stock" : "unknown"
    }
    function validHost(uuid, generation) {
        return generation === modelGeneration && indexForUuid(uuid) >= 0
    }
    function selectBeforeAccess(uuid) {
        var index = indexForUuid(uuid)
        if (index < 0)
            return false
        if (wakeOperation && wakeOperation.hostUuid !== uuid) wakeOperation = null
        selectedHostUuid = uuid
        selectedHostIndex = index
        initializeHostSelection(uuid, index)
        return true
    }
    function reselectHost() {
        if (!hostList) return
        var index = indexForUuid(selectedHostUuid)
        // Hosts arrive one at a time at startup. Until someone chooses, keep
        // moving to the host they used last as soon as it appears.
        if (!explicitSelection && preferences && preferences.lastSelectedHost) {
            var remembered = indexForUuid(preferences.lastSelectedHost())
            if (remembered >= 0) {
                index = remembered
                selectedHostUuid = activeHostSource.uuidForIndex(remembered)
            }
        }
        if (index < 0 && hostList.rowCount() > 0) {
            index = 0
            selectedHostUuid = activeHostSource.uuidForIndex(0)
        }
        if (index < 0)
            selectedHostUuid = ""
        selectedHostIndex = index
        initializeHostSelection(selectedHostUuid, selectedHostIndex)
    }
    function requestHostReconciliation() {
        if (!hostList)
            return
        if (!routeActive) {
            hostReconciliationPending = true
            return
        }
        hostReconciliationPending = false
        reselectHost()
    }
    function initializeHostSelection(uuid, index) {
        if (preferences && preferences.setCurrentHost)
            preferences.setCurrentHost(uuid)
        refreshDisplayMetadata()
        if (!uuid || !preferences)
            return

        var needsInitialDisplayMode = preferences.hasStoredResolution &&
                !preferences.hasStoredResolution(uuid)
        var nativeSize = systemProperties && systemProperties.getNativeResolution
                ? systemProperties.getNativeResolution(currentDisplayIndex) : ({ width: 0, height: 0 })
        var refreshRate = systemProperties && systemProperties.getRefreshRate
                ? systemProperties.getRefreshRate(currentDisplayIndex) : 0
        if (needsInitialDisplayMode &&
                (nativeSize.width <= 0 || nativeSize.height <= 0))
            return
        var initializedDisplayMode = !needsInitialDisplayMode
        if (preferences.initializeHostDisplayMode && nativeSize.width > 0 &&
                nativeSize.height > 0) {
            initializedDisplayMode = preferences.initializeHostDisplayMode(
                        uuid, nativeSize.width, nativeSize.height, refreshRate) ||
                    initializedDisplayMode
        }
        enforceDisplayLimits()
        if (!initializedDisplayMode)
            return

        var serverSoftware = activeHostSource && activeHostSource.serverSoftwareForIndex
                ? activeHostSource.serverSoftwareForIndex(index) : ""
        if (needsInitialDisplayMode && serverSoftware === "Skylight") {
            preferences.localCursor = true
            preferences.absoluteMouseMode = true
            if (preferences.save) preferences.save()
        }
    }
    function rememberSelection(uuid) {
        explicitSelection = true
        if (preferences && preferences.setLastSelectedHost)
            preferences.setLastSelectedHost(uuid)
    }
    function selectHostByUuid(uuid) {
        if (selectBeforeAccess(uuid)) {
            rememberSelection(uuid)
            return true
        }
        return false
    }
    function selectHostAtIndex(index) {
        if (!isInteger(index) || !hostList || index < 0 || index >= hostList.rowCount())
            return false
        var uuid = activeHostSource.uuidForIndex(index)
        if (!selectBeforeAccess(uuid))
            return false
        rememberSelection(uuid)
        return true
    }
    function releaseSession(session) {
        if (!session) return
        if (typeof session.release === "function")
            session.release()
        else if (typeof session.deleteLater === "function")
            session.deleteLater()
    }
    function invalidatePendingEffects() {
        for (var effectId in pendingEffects) {
            releaseSession(pendingEffects[effectId].payload.session)
            delete pendingEffects[effectId]
        }
        if (!quitRoutePinned)
            pendingQuitOperations = ({})
    }
    function setQuitRoutePinned(pinned) {
        quitRoutePinned = pinned === true
        if (!quitRoutePinned && !routeActive) {
            pendingQuitOperations = ({})
            disposeRouteState()
        }
    }
    function disposeRouteState() {
        if (baseOverlay.kind === "pairing" && computerManager && computerManager.cancelPairing)
            computerManager.cancelPairing(baseOverlay.payload.attemptId)
        pairingWorkflow.cancelCurrentPairingAttempt()
        cancelBaseOperation(baseOverlay)
        baseOverlay = emptyOverlay()
        interruptOverlay = emptyOverlay()
        queuedBaseOverlays = []
        queuedErrors = []
        connectionTestInFlight = false
        connectionTestOperationId = -1
        wakeOperation = null
    }
    function detachedValue(value) {
        if (value === null || typeof value !== "object") return value
        if (value instanceof Array) {
            var list = []
            for (var index = 0; index < value.length; ++index)
                list.push(detachedValue(value[index]))
            return list
        }
        var copy = ({})
        for (var key in value)
            copy[key] = detachedValue(value[key])
        return copy
    }
    function detachedPayload(payload) {
        var copy = ({})
        for (var key in payload)
            // Sessions are opaque ownership objects, not structured data.
            copy[key] = key === "session" ? payload[key] : detachedValue(payload[key])
        return copy
    }
    function freezeValue(value) {
        if (value && typeof value === "object") {
            for (var key in value)
                freezeValue(value[key])
            Object.freeze(value)
        }
        return value
    }
    function frozenDetachedValue(value) {
        return freezeValue(detachedValue(value))
    }
    function frozenEffectDescriptor(kind, effectId, payload) {
        var descriptorPayload = detachedPayload(payload || ({}))
        for (var key in descriptorPayload) {
            if (key !== "session")
                freezeValue(descriptorPayload[key])
        }
        Object.freeze(descriptorPayload)
        return Object.freeze(({ kind: kind, effectId: effectId,
                                routeGeneration: routeGeneration,
                                payload: descriptorPayload }))
    }
    function emitEffect(kind, payload) {
        var effectId = nextEffectId++
        // The stored descriptor is distinct from the signal value. Consumers
        // may annotate/mutate their signal copy without affecting ownership.
        var descriptor = frozenEffectDescriptor(kind, effectId, payload)
        pendingEffects[effectId] = descriptor
        var emitted = ({ kind: kind, effectId: effectId, routeGeneration: routeGeneration,
                         payload: detachedPayload(payload || ({})) })
        windowEffectRequested(emitted)
        return emitted
    }
    function acknowledgeWindowEffect(effectId, effectRouteGeneration, outcome) {
        var effect = pendingEffects[effectId]
        if (!effect || effect.routeGeneration !== effectRouteGeneration ||
                effect.routeGeneration !== routeGeneration || !routeActive)
            return false
        delete pendingEffects[effectId]
        if (outcome !== "consumed")
            releaseSession(effect.payload.session)
        return true
    }
    function promoteNextBase() {
        if (baseOverlay.kind !== "none" || queuedBaseOverlays.length === 0)
            return
        var queue = queuedBaseOverlays
        queuedBaseOverlays = []
        while (queue.length > 0) {
            var candidate = queue.shift()
            if (candidate.kind === "add-host" || candidate.kind === "connection-test"
                    || overlayHostIsCurrent(candidate)) {
                baseOverlay = candidate
                dispatchPromotedBase(candidate)
                break
            }
        }
        if (queue.length > 0)
            queuedBaseOverlays = queue
    }
    function presentBase(kind, uuid, payload) {
        var candidate = makeOverlay(kind, uuid, payload)
        if (baseOverlay.kind === "none")
            baseOverlay = candidate
        else
            queuedBaseOverlays = queuedBaseOverlays.concat([candidate])
        return candidate.operationId
    }
    function updateBaseOverlay(operationId, payload) {
        if (baseOverlay.operationId !== operationId) return false
        baseOverlay = ({ kind: baseOverlay.kind, operationId: baseOverlay.operationId,
                         hostUuid: baseOverlay.hostUuid, modelGeneration: baseOverlay.modelGeneration,
                         hostIdentity: baseOverlay.hostIdentity,
                         payload: payload })
        return true
    }
    function dispatchPromotedBase(overlay) {
        if (!overlay.payload || overlay.payload.dispatched) return
        if (overlay.kind === "connection-test") {
            var testId = activeHostSource.reserveConnectionTestAttemptId()
            connectionTestAttemptId = testId
            updateBaseOverlay(overlay.operationId,
                              ({ result: -1, blockedPorts: "", dispatched: true, attemptId: testId }))
            // The legacy method name is retained for compatibility. This is a
            // global Internet port test; it never contacts a selected host.
            activeHostSource.testConnectionForComputer(-1, testId)
        } else if (overlay.kind === "add-host" && overlay.payload.address) {
            var addId = computerManager.reserveManualAddAttemptId()
            updateBaseOverlay(overlay.operationId,
                              ({ address: overlay.payload.address, dispatched: true, attemptId: addId }))
            computerManager.addNewHostManually(overlay.payload.address, addId)
        }
    }
    function cancelBaseOperation(overlay) {
        if (overlay.kind === "connection-test") {
            var oldTest = connectionTestAttemptId
            connectionTestAttemptId = -1
            connectionTestOperationId = -1
            connectionTestInFlight = false
            if (oldTest >= 0 && activeHostSource && activeHostSource.cancelConnectionTest)
                activeHostSource.cancelConnectionTest(oldTest)
        } else if (overlay.kind === "add-host" && overlay.payload.attemptId && computerManager && computerManager.cancelManualAdd) {
            computerManager.cancelManualAdd(overlay.payload.attemptId)
        }
    }

    function dispatchCurrentBase(operationId) {
        if (baseOverlay.operationId === operationId)
            dispatchPromotedBase(baseOverlay)
    }
    function presentError(message, uuid, presentation) {
        var payload = ({ message: String(message || "Error") })
        var details = presentation || ({})
        for (var key in details)
            payload[key] = details[key]
        var candidate = makeOverlay("error", uuid, payload)
        if (interruptOverlay.kind === "none")
            interruptOverlay = candidate
        else
            queuedErrors = queuedErrors.concat([candidate])
        return candidate.operationId
    }
    function dismissOverlay(operationId) {
        if (interruptOverlay.operationId === operationId) {
            interruptOverlay = emptyOverlay()
            if (queuedErrors.length > 0) {
                interruptOverlay = queuedErrors[0]
                queuedErrors = queuedErrors.slice(1)
            }
            return
        }
        if (baseOverlay.operationId === operationId) {
            cancelBaseOperation(baseOverlay)
            baseOverlay = emptyOverlay()
            promoteNextBase()
        }
    }

    function snapshotForUuid(hostUuid) {
        var index = indexForUuid(hostUuid)
        return index >= 0 && activeHostSource && activeHostSource.presentationForIndex
                ? activeHostSource.presentationForIndex(index) : ({})
    }
    function requestDetails(hostUuid) {
        if (validHost(hostUuid, modelGeneration) && selectBeforeAccess(hostUuid)) {
            var snapshot = snapshotForUuid(hostUuid)
            var details = snapshot.details || ""
            var warnings = sessionWarningsByHost[hostUuid] || []
            if (warnings.length > 0)
                details += "\n\n" + qsTranslate("PcView", "Last stream adjustments")
                           + "\n" + warnings.join("\n")
            presentBase("details", hostUuid, ({ uuid: hostUuid, details: details,
                                               connectionAddress: preferences.connectionAddress || "" }))
        }
    }
    function submitConnectionAddress(operationId, value) {
        if (!routeActive || baseOverlay.kind !== "details"
                || baseOverlay.operationId !== operationId
                || baseOverlay.hostUuid !== selectedHostUuid
                || interruptOverlay.kind !== "none"
                || resolveOverlayHost("details", operationId) < 0) return false
        if (!setConnectionAddress(value)) return false
        var payload = Object.assign({}, baseOverlay.payload,
                                    { connectionAddress: preferences.connectionAddress || "" })
        return updateBaseOverlay(operationId, payload)
    }
    function requestOpenSettings() {
        if (routeActive)
            emitEffect("open-settings", ({}))
    }
    function requestAddHostOverlay() {
        if (!routeActive)
            return -1
        if (baseOverlay.kind === "add-host")
            return baseOverlay.operationId
        for (var index = 0; index < queuedBaseOverlays.length; ++index) {
            if (queuedBaseOverlays[index].kind === "add-host")
                return queuedBaseOverlays[index].operationId
        }
        return presentBase("add-host", "", ({ address: "" }))
    }

    function beginPairing() {
        if (!routeActive)
            return
        if (!selectedHostUuid) {
            emitEffect("toast", ({ text: qsTranslate("PcView", "Select a host to pair with first") }))
            return
        }
        if (baseOverlay.kind !== "none" || !selectBeforeAccess(selectedHostUuid))
            return
        var operationId = allocateOperationId()
        var attemptId = activeHostSource && activeHostSource.reservePairingAttemptId
                ? activeHostSource.reservePairingAttemptId() : -1
        if (attemptId < 0 || pairingWorkflow.beginPairingAttempt(attemptId) < 0)
            return
        var index = selectedHostIndex
        var pin = activeHostSource && activeHostSource.generatePinString
                ? activeHostSource.generatePinString() : ""
        if (activeHostSource && activeHostSource.pairComputer)
            activeHostSource.pairComputer(index, pin, attemptId)
        baseOverlay = ({ kind: "pairing", operationId: operationId, hostUuid: selectedHostUuid,
                         modelGeneration: modelGeneration,
                         payload: ({ attemptId: attemptId, pin: pin,
                                     hostName: activeHostSource && activeHostSource.nameForIndex
                                               ? activeHostSource.nameForIndex(index) : "" }) })
    }
    function cancelPairing(attemptId) {
        if (baseOverlay.kind !== "pairing" || baseOverlay.payload.attemptId !== attemptId)
            return
        var operationId = baseOverlay.operationId
        pairingWorkflow.cancelCurrentPairingAttempt()
        if (computerManager && computerManager.cancelPairing)
            computerManager.cancelPairing(attemptId)
        dismissOverlay(operationId)
    }
    function completePairing(attemptId, error) {
        var completion = pairingWorkflow.completePairingAttempt(attemptId, error)
        if (!completion.closeCurrent)
            return
        if (baseOverlay.kind === "pairing" && baseOverlay.payload.attemptId === completion.attemptId)
            dismissOverlay(baseOverlay.operationId)
        if (completion.showError)
            presentError(completion.error, selectedHostUuid)
        else if (completion.closeCurrent && !completion.error)
            emitEffect("toast", ({ text: qsTranslate("PcView", "Paired with %1").arg(selectedHostSnapshot.name || ""),
                                   actionText: qsTranslate("PcView", "Connect"), connectHostUuid: selectedHostUuid }))
    }

    function submitAddHost(address) {
        var normalized = String(address || "").trim()
        if (!normalized) {
            if (baseOverlay.kind === "add-host")
                updateBaseOverlay(baseOverlay.operationId,
                                  ({ address: "", error: qsTranslate("PcView", "Enter a host address.") }))
            else
                presentError(qsTranslate("PcView", "Enter a host address."), "")
            return
        }
        if (computerManager && computerManager.isValidHostAddress
                && !computerManager.isValidHostAddress(normalized)) {
            if (baseOverlay.kind === "add-host")
                updateBaseOverlay(baseOverlay.operationId,
                                  ({ address: normalized, error: qsTranslate("PcView", "Enter a valid IP address or hostname.") }))
            else
                presentError(qsTranslate("PcView", "Enter a valid IP address or hostname."), "")
            return
        }
        var operationId = baseOverlay.kind === "add-host"
                ? baseOverlay.operationId : presentBase("add-host", "", ({ address: normalized }))
        if (baseOverlay.kind === "add-host" && baseOverlay.operationId === operationId &&
                baseOverlay.payload.dispatched)
            return
        if (baseOverlay.kind === "add-host" && baseOverlay.operationId === operationId)
            updateBaseOverlay(operationId, ({ address: normalized }))
        dispatchCurrentBase(operationId)
    }
    function completeAddHost(attemptId, success, blocked) {
        if (baseOverlay.kind !== "add-host" || baseOverlay.payload.attemptId !== attemptId)
            return
        if (success)
            dismissOverlay(baseOverlay.operationId)
        else
            updateBaseOverlay(baseOverlay.operationId,
                              ({ address: baseOverlay.payload.address || "",
                                 error: blocked
                                        ? qsTranslate("PcView", "A firewall or network may be blocking this host. Check the connection and try again.")
                                        : qsTranslate("PcView", "Could not add this host. Check the address and confirm its streaming host is running.") }))
    }
    function testConnection() {
        if (connectionTestInFlight)
            return
        connectionTestInFlight = true
        connectionTestOperationId = presentBase("connection-test", "", ({ result: -1, blockedPorts: "" }))
        dispatchCurrentBase(connectionTestOperationId)
    }
    function completeConnectionTest(attemptId, result, blockedPorts) {
        if (!connectionTestInFlight || connectionTestAttemptId !== attemptId)
            return
        connectionTestInFlight = false
        connectionTestAttemptId = -1
        if (baseOverlay.kind === "connection-test" && baseOverlay.operationId === connectionTestOperationId)
            baseOverlay = ({ kind: "connection-test", operationId: connectionTestOperationId, hostUuid: "",
                             modelGeneration: modelGeneration, payload: ({ result: result, blockedPorts: blockedPorts || "" }) })
    }
    function wakeHost(hostUuid, connectWhenReady) {
        var index = indexForUuid(hostUuid)
        if (index < 0 || wakeOperation)
            return
        wakeOperation = ({ operationId: allocateOperationId(), hostUuid: hostUuid,
                            generation: modelGeneration, identity: hostIdentity(hostUuid),
                            deadline: Date.now() + wakeTimeoutSeconds * 1000,
                            connectWhenReady: connectWhenReady === true })
        var host = snapshotForUuid(hostUuid)
        emitEffect("toast", ({ text: qsTranslate("PcView", "Waking %1…").arg(host.name || "") }))
        if (activeHostSource && activeHostSource.wakeComputer) {
            var immediate = activeHostSource.wakeComputer(index)
            if (immediate)
                completeWake(index, immediate)
        }
    }
    function completeWake(index, error) {
        var operation = wakeOperation
        if (!operation)
            return
        if (!operation.connectWhenReady || error) wakeOperation = null
        // The backend completion exposes only an index. With one wake in
        // flight it is merely a completion notification, never identity: a
        // reset/reorder must resolve the original UUID afresh.
        if (indexForUuid(operation.hostUuid) < 0)
            return
        if (error)
            presentError(error, operation.hostUuid)
        else if (operation.connectWhenReady)
            checkWakeReadiness()
        else
            emitEffect("toast", ({ text: qsTranslate("PcView", "Wake packet sent") }))
    }
    function cancelWakeConnect() { wakeOperation = null }
    function checkWakeReadiness() {
        var operation = wakeOperation
        if (!operation || !operation.connectWhenReady) return
        if (!routeActive || hostIdentity(operation.hostUuid) !== operation.identity) {
            wakeOperation = null
            return
        }
        if (Date.now() >= operation.deadline) {
            wakeOperation = null
            presentError(qsTranslate("PcView", "The host did not become ready in time. Check its power and Wake-on-LAN settings, then try again."), operation.hostUuid)
            return
        }
        var host = snapshotForUuid(operation.hostUuid)
        if (host.online !== true || host.statusUnknown === true) return
        if (host.paired !== true || host.serverSupported === false) {
            wakeOperation = null
            presentError(qsTranslate("PcView", "The host is awake but needs pairing or a compatible Moonwalk version before connecting."), operation.hostUuid)
            return
        }
        var hostIndex = indexForUuid(operation.hostUuid)
        if (activeHostSource.getDesktopAppId(hostIndex) < 0) return // app list may still be refreshing
        wakeOperation = null
        requestConnect(operation.hostUuid)
    }
    function requestResume(hostUuid) {
        if (!routeActive || !selectBeforeAccess(hostUuid)) return
        var host = selectedHostSnapshot
        if (!host.online || !host.paired || !host.serverSupported || host.runningAppId <= 0) return
        var session = activeHostSource.createSessionForCurrentGame(selectedHostIndex)
        if (!session) return presentError(qsTranslate("PcView", "The running session changed. Refresh the host and try again."), hostUuid)
        emitEffect("launch", ({ session: session, hostUuid: hostUuid, desktop: false,
                                 appName: host.runningAppName, resume: true }))
    }
    function requestEndSession(hostUuid) {
        if (!routeActive || !selectBeforeAccess(hostUuid) || selectedHostSnapshot.runningAppId <= 0) return
        presentBase("confirm-end", hostUuid, ({ appId: selectedHostSnapshot.runningAppId,
                                               runningAppLabel: selectedHostSnapshot.runningAppName }))
    }
    function confirmEndSession(operationId) {
        var index = resolveOverlayHost("confirm-end", operationId)
        if (index < 0) return
        if (activeHostSource.getRunningAppId(index) !== baseOverlay.payload.appId) {
            dismissOverlay(operationId)
            return presentError(qsTranslate("PcView", "The running session changed. Reopen this action to try again."), selectedHostUuid)
        }
        activeHostSource.quitRunningApp(index)
        dismissOverlay(operationId)
    }

    function requestConnect(hostUuid, safeSettings) {
        if (!routeActive || !selectBeforeAccess(hostUuid))
            return
        var host = snapshotForUuid(hostUuid)
        if (host.serverSupported === false) {
            presentError("The host server version is unsupported", hostUuid,
                         ({ code: "server-unsupported", hostName: host.name || "" }))
            return
        }
        var desktopAppId = activeHostSource && activeHostSource.getDesktopAppId
                ? activeHostSource.getDesktopAppId(selectedHostIndex) : -1
        if (desktopAppId < 0) {
            presentError("This host does not expose a Desktop stream target", hostUuid,
                         ({ code: "desktop-target-unavailable", hostName: host.name || "" }))
            return
        }
        var runningAppId = activeHostSource && activeHostSource.getRunningAppId
                ? activeHostSource.getRunningAppId(selectedHostIndex) : 0
        if (runningAppId === 0 && host.launchAllowed === false) {
            presentError(qsTranslate("PcView", "This device cannot launch applications. Enable Launch applications in the host's client settings, then reconnect."), hostUuid)
            return
        }
        if (host.foreignDesktopRetained === true) {
            if (host.desktopTakeoverAvailable !== true) {
                presentError(qsTranslate("PcView", "This host retains a Desktop stream from another device. End that stream on the original device before connecting."), hostUuid)
                return
            }
            presentBase("confirm-quit", hostUuid, ({ retainedDesktopTakeover: true,
                runningAppLabel: qsTranslate("PcView", "Desktop"), safeSettings: safeSettings === true }))
            return
        }
        if (runningAppId !== 0 && runningAppId !== desktopAppId) {
            presentBase("confirm-quit", hostUuid, ({ runningAppLabel: activeHostSource.getRunningAppName(selectedHostIndex), safeSettings: safeSettings === true }))
            return
        }
        emitLaunch(hostUuid, runningAppId !== 0 && runningAppId === desktopAppId, safeSettings)
    }
    function emitLaunch(hostUuid, resume, safeSettings) {
        if (!routeActive || !selectBeforeAccess(hostUuid))
            return
        var session = activeHostSource && activeHostSource.createSessionForDesktop
                ? activeHostSource.createSessionForDesktop(selectedHostIndex, safeSettings === true) : null
        if (!session) {
            return presentError("Unable to create stream session", hostUuid)
        }
        emitEffect("launch", ({ session: session, hostUuid: hostUuid,
                                 desktop: true, resume: resume }))
    }
    function confirmQuitAndConnect(operationId) {
        if (!routeActive || resolveOverlayHost("confirm-quit", operationId) < 0)
            return
        var uuid = baseOverlay.hostUuid
        if (!selectBeforeAccess(uuid)) return
        var session = activeHostSource && activeHostSource.createSessionForDesktop
                ? activeHostSource.createSessionForDesktop(selectedHostIndex, baseOverlay.payload.safeSettings === true, baseOverlay.payload.retainedDesktopTakeover === true) : null
        if (!session) {
            presentError("Unable to create stream session", uuid)
            return
        }
        if (baseOverlay.payload.retainedDesktopTakeover === true) {
            dismissOverlay(operationId)
            emitEffect("launch", ({session: session, hostUuid: uuid, desktop: true, resume: false}))
            return
        }
        var quitOperationId = allocateOperationId()
        var runningAppLabel = baseOverlay.payload.runningAppLabel || ""
        pendingQuitOperations[quitOperationId] = ({ operationId: quitOperationId,
                                                      hostUuid: uuid,
                                                      generation: modelGeneration,
                                                      runningAppLabel: runningAppLabel })
        dismissOverlay(operationId)
        emitEffect("quit-then-launch", ({ operationId: quitOperationId, session: session,
                                           hostUuid: uuid, runningAppLabel: runningAppLabel,
                                           desktop: true, resume: false }))
    }
    function quitRunningAppForOperation(operationId) {
        var operation = pendingQuitOperations[operationId]
        if (!operation) {
            presentError("The requested quit operation is no longer available", "")
            return false
        }
        delete pendingQuitOperations[operationId]
        if (operation.generation !== modelGeneration) {
            presentError("The host list changed before the running app could be closed", operation.hostUuid)
            return false
        }
        var index = indexForUuid(operation.hostUuid)
        if (index < 0) {
            presentError("The selected host is no longer available", operation.hostUuid)
            return false
        }
        if (activeHostSource && activeHostSource.quitRunningApp) {
            activeHostSource.quitRunningApp(index)
            return true
        }
        presentError("Unable to dispatch the running app quit request", operation.hostUuid)
        return false
    }
    // A window-owned quit segue has not dispatched the backend operation
    // until it calls quitRunningAppForOperation(). If construction or lease
    // setup fails first, main.qml cancels only this exact pending operation.
    function cancelQuitOperation(operationId) {
        if (!pendingQuitOperations[operationId])
            return false
        delete pendingQuitOperations[operationId]
        return true
    }
    function rejectQuit(operationId) { dismissOverlay(operationId) }

    function requestRename(hostUuid) {
        if (validHost(hostUuid, modelGeneration)) {
            var snapshot = snapshotForUuid(hostUuid)
            presentBase("rename-host", hostUuid, ({ name: snapshot.name || "" }))
        }
    }
    function submitRename(operationId, name) {
        var normalized = String(name || "").trim()
        if (!normalized) {
            if (!normalized) presentError("Host name is required", baseOverlay.hostUuid)
            return
        }
        var index = resolveOverlayHost("rename-host", operationId)
        if (index < 0) return
        if (activeHostSource && activeHostSource.renameComputer) activeHostSource.renameComputer(index, normalized)
        dismissOverlay(operationId)
    }
    function requestDelete(hostUuid) {
        if (validHost(hostUuid, modelGeneration)) {
            var snapshot = snapshotForUuid(hostUuid)
            presentBase("confirm-delete", hostUuid, ({ name: snapshot.name || "" }))
        }
    }
    function confirmDelete(operationId) {
        var index = resolveOverlayHost("confirm-delete", operationId)
        if (index < 0) return
        if (activeHostSource && activeHostSource.deleteComputer) activeHostSource.deleteComputer(index)
        dismissOverlay(operationId)
    }
    function isInteger(value) { return typeof value === "number" && isFinite(value) && Math.floor(value) === value }
    function isOneOf(value, values) {
        for (var index = 0; index < values.length; ++index) if (value === values[index]) return true
        return false
    }
    function requestCustomResolution(hostUuid) {
        if (!selectBeforeAccess(hostUuid)) return
        var limit = maximumStreamSize()
        presentBase("custom-resolution", hostUuid, ({ width: Math.min(preferences.width, limit.width), height: Math.min(preferences.height, limit.height),
                                                      minimum: 256, maximum: Math.max(limit.width, limit.height),
                                                      maximumWidth: limit.width, maximumHeight: limit.height }))
    }
    function submitCustomResolution(operationId, width, height) {
        if (!isInteger(width) || !isInteger(height) ||
                width < 256 || width > 8192 || height < 256 || height > 8192 ||
                resolveOverlayHost("custom-resolution", operationId) < 0 ||
                !selectBeforeAccess(baseOverlay.hostUuid)) return
        setResolution(width, height); dismissOverlay(operationId)
    }
    function requestCustomFrameRate(hostUuid) { if (selectBeforeAccess(hostUuid)) presentBase("custom-frame-rate", hostUuid, ({ fps: Math.min(preferences.fps, maximumFrameRate()), minimum: 10, maximum: maximumFrameRate() })) }
    function submitCustomFrameRate(operationId, fps) {
        if (!isInteger(fps) || fps < 10 || fps > maximumFrameRate() ||
                resolveOverlayHost("custom-frame-rate", operationId) < 0 ||
                !selectBeforeAccess(baseOverlay.hostUuid)) return
        setFrameRate(fps); dismissOverlay(operationId)
    }

    function selectedPreference() { return selectedHostUuid && selectBeforeAccess(selectedHostUuid) }
    function saveIfChanged(propertyName, value) {
        if (!selectedPreference() || preferences[propertyName] === value) return false
        preferences[propertyName] = value; preferences.save(); return true
    }
    // A choice of the person's own makes the host Manual: for the stream, or for
    // the group named (manualDisplay, manualInput). Reports whether that changed,
    // so a caller whose own write turned out a no-op still saves it.
    function chooseManually(group) {
        var property = group || "manualStream"
        if (preferences[property] === true) return false
        preferences[property] = true
        return true
    }
    // Display and Input: Automatic keeps the saved choices for when Manual comes
    // back, and sessions use the automatic values meanwhile.
    function setDisplayMode(mode) {
        if (["automatic", "manual"].indexOf(mode) >= 0) saveIfChanged("manualDisplay", mode === "manual")
    }
    function setInputMode(mode) {
        if (["automatic", "manual"].indexOf(mode) >= 0) saveIfChanged("manualInput", mode === "manual")
    }
    function saveManualChoice(becameManual, saved) { if (becameManual && !saved) preferences.save() }
    function setResolution(width, height) {
        var limit = maximumStreamSize()
        if (!selectedPreference() || !isInteger(width) || !isInteger(height) || width < 256 || height < 256
                || width > limit.width || height > limit.height) return
        var becameManual = chooseManually()
        saveManualChoice(becameManual, preferences.setResolution(width, height))
    }
    function setAutomaticResolution() {
        if (!selectedPreference() || !preferences.setAutomaticResolution) return
        var size = systemProperties.getNativeResolution(currentDisplayIndex)
        preferences.setAutomaticResolution(size.width, size.height)
    }
    // A saved setup, the last working settings or a different display can
    // bring back a size or rate this display can't show; those go Automatic.
    // Runs inside host selection, so it writes directly rather than through
    // selectedPreference(), which would select the host again.
    function enforceDisplayLimits() {
        if (!selectedHostUuid || !preferences) return
        var limit = maximumStreamSize()
        var size = nativeResolution()
        if (preferences.automaticResolution !== true && preferences.setAutomaticResolution
                && (preferences.width > limit.width || preferences.height > limit.height))
            preferences.setAutomaticResolution(size.width, size.height)
        if (preferences.automaticFrameRate !== true && preferences.setAutomaticFrameRate
                && preferences.fps > maximumFrameRate())
            preferences.setAutomaticFrameRate(displayRefreshRate)
    }
    function setStreamMode(mode) {
        if (["automatic", "manual"].indexOf(mode) < 0 || !selectedPreference()) return
        if (mode === "manual") {
            if (chooseManually()) preferences.save()
            return
        }
        if (preferences.manualStream === false && preferences.automaticResolution === true
                && preferences.automaticFrameRate === true && preferences.autoAdjustBitrate === true
                && preferences.videoCodecConfig === preferences.VCC_AUTO) return
        // Everything chosen for this host again, in one write.
        var size = nativeResolution()
        preferences.manualStream = false
        preferences.automaticResolution = true
        preferences.width = size.width
        preferences.height = size.height
        preferences.automaticFrameRate = true
        preferences.fps = displayRefreshRate
        preferences.autoAdjustBitrate = true
        preferences.adaptiveBitrate = true
        preferences.videoCodecConfig = preferences.VCC_AUTO
        preferences.bitrateKbps = recommendedBitrate()
        preferences.save()
    }
    // "Limit bandwidth": off lets the stream use what the network gives it; on
    // makes the chosen amount a ceiling, never below what this size needs.
    function setBandwidthLimit(enabled) {
        if (typeof enabled !== "boolean" || !selectedPreference()) return
        if (enabled) setQualityMode("manual")
        else setQualityMode(selectedHostSnapshot.adaptiveBitrateSupported ? "adaptive" : "recommended")
    }
    function setDisplayTarget(value) {
        if (["host", "auto", "virtual", "physical"].indexOf(value) < 0 ||
                (value !== "host" && !selectedHostSnapshot.virtualDisplaySupported) || !selectedPreference()) return
        saveManualChoice(chooseManually("manualDisplay"), saveIfChanged("displayTarget", value))
    }
    function setFrameRate(fps) {
        if (!selectedPreference() || !isInteger(fps) || fps < 10 || fps > maximumFrameRate()) return
        var becameManual = chooseManually()
        saveManualChoice(becameManual, preferences.setFrameRate(fps))
    }
    function setAutomaticFrameRate() {
        if (!selectedPreference() || !preferences.setAutomaticFrameRate) return
        refreshDisplayMetadata()
        preferences.setAutomaticFrameRate(systemProperties && systemProperties.getRefreshRate
                                         ? systemProperties.getRefreshRate(currentDisplayIndex) : 0)
    }
    function boundedBitrate(value) { return preferences.effectiveBitrate(value) }
    function recommendedBitrate() { return boundedBitrate(preferences.getDefaultBitrate(preferences.width, preferences.height, preferences.fps, preferences.enableYUV444)) }
    function setBitrateKbps(value) {
        if (!selectedPreference() || !isInteger(value) || value < 1) return
        var bounded = boundedBitrate(Math.max(value, streamPlan.floorKbps || 0))
        if (preferences.bitrateKbps === bounded && !preferences.autoAdjustBitrate && preferences.manualStream === true) return
        chooseManually()
        preferences.bitrateKbps = bounded
        preferences.autoAdjustBitrate = false
        preferences.save()
    }
    function setAutoAdjustBitrate(value) {
        if (typeof value !== "boolean" || !selectedPreference() || preferences.autoAdjustBitrate === value) return
        preferences.autoAdjustBitrate = value
        if (value)
            preferences.bitrateKbps = recommendedBitrate()
        preferences.save()
    }
    function setLocalCursorEnabled(value) {
        if (typeof value === "boolean" && selectedPreference())
            saveManualChoice(chooseManually("manualInput"), saveIfChanged("localCursor", value))
    }
    function setVideoCodec(value) {
        if (!isOneOf(value, [preferences.VCC_AUTO, preferences.VCC_FORCE_H264, preferences.VCC_FORCE_HEVC, preferences.VCC_FORCE_AV1])
                || !selectedPreference()) return
        var becameManual = chooseManually()
        saveManualChoice(becameManual, saveIfChanged("videoCodecConfig", value))
    }
    function setAbsoluteMouseMode(value) {
        if (typeof value !== "boolean" || !selectedPreference()) return
        if (preferences.absoluteMouseMode === value && !preferences.automaticPointer && preferences.manualInput === true) return
        preferences.manualInput = true
        preferences.automaticPointer = false
        preferences.absoluteMouseMode = value
        preferences.save()
    }
    function setAutomaticPointer(value) {
        if (typeof value !== "boolean" || !selectedPreference()
                || (preferences.automaticPointer === value && preferences.manualInput === true)) return
        preferences.manualInput = true
        preferences.automaticPointer = value
        if (value) preferences.absoluteMouseMode = true
        preferences.save()
    }
    function setViewScale(value) {
        if (["fit", "actual"].indexOf(value) >= 0 && selectedPreference())
            saveManualChoice(chooseManually("manualDisplay"), saveIfChanged("viewScale", value))
    }
    function setDesktopScale(value) {
        if (["auto", "1", "2"].indexOf(value) >= 0 && selectedHostSnapshot.desktopScaleSupported && selectedPreference())
            saveManualChoice(chooseManually("manualDisplay"), saveIfChanged("desktopScale", value))
    }
    function setAdaptiveBitrate(value) {
        if (typeof value === "boolean" && selectedHostSnapshot.adaptiveBitrateSupported)
            saveIfChanged("adaptiveBitrate", value)
    }
    function setQualityMode(mode) {
        if (["recommended", "adaptive", "manual"].indexOf(mode) < 0
                || !selectedPreference()
                || (mode === "adaptive" && !selectedHostSnapshot.adaptiveBitrateSupported)) return
        var automatic = mode !== "manual"
        var adaptive = mode === "adaptive"
        var bitrate = automatic ? recommendedBitrate() : boundedBitrate(Math.max(preferences.bitrateKbps, streamPlan.floorKbps || 0))
        if (preferences.autoAdjustBitrate === automatic
                && preferences.adaptiveBitrate === adaptive
                && preferences.bitrateKbps === bitrate
                && (automatic || preferences.manualStream === true)) return
        if (!automatic) preferences.manualStream = true
        preferences.autoAdjustBitrate = automatic
        preferences.adaptiveBitrate = adaptive
        preferences.bitrateKbps = bitrate
        preferences.save()
    }
    function setLocalTextInput(value) { if (typeof value === "boolean") saveIfChanged("localTextInput", value) }
    function setConnectionAddress(value) {
        var normalized = String(value || "").trim()
        if (normalized && computerManager.isValidHostAddress && !computerManager.isValidHostAddress(normalized)) {
            presentError(qsTranslate("PcView", "Enter a valid IP address or hostname, optionally with its port."), selectedHostUuid)
            return false
        }
        saveIfChanged("connectionAddress", normalized)
        return true
    }
    function toastActionTriggered(payload) {
        if (payload && payload.connectHostUuid) requestConnect(payload.connectHostUuid)
    }
    function requestPresets(hostUuid) {
        if (validHost(hostUuid, modelGeneration) && selectBeforeAccess(hostUuid))
            presentBase("presets", hostUuid, ({}))
    }
    function savePreset(name) { return selectedPreference() && preferences.saveHostPreset && preferences.saveHostPreset(name) }
    function applyPreset(id) {
        var applied = selectedPreference() && preferences.applyHostPreset && preferences.applyHostPreset(id)
        if (applied) enforceDisplayLimits()
        return applied
    }
    function renamePreset(id, name) { return selectedPreference() && preferences.renameHostPreset && preferences.renameHostPreset(id, name) }
    function duplicatePreset(id, name) { return selectedPreference() && preferences.duplicateHostPreset && preferences.duplicateHostPreset(id, name) }
    function removePreset(id) { return selectedPreference() && preferences.removeHostPreset && preferences.removeHostPreset(id) }
    function resetLearnedPerformance() {
        if (!selectedPreference() || !preferences.resetLearnedPerformance || !preferences.resetLearnedPerformance()) return false
        displayRevision += 1
        emitEffect("toast", ({text: qsTranslate("PcView", "Moonwalk will measure this host again on the next connection")}))
        return true
    }
    function restoreSuccessfulSettings() {
        if (!selectedPreference() || !preferences.restoreLastSuccessfulSettings || !preferences.restoreLastSuccessfulSettings())
            presentError(qsTranslate("PcView", "No successful configuration has been saved for this host yet."), selectedHostUuid)
        else {
            enforceDisplayLimits()
            emitEffect("toast", ({ text: qsTranslate("PcView", "Last working settings restored") }))
        }
    }
}
