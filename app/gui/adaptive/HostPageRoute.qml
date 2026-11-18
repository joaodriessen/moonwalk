import QtQuick 2.15
import QtQuick.Controls 2.15

import ComputerModel 1.0
import ".." as Legacy
import "gnome" as Gnome

// Routes shared host-page actions and state to the selected desktop surface.
Item {
    id: root

    property string shellRouteId:
        typeof window !== "undefined" && window.adaptiveShellActive === true
        ? "adaptive-host" : "legacy-host"
    readonly property string shellRouteKind: "hosts"
    readonly property bool shellRouteActive: StackView.status === StackView.Active
    property var hostSource: null
    readonly property var context: contextLoader.item
    readonly property var page: context ? context.facade : null
    readonly property var legacySurface: legacySurfaceLoader.item
    // Compatibility landmarks expose context-owned state to the existing
    // characterization fixtures without leaking it into the painted surface.
    readonly property var computerModel: context ? context.activeHostSource : null
    readonly property bool addHostOverlayActive:
        page && page.baseOverlay.kind === "add-host"
    // Records the route-boundary delivery that owns Ctrl+N. Keeping this on
    // the route makes duplicate-delivery coverage observable without giving
    // the window a direct facade shortcut.
    readonly property int addHostOverlayForwardCount:
        lifecycleState.addHostOverlayForwardCount
    // The legacy page still owns painting, but activation is route-owned. Keep
    // read-only counter projections so duplicate loader/status delivery stays
    // observable without making lifecycle state route-writable.
    readonly property int contextActivationTransitionCount:
        lifecycleState.contextActivationTransitionCount
    readonly property int contextDeactivationTransitionCount:
        lifecycleState.contextDeactivationTransitionCount
    readonly property int surfaceActivationForwardCount:
        lifecycleState.surfaceActivationForwardCount
    readonly property int surfaceDeactivationTransitionCount:
        lifecycleState.surfaceDeactivationTransitionCount

    signal addHostOverlayActivated()
    signal routeDestroyed()
    signal windowEffectRequested(var effect)

    QtObject {
        id: lifecycleState

        property int addHostOverlayForwardCount: 0
        property int contextActivationTransitionCount: 0
        property int contextDeactivationTransitionCount: 0
        property int surfaceActivationForwardCount: 0
        property int surfaceDeactivationTransitionCount: 0
        property bool routeActive: false
        property bool surfaceActivated: false
    }

    // Legacy view tests exercise the route through these pairing properties.
    readonly property int testPairAttemptId:
        page.baseOverlay.kind === "pairing" ? page.baseOverlay.payload.attemptId : -1
    readonly property string testOverlayKind:
        page.baseOverlay.kind

    function beginPairing() { page.beginPairing() }
    function cancelPairing(attemptId) { page.cancelPairing(attemptId) }
    function testCompletePair(attemptId, succeeded) {
        if (attemptId === testPairAttemptId && computerModel && computerModel.pairingCompleted)
            computerModel.pairingCompleted(attemptId, succeeded ? undefined : "test pairing failure")
    }
    function testSelectHostIndex(index) {
        if (computerModel && computerModel.uuidForIndex)
            page.selectHostByUuid(computerModel.uuidForIndex(index))
    }
    function requestAddHostOverlay() {
        if (!page || !shellRouteActive)
            return -1
        lifecycleState.addHostOverlayForwardCount += 1
        return page.requestAddHostOverlay()
    }
    function dismissAddHostOverlay() {
        if (!addHostOverlayActive)
            return false
        page.dismissOverlay(page.baseOverlay.operationId)
        return true
    }
    function submitAddHost(address) {
        if (!addHostOverlayActive)
            return false
        page.submitAddHost(address)
        return true
    }
    // Compatibility-only test seam; production add completion arrives through
    // ComputerManager and is owned by HostPageContext.
    function addComplete(attemptId, success, blocked) { context.completeAddHost(attemptId, success, blocked) }
    function deliverSurfaceActivation() {
        if (!lifecycleState.routeActive || lifecycleState.surfaceActivated
                || !legacySurface || !legacySurface.routeActivated)
            return
        legacySurface.routeActivated()
        lifecycleState.surfaceActivationForwardCount += 1
        lifecycleState.surfaceActivated = true
    }
    function synchronizeRouteLifecycle() {
        if (shellRouteActive === lifecycleState.routeActive) {
            if (shellRouteActive)
                deliverSurfaceActivation()
            return
        }

        lifecycleState.routeActive = shellRouteActive
        if (lifecycleState.routeActive) {
            lifecycleState.contextActivationTransitionCount += 1
            lifecycleState.surfaceActivated = false
            deliverSurfaceActivation()
        } else {
            lifecycleState.contextDeactivationTransitionCount += 1
            lifecycleState.surfaceActivated = false
            lifecycleState.surfaceDeactivationTransitionCount += 1
        }
    }

    Component {
        id: productionContextComponent
        HostPageContext {
            id: productionContext
            objectName: "starlight.hosts.context"
            routeActive: root.shellRouteActive
            sessionWarningsByHost: typeof window !== "undefined" && window.lastSessionWarningsByHost !== undefined
                                   ? window.lastSessionWarningsByHost : ({})
            hostSource: ComputerModel {
                objectName: "production-host-source"
                Component.onCompleted: initialize(productionContext.computerManager)
            }
        }
    }
    Component {
        id: injectedContextComponent
        HostPageContext {
            objectName: "starlight.hosts.context"
            hostSource: root.hostSource
            routeActive: root.shellRouteActive
            sessionWarningsByHost: typeof window !== "undefined" && window.lastSessionWarningsByHost !== undefined
                                   ? window.lastSessionWarningsByHost : ({})
        }
    }
    Loader {
        id: contextLoader
        sourceComponent: root.hostSource ? injectedContextComponent : productionContextComponent
    }

    Connections {
        target: context
        function onWindowEffectRequested(effect) {
            root.windowEffectRequested(effect)
        }
    }

    Connections {
        target: page
        function onBaseOverlayChanged() {
            if (root.shellRouteActive && root.addHostOverlayActive
                    && !page.baseOverlay.payload.dispatched)
                root.addHostOverlayActivated()
        }
    }

    // The adaptive shell composes the platform-native painted surface over
    // the same route-owned facade; every other entry keeps the legacy page.
    // The GNOME surface draws its own CSD headerbar; main.qml only adds
    // fallback window chrome for stack items that do not.
    readonly property bool providesWindowChrome: nativeGnomeSurface
    readonly property bool nativeGnomeSurface:
        shellRouteId === "adaptive-host"
        && typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true
    readonly property bool nativePlasmaSurface:
        shellRouteId === "adaptive-host"
        && typeof plasmaDesktopAdapterActive !== "undefined"
        && plasmaDesktopAdapterActive === true
    // Hyprland and other desktops without a native adapter get the Adwaita-derived
    // surface, without window controls: the compositor owns the frame there.
    readonly property bool nativeGenericSurface:
        shellRouteId === "adaptive-host" && !nativeGnomeSurface && !nativePlasmaSurface

    Component {
        id: legacySurfaceComponent
        Legacy.PcView {
            shellRouteId: root.shellRouteId
            page: root.page
        }
    }
    Component {
        id: gnomeSurfaceComponent
        Gnome.GnomeHostsPage { page: root.page }
    }
    // By URL: only this file needs Kirigami, and a build or runtime without it
    // must still load the route's other surfaces.
    Loader {
        id: legacySurfaceLoader
        anchors.fill: parent
        Component.onCompleted: {
            if (root.page === null)
                return
            if (root.nativePlasmaSurface)
                setSource(Qt.resolvedUrl("plasma/PlasmaHostsPage.qml"), { page: root.page })
            else
                sourceComponent = root.nativeGnomeSurface || root.nativeGenericSurface
                                  ? gnomeSurfaceComponent : legacySurfaceComponent
        }
        onLoaded: {
            root.deliverSurfaceActivation()
        }
    }

    onShellRouteActiveChanged: synchronizeRouteLifecycle()

    Component.onCompleted: synchronizeRouteLifecycle()

    Component.onDestruction: {
        routeDestroyed()
        if (typeof window !== "undefined" && window.forgetConsumedWindowEffects)
            window.forgetConsumedWindowEffects(root)
    }
}
