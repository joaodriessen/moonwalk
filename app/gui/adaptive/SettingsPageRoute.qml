import QtQuick 2.15
import QtQuick.Controls 2.15

import ".." as Legacy
import "gnome" as Gnome

// Non-painted ownership bridge for the legacy global-settings surface.
Item {
    id: root

    property string shellRouteId:
        typeof window !== "undefined" && window.adaptiveShellActive === true
        ? "adaptive-settings" : "legacy-settings"
    readonly property string shellRouteKind: "settings"
    // Adwaita presents preferences as a dialog over the hosts page, outside the
    // StackView; everything else about this route is the same.
    property bool hostedInDialog: false
    property bool dialogActive: false
    readonly property bool shellRouteActive: hostedInDialog ? dialogActive
                                                            : StackView.status === StackView.Active
    readonly property var context: settingsContext
    readonly property var page: settingsContext.facade
    readonly property var legacySurface: legacySurfaceLoader.item
    // The loader and StackView can become ready in either order. These guards
    // make one context and one surface activation delivery per route entry,
    // while exposing only read-only observations to the route tests.
    readonly property int contextActivationForwardCount:
        lifecycleState.contextActivationForwardCount
    readonly property int contextDeactivationForwardCount:
        lifecycleState.contextDeactivationForwardCount
    readonly property int surfaceActivationForwardCount:
        lifecycleState.surfaceActivationForwardCount
    readonly property int surfaceDeactivationTransitionCount:
        lifecycleState.surfaceDeactivationTransitionCount

    signal routeDestroyed()
    signal windowEffectRequested(var effect)

    QtObject {
        id: lifecycleState

        property int contextActivationForwardCount: 0
        property int contextDeactivationForwardCount: 0
        property int surfaceActivationForwardCount: 0
        property int surfaceDeactivationTransitionCount: 0
        property bool routeActive: false
        property bool surfaceActivated: false
    }

    SettingsPageContext {
        id: settingsContext
        objectName: "starlight.settings.context"
        pollingActive: typeof window !== "undefined" ? window.pollingActive : false
    }

    function deliverSurfaceActivation() {
        if (!lifecycleState.routeActive || lifecycleState.surfaceActivated
                || !legacySurface || !legacySurface.routeActivated)
            return
        legacySurface.routeActivated(settingsContext.shouldFocusInitialControl())
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
            lifecycleState.contextActivationForwardCount += 1
            lifecycleState.surfaceActivated = false
            settingsContext.routeActivated()
            deliverSurfaceActivation()
        } else {
            lifecycleState.contextDeactivationForwardCount += 1
            lifecycleState.surfaceActivated = false
            lifecycleState.surfaceDeactivationTransitionCount += 1
            settingsContext.routeDeactivating()
        }
    }

    onShellRouteActiveChanged: synchronizeRouteLifecycle()

    Component.onCompleted: synchronizeRouteLifecycle()

    Connections {
        target: settingsContext
        function onWindowEffectRequested(effect) {
            root.windowEffectRequested(effect)
        }
    }

    // The adaptive shell composes the platform-native painted surface over
    // the same route-owned facade; every other entry keeps the legacy page.
    // The GNOME surface draws its own CSD headerbar; main.qml only adds
    // fallback window chrome for stack items that do not.
    readonly property bool providesWindowChrome: nativeGnomeSurface
    readonly property bool nativeGnomeSurface:
        shellRouteId === "adaptive-settings"
        && typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true
    readonly property bool nativePlasmaSurface:
        shellRouteId === "adaptive-settings"
        && typeof plasmaDesktopAdapterActive !== "undefined"
        && plasmaDesktopAdapterActive === true
    // Hyprland and other desktops without a native adapter get the Adwaita-derived
    // surface, without window controls: the compositor owns the frame there.
    readonly property bool nativeGenericSurface:
        shellRouteId === "adaptive-settings" && !nativeGnomeSurface && !nativePlasmaSurface

    Component {
        id: legacySurfaceComponent
        Legacy.SettingsView {
            shellRouteId: root.shellRouteId
            page: settingsContext.facade
        }
    }
    Component {
        id: gnomeSurfaceComponent
        Gnome.GnomeSettingsPage { page: settingsContext.facade }
    }
    // By URL: only this file needs Kirigami, and a build or runtime without it
    // must still load the route's other surfaces.
    Loader {
        id: legacySurfaceLoader
        anchors.fill: parent
        Component.onCompleted: {
            if (root.nativePlasmaSurface)
                setSource(Qt.resolvedUrl("plasma/PlasmaSettingsPage.qml"), { page: settingsContext.facade })
            else
                sourceComponent = root.nativeGnomeSurface || root.nativeGenericSurface
                                  ? gnomeSurfaceComponent : legacySurfaceComponent
        }
        onLoaded: {
            root.deliverSurfaceActivation()
        }
    }

    Component.onDestruction: {
        routeDestroyed()
        if (typeof window !== "undefined" && window.forgetConsumedWindowEffects)
            window.forgetConsumedWindowEffects(root)
    }
}
