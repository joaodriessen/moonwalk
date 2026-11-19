import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import Qt.labs.settings 1.0 as LegacySettings

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

import Starlight 1.0
import Starlight.Application 1.0
import "starlight"
import "adaptive/gnome" as AdaptiveGnome
// The CSD chrome is deliberately effects-free: the Qt 6.4 Ubuntu floor
// ships NO graphical-effects module (neither QtQuick.Effects nor
// Qt5Compat.GraphicalEffects), so the window shadow uses a 9-patch
// BorderImage and rounded corners use plain rectangles.

ApplicationWindow {
    id: window

    title: "Moonwalk"

    // GNOME client-side decoration: the adaptive shell draws its own
    // headerbar, window controls and rounded corners, so the window goes
    // frameless and transparent there. Every other configuration keeps the
    // stock decorated window byte-for-byte.
    readonly property bool gnomeCsdActive:
        typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true
    readonly property bool plasmaAdaptiveActive:
        typeof plasmaDesktopAdapterActive !== "undefined"
        && plasmaDesktopAdapterActive === true
        && typeof adaptiveShellRequested !== "undefined"
        && adaptiveShellRequested === true
    // A floating (not maximized/fullscreen) CSD window gets the shadow
    // margin, rounded corners and resize grips. Maximized and fullscreen
    // windows are flush and square, exactly like GTK.
    // NOTE: a "is the window tiled/snapped?" heuristic based on the window
    // filling the available height was tried and REVERTED — a tall window the
    // user simply sized that way is indistinguishable from a snap, so it
    // stripped the rounded corners in the common case. xdg-shell tiled states
    // are not exposed to QML; leave snapped windows slightly imperfect rather
    // than break ordinary ones.
    readonly property bool floatingChromeActive:
        gnomeCsdActive
        && visibility !== Window.Maximized
        && visibility !== Window.FullScreen
    // Transparent margin around the content: it holds the client-drawn
    // shadow and doubles as the resize grab zone (the GTK CSD model).
    // 19px is the measured reach of libadwaita's active window.csd shadow in
    // a native GTK 4.22.4 render (scripts/design/gnome-shadows.py); a
    // smaller margin clips its outer rows.
    readonly property int csdMargin: floatingChromeActive ? 19 : 0
    readonly property int csdRadius: floatingChromeActive ? 15 : 0
    // window.csd vs window.csd:backdrop. Both nine-patches are generated on
    // one shared canvas, because libadwaita keeps the shadow extents equal
    // across focus changes specifically so the window does not jump.
    readonly property url csdShadowSource:
        active ? "qrc:/res/window_shadow.png"
               : "qrc:/res/window_shadow_backdrop.png"
    // Full-window GNOME transitions share the page's live Appearance-backed
    // ground. Other shells retain the legacy Theme palette unchanged.
    readonly property AdaptiveGnome.GnomeTokens gnomeTokens:
        AdaptiveGnome.GnomeTokens {}
    readonly property color transitionGround:
        plasmaAdaptiveActive ? palette.window : gnomeTokens.window
    // The Adwaita surface serves GNOME and every desktop without its own adapter.
    readonly property bool adwaitaSurfaceActive: adaptiveShellActive && !plasmaAdaptiveActive
    flags: gnomeCsdActive ? Qt.Window | Qt.FramelessWindowHint : Qt.Window
    color: gnomeCsdActive ? "transparent"
                          : (plasmaAdaptiveActive ? palette.window : gnomeTokens.window)

    SystemPalette { id: nativeApplicationPalette; colorGroup: SystemPalette.Active }
    readonly property bool adaptiveShellRequestedForPalette:
        typeof adaptiveShellRequested !== "undefined" && adaptiveShellRequested === true

    Loader {
        id: materialPaletteBridgeLoader
        active: typeof materialStyleActive !== "undefined"
                && materialStyleActive === true
        source: "starlight/MaterialPaletteBridge.qml"
        onLoaded: item.target = window
    }

    // Plain Controls roots can follow whichever single style was selected
    // before the QML engine exists. Semantic colors remain app-owned and
    // live, independent of that style's private attached-property namespace.
    palette.window: plasmaAdaptiveActive ? nativeApplicationPalette.window
                    : (adaptiveShellRequestedForPalette ? gnomeTokens.window : Theme.ground)
    palette.windowText: plasmaAdaptiveActive ? nativeApplicationPalette.windowText
                        : (adaptiveShellRequestedForPalette ? gnomeTokens.text : Theme.label)
    palette.text: plasmaAdaptiveActive ? nativeApplicationPalette.text
                  : (adaptiveShellRequestedForPalette ? gnomeTokens.text : Theme.label)
    palette.buttonText: plasmaAdaptiveActive ? nativeApplicationPalette.buttonText
                        : (adaptiveShellRequestedForPalette ? gnomeTokens.text : Theme.label)
    palette.highlight: plasmaAdaptiveActive ? nativeApplicationPalette.highlight
                       : (adaptiveShellRequestedForPalette ? gnomeTokens.accent : Theme.accent)
    palette.highlightedText: plasmaAdaptiveActive ? nativeApplicationPalette.highlightedText
                             : (adaptiveShellRequestedForPalette ? gnomeTokens.accentInk : Theme.accentInk)
    palette.toolTipBase: plasmaAdaptiveActive ? nativeApplicationPalette.toolTipBase : Theme.darkChip
    palette.toolTipText: plasmaAdaptiveActive ? nativeApplicationPalette.toolTipText : Theme.accentInk

    // The window owns its ground in BOTH palettes. Without this it inherits
    // the Material style's forced dark #303030, which every page then has to
    // paint over (PcView's full-bleed Rectangle exists for exactly that) and
    // which bleeds through the StackView push/pop fades in light mode.
    background: Rectangle {
        color: window.gnomeCsdActive ? "transparent" : window.palette.window
    }

    // Client-drawn window shadow. On Wayland a frameless surface gets no
    // compositor shadow, so — like every GTK app — the app draws its own into
    // the transparent margin.
    //
    // Effects-free nine-patch (the Qt 6.4 floor ships no graphical-effects
    // modules). Both assets are cropped by scripts/design/gnome-shadows.py
    // out of a native GTK 4.22.4 render of libadwaita's own declarations, so
    // all three box-shadow layers — including the 0 0 0 1px black 5% ring,
    // which is a shadow layer and not a border — are already in the pixels.
    // The source defines one shadow, not a light one and a dark one, so there
    // is nothing here to condition on the colour scheme.
    BorderImage {
        objectName: "window.csd.shadow"
        visible: window.floatingChromeActive
        anchors.fill: csdBase
        anchors.margins: -window.csdMargin
        source: window.csdShadowSource
        border { left: 42; right: 42; top: 42; bottom: 42 }
        z: -2
    }

    // Rounded ground beneath the content, keeping the corners solid through
    // page transitions. The GNOME pages round their own outer corners to
    // match (plain rectangles, no masking). The edge itself is drawn by the
    // shadow below and the outline above, exactly as in the stylesheet.
    Rectangle {
        id: csdBase
        anchors.fill: parent
        anchors.margins: window.csdMargin
        z: -1
        radius: window.csdRadius
        color: window.transitionGround
        visible: window.gnomeCsdActive
    }

    // outline: 1px solid RGB(255 255 255/7%); outline-offset: -1px
    // Drawn on top so the inset ring reads over page content that fills to
    // the corners. This is the only ring not carried by the shadow asset.
    Rectangle {
        objectName: "window.csd.outline"
        anchors.fill: csdBase
        z: 8000
        radius: window.csdRadius
        color: "transparent"
        visible: window.floatingChromeActive
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.07)
    }

    // Fallback window chrome for stack items that do not draw their own
    // CSD headerbar (stream segues, legacy views): window controls in the
    // configured corners plus a drag strip, hidden while fullscreen.
    Loader {
        active: window.gnomeCsdActive
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: window.csdMargin
        anchors.leftMargin: window.csdMargin
        anchors.rightMargin: window.csdMargin
        height: window.gnomeTokens.headerbarHeight
        z: 9000

        sourceComponent: Item {
            visible: window.visibility !== Window.FullScreen
                     && !(stackView.currentItem
                          && stackView.currentItem.providesWindowChrome === true)

            DragHandler {
                target: null
                onActiveChanged: if (active) window.startSystemMove()
            }
            TapHandler {
                onDoubleTapped: window.visibility === Window.Maximized
                                ? window.showNormal() : window.showMaximized()
            }
            AdaptiveGnome.GnomeWindowControls {
                side: "left"
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            AdaptiveGnome.GnomeWindowControls {
                side: "right"
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Frameless windows must provide their own interactive resize. The grab
    // strips fill the shadow margin, so the whole soft edge resizes like a
    // GTK window. Only while floating.
    Loader {
        active: window.floatingChromeActive
        anchors.fill: parent
        z: 10000

        sourceComponent: Item {
            readonly property int grip: Math.max(6, window.csdMargin)

            MouseArea {
                anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
                width: grip
                cursorShape: Qt.SizeHorCursor
                onPressed: window.startSystemResize(Qt.LeftEdge)
            }
            MouseArea {
                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                width: grip
                cursorShape: Qt.SizeHorCursor
                onPressed: window.startSystemResize(Qt.RightEdge)
            }
            MouseArea {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: grip
                cursorShape: Qt.SizeVerCursor
                onPressed: window.startSystemResize(Qt.TopEdge)
            }
            MouseArea {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: grip
                cursorShape: Qt.SizeVerCursor
                onPressed: window.startSystemResize(Qt.BottomEdge)
            }
            MouseArea {
                anchors { bottom: parent.bottom; right: parent.right }
                width: grip * 2; height: grip * 2
                cursorShape: Qt.SizeFDiagCursor
                onPressed: window.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
            }
            MouseArea {
                anchors { bottom: parent.bottom; left: parent.left }
                width: grip * 2; height: grip * 2
                cursorShape: Qt.SizeBDiagCursor
                onPressed: window.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
            }
            MouseArea {
                anchors { top: parent.top; right: parent.right }
                width: grip * 2; height: grip * 2
                cursorShape: Qt.SizeBDiagCursor
                onPressed: window.startSystemResize(Qt.TopEdge | Qt.RightEdge)
            }
            MouseArea {
                anchors { top: parent.top; left: parent.left }
                width: grip * 2; height: grip * 2
                cursorShape: Qt.SizeFDiagCursor
                onPressed: window.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
            }
        }
    }

    readonly property var preferencesPopup: preferencesDialog
    AdaptiveGnome.GnomePreferencesDialog {
        id: preferencesDialog
        objectName: "starlight.preferences-dialog"
        outerMargin: window.csdMargin
        onEffectRequested: function(effect, route) { window.consumeWindowEffect(effect, route) }
    }

    PollingLifecycle {
        id: pollingLifecycle
    }

    property alias pollingActive: pollingLifecycle.pollingActive

    // Set by SettingsView to force the back operation to pop all
    // pages except the initial view. This is required when doing
    // a retranslate() because AppView breaks for some reason.
    property bool clearOnBack: false

    property bool adaptiveShellActive: false
    property bool pendingAddHostRequest: false
    property var pendingAddHostSourceRoute: null
    property var pendingAddHostTargetRoute: null
    // Record effect IDs before navigating so duplicate deliveries cannot own
    // the same opaque Session twice.
    // At most one monotonic acknowledgement watermark per live route. This
    // keeps duplicate detection bounded while preserving the effect pair.
    property var consumedWindowEffects: []
    property var pinnedQuitRoute: null
    property var pinnedQuitContext: null
    // Characterization-only construction seam. Production leaves this null,
    // preserving the route context's inline ComputerModel source.
    property var hostSourceOverride: null
    property url adaptiveAdapterSource:
        Qt.resolvedUrl("adaptive/PlatformAdapter.qml")
    property var platformAdapter: null
    readonly property url settingsRoute:
        platformAdapter !== null
            ? platformAdapter.settingsRoute
            : Qt.resolvedUrl("adaptive/SettingsPageRoute.qml")

    function legacyStartupRoute(warning) {
        adaptiveShellActive = false
        if (warning) {
            console.warn(warning)
        }
        console.info("Starlight shell route: legacy")
        return initialView
    }

    function startupRoute() {
        if (!adaptiveShellRequested) {
            return legacyStartupRoute("")
        }

        var adapterComponent = Qt.createComponent(
                    adaptiveAdapterSource, Component.PreferSynchronous)
        if (adapterComponent.status !== Component.Ready) {
            return legacyStartupRoute(
                        "Adaptive shell adapter unavailable; using legacy shell")
        }

        var adapter = adapterComponent.createObject(window)
        if (adapter === null) {
            return legacyStartupRoute(
                        "Adaptive shell adapter unavailable; using legacy shell")
        }

        var hostComponent = Qt.createComponent(
                    adapter.hostRoute, Component.PreferSynchronous)
        var settingsComponent = Qt.createComponent(
                    adapter.settingsRoute, Component.PreferSynchronous)
        if (hostComponent.status !== Component.Ready
                || settingsComponent.status !== Component.Ready) {
            adapter.destroy()
            return legacyStartupRoute(
                        "Adaptive shell route unavailable; using legacy shell")
        }

        platformAdapter = adapter
        adaptiveShellActive = true
        console.info("Starlight shell route: adaptive")
        return adapter.hostRoute
    }
    // Compact default sized for a launcher, not a media window (was 1280x600,
    // then 836x557). Landscape, not taller-than-wide -- a fresh profile (no
    // launcherWindow Settings group yet, see below) opens at exactly this
    // size, so this literal IS the real first-launch size, not just a
    // fallback. Content below (PcView.qml's contentFlick) is a Flickable,
    // so it scrolls within whatever height the window has rather than
    // forcing the window taller to fit -- this height is never overridden
    // by content implicit-sizing.
    // Resolved (saved-or-default) from the very first frame. The property,
    // the StackView implicit pin, and the backstop pass must all agree on the
    // SAME number — when this still said the default while the map size was
    // the saved size, the window visibly resized itself at launch.
    width: resolvedWidth()
    height: resolvedHeight()
    // Small enough for a tiled column or a narrow display: below 680 points every
    // surface collapses to one pane at a time.
    // The CSD margin is part of the window but not of the usable content, so
    // the floor grows by the margin on both axes when the chrome is active.
    minimumWidth: 360 + (gnomeCsdActive ? 2 * csdMargin : 0)
    minimumHeight: 400 + (gnomeCsdActive ? 2 * csdMargin : 0)

    // Remember the size the user picks between launches (width/height only —
    // deliberately not x/y, to avoid restoring off-screen on a monitor change).
    //
    // We deliberately do NOT use `property alias width: window.width`. On
    // Wayland, Qt sizes the xdg-toplevel to the content's implicit size on the
    // first map and ignores the window's explicit width/height until they are
    // re-asserted *after* show() (measured: the launcher opened at ~800x828,
    // its content size, instead of the saved size). A two-way alias makes that
    // worse — the content-driven size clobbers window.width, which the alias
    // then persists, corrupting the saved size. Instead we hold the saved size
    // in plain properties, re-assert it right after show(), and only persist
    // genuine user resizes (guarded by sizeSettled).
    // Saved under new keys (savedWidth/savedHeight). The pre-1.0 alias-based
    // keys (width/height) are intentionally NOT migrated: QML's Settings type
    // exposes no value()/setValue(), so a legacy key can only be read by
    // declaring it as a property — which then persists its own default and
    // clobbers the stored value. Not worth that risk for a one-time size that
    // the user re-establishes with a single resize. -1 sentinel = never saved.
    LegacySettings.Settings {
        id: launcherWindowSettings
        category: "launcherWindow"
        property int savedWidth: -1
        property int savedHeight: -1
    }

    property bool sizeSettled: false

    // A remembered size wins. The first-run size is modest and always fits the
    // display, whatever its size or scale.
    function firstRunSize(preferred, available) {
        return available > 0 ? Math.min(preferred, Math.round(available * 0.9)) : preferred
    }
    function resolvedWidth() {
        return launcherWindowSettings.savedWidth > 0 ? launcherWindowSettings.savedWidth
                                                     : firstRunSize(Theme.windowDefaultWidth, Screen.desktopAvailableWidth)
    }
    function resolvedHeight() {
        return launcherWindowSettings.savedHeight > 0 ? launcherWindowSettings.savedHeight
                                                      : firstRunSize(Theme.windowDefaultHeight, Screen.desktopAvailableHeight)
    }

    // The implicit size pinned on the StackView below is what Wayland maps first,
    // so nothing re-asserts it: a tiling compositor's configure must win, or the
    // content stays in a corner of its tile. This delay only keeps the mapping
    // resize out of the remembered size.
    Timer {
        id: sizeReassertTimer
        interval: 150
        onTriggered: sizeSettled = true
    }

    // Persist the size 400ms after the LAST change instead of on every
    // resize event — per-event QSettings writes during an interactive drag
    // were the source of the resize jitter.
    Timer {
        id: sizePersistTimer
        interval: 400
        onTriggered: {
            // A restart queued just before maximizing must not land after it.
            if (!window.sizeIsPersistable)
                return
            launcherWindowSettings.savedWidth = window.width
            launcherWindowSettings.savedHeight = window.height
        }
    }

    // Persist only the FLOATING size. Without this guard, maximizing (or a
    // fullscreen stream) overwrote the remembered size, so the next launch
    // opened at screen size instead of the size the user chose. Verified:
    // pre-fix a maximize+quit wrote savedWidth/Height = 1500x1050.
    // Filling the display is what a tile or a snap looks like; a remembered size
    // should be one a floating window can use.
    readonly property bool sizeIsPersistable:
        visibility !== Window.Maximized
        && visibility !== Window.FullScreen
        && visibility !== Window.Minimized
        && visibility !== Window.Hidden
        && (width < Screen.desktopAvailableWidth * 0.98
            || height < Screen.desktopAvailableHeight * 0.95)
    onWidthChanged: if (sizeSettled && sizeIsPersistable) sizePersistTimer.restart()
    onHeightChanged: if (sizeSettled && sizeIsPersistable) sizePersistTimer.restart()

    // Session advisories remain available in host details after the launcher
    // returns. This is deliberately in-memory and scoped to the host UUID.
    property var lastSessionWarningsByHost: ({})

    function retainSessionWarnings(hostUuid, warnings) {
        if (!hostUuid)
            return
        var retained = Object.assign({}, lastSessionWarningsByHost)
        retained[hostUuid] = warnings.slice()
        lastSessionWarningsByHost = retained
    }

    // Lightweight passive toast for transient feedback (e.g. Wake PC results).
    // The previous ToolTip.show() approach was effectively invisible — a
    // ToolTip attaches to the item it's shown from, and when called from a
    // Connections/function context it rendered nothing the user could see.
    // An optional action sits beside the words, as on an AdwToast or a Kirigami
    // passive notification, and stays reachable a little longer.
    function showToast(msg, actionText, action) {
        toastLabel.text = msg
        toast.actionText = actionText || ""
        toast.action = action || null
        toast.shown = true
        toastTimer.interval = toast.actionText.length > 0 ? 6000 : 3200
        toastTimer.restart()
    }

    function copyDiagnosticSummary() {
        var copied = typeof supportInfo !== "undefined" && supportInfo.copySummary()
        showToast(copied ? qsTr("Diagnostic summary copied. Review before sharing.")
                        : qsTr("Could not copy diagnostic summary"))
        return copied
    }

    function effectAlreadyConsumed(effect, route) {
        for (var index = 0; index < consumedWindowEffects.length; ++index) {
            var record = consumedWindowEffects[index]
            if (record.route !== route)
                continue
            if (record.routeGeneration === effect.routeGeneration
                    && effect.effectId <= record.effectId)
                return true
            record.routeGeneration = effect.routeGeneration
            record.effectId = effect.effectId
            return false
        }
        consumedWindowEffects = consumedWindowEffects.concat([({
            route: route,
            routeGeneration: effect.routeGeneration,
            effectId: effect.effectId
        })])
        return false
    }

    function forgetConsumedWindowEffects(route) {
        var retained = []
        for (var index = 0; index < consumedWindowEffects.length; ++index) {
            if (consumedWindowEffects[index].route !== route)
                retained.push(consumedWindowEffects[index])
        }
        consumedWindowEffects = retained
    }

    function osIconName(os) {
        return os === "macos" ? "os_macos" : os === "windows" ? "os_windows"
                              : os === "linux" ? "os_linux" : "monitor"
    }
    function createStreamSegue(session, resume, hostUuid, appName) {
        if (!session)
            return null
        // Characterization can supply a Component through the engine context
        // only; production exposes no mutable component/source property.
        var testComponent = typeof streamSegueComponentOverride !== "undefined"
                ? streamSegueComponentOverride : null
        var component = testComponent !== null
                ? testComponent : Qt.createComponent("StreamSegue.qml")
        if (component.status !== Component.Ready) {
            console.error(component.errorString())
            return null
        }
        // The host page that started this knows what the host looks like.
        var startingRoute = stackView.currentItem
        var startingHost = startingRoute && startingRoute.context && startingRoute.context.selectedHostSnapshot
                ? startingRoute.context.selectedHostSnapshot : ({})
        var segue = component.createObject(stackView, {
                                             "objectName": "starlight.stream-segue",
                                             "hostSeed": hostUuid || startingHost.uuid || "",
                                             "hostIcon": window.osIconName(startingHost.hostOs),
                                             "appName": appName || qsTr("Desktop"),
                                             "session": session,
                                             "isResume": resume === true,
                                             "retainLaunchWarningsFn": function(warnings) {
                                                 window.retainSessionWarnings(hostUuid, warnings)
                                             },
                                             "quitSegueRequestedFn": function(appName) {
                                                 window.replaceStreamWithQuit(segue, appName)
                                             }
                                         })
        if (segue === null)
            console.error(component.errorString())
        return segue
    }

    function createQuitSegue(appName, quitRunningAppFn, nextSession, route, hostUuid) {
        // Characterization can supply a factory through the engine context
        // only; production exposes no mutable component/source property.
        var testComponent = typeof quitSegueComponentOverride !== "undefined"
                ? quitSegueComponentOverride : null
        var component = testComponent !== null
                ? testComponent : Qt.createComponent("QuitSegue.qml")
        if (component.status !== Component.Ready) {
            console.error(component.errorString())
            return null
        }
        return component.createObject(stackView, {
                                          "objectName": "starlight.quit-segue",
                                          "appName": appName,
                                          "quitRunningAppFn": quitRunningAppFn,
                                          "nextAppName": qsTr("Desktop"),
                                          "nextSession": nextSession,
                                          "streamHandoffFn": function(session, nextAppName) {
                                              return window.handoffPinnedQuit(route, session, nextAppName, hostUuid)
                                          },
                                          "terminalRecoveryFn": function() {
                                              window.recoverPinnedQuit(route)
                                          }
                                      })
    }

    function pinQuitRoute(route, quitSegue) {
        if (!route || !route.context || !quitSegue || pinnedQuitRoute !== null)
            return false
        pinnedQuitRoute = route
        pinnedQuitContext = route.context
        if (pinnedQuitContext.setQuitRoutePinned)
            pinnedQuitContext.setQuitRoutePinned(true)
        if (!stackView.beginQuitLease(route, quitSegue)) {
            if (pinnedQuitContext.setQuitRoutePinned)
                pinnedQuitContext.setQuitRoutePinned(false)
            pinnedQuitRoute = null
            pinnedQuitContext = null
            return false
        }
        return true
    }

    function unpinQuitRoute(route) {
        if (pinnedQuitRoute !== route)
            return
        if (pinnedQuitContext && pinnedQuitContext.setQuitRoutePinned)
            pinnedQuitContext.setQuitRoutePinned(false)
        pinnedQuitRoute = null
        pinnedQuitContext = null
        stackView.unpinOrigin(route)
    }

    function recoverPinnedQuit(route) {
        unpinQuitRoute(route)
    }

    function handoffPinnedQuit(route, session, nextAppName, hostUuid) {
        if (pinnedQuitRoute !== route)
            return false
        var streamSegue = createStreamSegue(session, false, hostUuid)
        if (!streamSegue) {
            if (route && route.context && route.context.presentError)
                route.context.presentError(qsTr("Unable to create stream session"), "")
            return false
        }
        unpinQuitRoute(route)
        stackView.replace(streamSegue)
        return true
    }

    function replaceStreamWithQuit(streamSegue, appName) {
        if (stackView.currentItem !== streamSegue)
            return false
        var quitSegue = createQuitSegue(appName, null, null, null)
        if (!quitSegue)
            return false
        stackView.replace(quitSegue, StackView.Immediate)
        return true
    }

    function consumeWindowEffect(effect, route) {
        if (!effect || !route || !route.context || !effect.payload
                || typeof effect.effectId !== "number"
                || typeof effect.routeGeneration !== "number")
            return false
        var context = route.context
        if (context.routeGeneration !== effect.routeGeneration || !context.routeActive)
            return false
        // Effect IDs are context-local, monotonically increasing, and only
        // valid inside their route generation. Record the pair before action;
        // a duplicate then cannot acquire its opaque Session or navigate.
        if (effectAlreadyConsumed(effect, route))
            return false

        var acknowledged = false
        if (effect.kind === "toast") {
            showToast(effect.payload.text || "", effect.payload.actionText,
                      effect.payload.actionText ? function() { context.toastActionTriggered(effect.payload) } : null)
            acknowledged = context.acknowledgeWindowEffect(
                        effect.effectId, effect.routeGeneration, "consumed")
        } else if (effect.kind === "open-settings") {
            acknowledged = context.acknowledgeWindowEffect(
                        effect.effectId, effect.routeGeneration, "consumed")
            if (acknowledged)
                navigateTo(settingsRoute, "settings")
        } else if (effect.kind === "back") {
            if (effect.payload.clearToRoot)
                clearOnBack = true
            acknowledged = context.acknowledgeWindowEffect(
                        effect.effectId, effect.routeGeneration, "consumed")
            if (acknowledged) {
                if (route === preferencesDialog.settingsRoute) {
                    clearOnBack = false
                    preferencesDialog.close()
                } else {
                    goBack()
                }
            }
        } else if (effect.kind === "launch") {
            var streamSegue = effect.payload.session
                    ? createStreamSegue(effect.payload.session, effect.payload.resume,
                                         effect.payload.hostUuid, effect.payload.appName) : null
            if (streamSegue) {
                acknowledged = context.acknowledgeWindowEffect(
                            effect.effectId, effect.routeGeneration, "consumed")
                if (acknowledged)
                    stackView.push(streamSegue)
                else
                    streamSegue.destroy()
            }
        } else if (effect.kind === "quit-then-launch") {
            if (effect.payload.session && stackView.canAcceptQuitRoute(route)) {
                var quitSegue = createQuitSegue(effect.payload.runningAppLabel,
                                                 function() {
                                                     return context.quitRunningAppForOperation(
                                                                 effect.payload.operationId)
                                                 },
                                                 effect.payload.session, route, effect.payload.hostUuid)
                if (quitSegue && stackView.canBeginQuitLease(route, quitSegue)) {
                    acknowledged = context.acknowledgeWindowEffect(
                        effect.effectId, effect.routeGeneration, "consumed")
                    if (acknowledged) {
                        if (!pinQuitRoute(route, quitSegue)) {
                            if (context.releaseSession)
                                context.releaseSession(effect.payload.session)
                            if (typeof quitSegue.destroy === "function")
                                quitSegue.destroy()
                            acknowledged = false
                        }
                    } else {
                        quitSegue.destroy()
                    }
                } else if (quitSegue) {
                    quitSegue.destroy()
                }
            }
        }

        if (!acknowledged) {
            // A quit operation is created with its effect but is consumed only
            // when QuitSegue dispatches the backend request. Every failed
            // pre- or post-acknowledgement setup path must remove that exact
            // context-owned operation before the effect releases its Session.
            if (effect.kind === "quit-then-launch" && context.cancelQuitOperation)
                context.cancelQuitOperation(effect.payload.operationId)
            context.acknowledgeWindowEffect(effect.effectId,
                                            effect.routeGeneration, "failed")
        }
        return acknowledged
    }

    readonly property Item toastItem: toast
    // AdwToast: a dark rounded bubble over everything, dialogs included.
    Rectangle {
        id: toast
        objectName: "starlight.toast"
        z: 100000
        // Above dialogs, so on the overlay, which exists only once the window does.
        Component.onCompleted: parent = Overlay.overlay
        x: Math.round((window.width - width) / 2)
        property bool shown: false
        property string actionText: ""
        property var action: null
        readonly property int restingY: window.height - height - 24 - window.csdMargin
        y: shown ? restingY : restingY + 16
        radius: plasmaAdaptiveActive ? 6 : height / 2 > 24 ? gnomeTokens.radiusPopup : height / 2
        color: plasmaAdaptiveActive ? window.palette.toolTipBase : Qt.rgba(0.11, 0.11, 0.13, 0.95)
        readonly property real maximumWidth: Math.max(1, window.width - 2 * window.csdMargin - 32)
        readonly property real actionSpace: toastAction.visible ? toastAction.implicitWidth + 12 : 0
        width: Math.min(toastLabel.implicitWidth + 40 + actionSpace, maximumWidth)
        height: Math.max(toastLabel.height, toastAction.visible ? toastAction.implicitHeight : 0) + 20
        opacity: shown ? 0.97 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.durationMedium } }
        Behavior on y { NumberAnimation { duration: Theme.durationMedium; easing.type: Easing.OutCubic } }

        Text {
            id: toastLabel
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 40 - parent.actionSpace
            color: plasmaAdaptiveActive ? window.palette.toolTipText : "#ffffff"
            font.pixelSize: plasmaAdaptiveActive ? Theme.sizeBody : gnomeTokens.bodySize
            wrapMode: Text.Wrap
            maximumLineCount: 8
            elide: Text.ElideRight
            horizontalAlignment: toastAction.visible ? Text.AlignLeft : Text.AlignHCenter
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
        // Its own ink: a style's flat button assumes the window behind it, not this bubble.
        AbstractButton {
            id: toastAction
            objectName: "starlight.toast.action"
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: toastActionLabel.implicitWidth + 24
            implicitHeight: toastActionLabel.implicitHeight + 12
            visible: toast.actionText.length > 0
            text: toast.actionText
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            background: Rectangle {
                radius: height / 2
                color: toastLabel.color
                opacity: toastAction.down ? 0.28 : toastAction.hovered || toastAction.visualFocus ? 0.20 : 0.12
            }
            contentItem: Text {
                id: toastActionLabel
                text: toastAction.text
                color: toastLabel.color
                font: toastLabel.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Accessible.role: Accessible.Button
            Accessible.name: text
            onClicked: {
                toast.shown = false
                if (toast.action) toast.action()
            }
        }

        Timer { id: toastTimer; interval: 3200; onTriggered: toast.shown = false }
    }

    // This function runs prior to creation of the initial StackView item
    function doEarlyInit() {
        // (The old Material.background = "#303030" override is gone: it
        // existed to give the DELETED app grid contrast against GFE box art,
        // and the window now owns its ground via the Theme-bound background
        // above — the forced dark ground was actively wrong in light mode.)
        SdlGamepadKeyNavigation.enable()
    }

    Component.onCompleted: {
        // Show the window according to the user's preferences
        if (SystemProperties.hasDesktopEnvironment) {
            if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) {
                window.showMaximized()
                sizeSettled = true
            }
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) {
                window.showFullScreen()
                sizeSettled = true
            }
            else {
                window.show()
                sizeReassertTimer.start()
            }
        } else {
            window.showFullScreen()
            sizeSettled = true
        }

        // Display any modal dialogs for configuration warnings
        if (runConfigChecks) {
            if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }

            // Hardware acceleration and unmapped gamepads are checked asynchronously
            SystemProperties.hasHardwareAccelerationChanged.connect(hasHardwareAccelerationChanged)
            SystemProperties.unmappedGamepadsChanged.connect(hasUnmappedGamepadsChanged)
            SystemProperties.startAsyncLoad()
        }
    }

    function hasHardwareAccelerationChanged() {
        if (!SystemProperties.hasHardwareAcceleration && StreamingPreferences.videoDecoderSelection !== StreamingPreferences.VDS_FORCE_SOFTWARE) {
            if (SystemProperties.isRunningXWayland) {
                xWaylandDialog.open()
            }
            else {
                noHwDecoderDialog.open()
            }
        }
    }

    function hasUnmappedGamepadsChanged() {
        if (SystemProperties.unmappedGamepads) {
            unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
            unmappedGamepadDialog.open()
        }
    }

    // It would be better to use TextMetrics here, but it always lays out
    // the text slightly more compactly than real Text does in ToolTip,
    // causing unexpected line breaks to be inserted.
    //
    // ALL ToolTip.toolTip.* attached assignments live on this (invisible)
    // Text because the attached type must hang off an Item — declaring them
    // on the ApplicationWindow itself logged "ToolTip attached property must
    // be attached to an object deriving from Item" at every launch.
    Text {
        id: tooltipTextLayoutHelper
        visible: false
        font: ToolTip.toolTip.font
        text: ToolTip.toolTip.text
        readonly property var configuredToolTip: ToolTip.toolTip

        // Cap the singleton tooltip's width so long text wraps instead of
        // extending forever.
        ToolTip.toolTip.contentWidth: Math.min(tooltipTextLayoutHelper.width, 400)

        // Theme the shared attached-ToolTip singleton to match the app's own
        // toast pill (showToast below) instead of the Material style's bubble —
        // the one popup surface that was still rendering in foreign chrome.
        ToolTip.toolTip.background: Rectangle {
            radius: Theme.radiusChip
            color: Theme.darkChip
        }
        ToolTip.toolTip.contentItem: Text {
            text: ToolTip.toolTip.text
            font: ToolTip.toolTip.font
            color: Theme.accentInk
            wrapMode: Text.Wrap
        }

        // Some non-Material styles lazily replace the shared ToolTip's
        // deferred background/content delegates the first time it opens.
        // Re-assert only the semantic surface values on that selected-style
        // implementation so the established tooltip scaffold does not drift.
        Binding {
            target: tooltipTextLayoutHelper.configuredToolTip.background
            property: "radius"
            value: Theme.radiusChip
            when: tooltipTextLayoutHelper.configuredToolTip.background !== null
        }
        Binding {
            target: tooltipTextLayoutHelper.configuredToolTip.background
            property: "color"
            value: Theme.darkChip
            when: tooltipTextLayoutHelper.configuredToolTip.background !== null
        }
        Binding {
            target: tooltipTextLayoutHelper.configuredToolTip.contentItem
            property: "color"
            value: Theme.accentInk
            when: tooltipTextLayoutHelper.configuredToolTip.contentItem !== null
                  && "color" in tooltipTextLayoutHelper.configuredToolTip
                      .contentItem
        }
    }

    // Stable read-only validation seam for the one real attached ToolTip
    // singleton. It lets offscreen tests compare the actual popup scaffold
    // without reproducing it in a fixture.
    readonly property var sharedToolTip: tooltipTextLayoutHelper.configuredToolTip

    function goBack() {
        if (pinnedQuitRoute !== null)
            return
        if (clearOnBack) {
            // Pop all items except the first one
            stackView.pop(null)
            clearOnBack = false
        }
        else {
            stackView.pop()
        }
    }

    function requestRouteBack() {
        var route = stackView.currentItem
        if (route && route.shellRouteKind === "settings" && route.page) {
            route.page.requestBack(false)
            return
        }
        goBack()
    }

    function clearPendingAddHostRequest() {
        pendingAddHostRequest = false
        pendingAddHostSourceRoute = null
        pendingAddHostTargetRoute = null
    }

    function openAddHostDialogForRoute(route) {
        if (!route || route.shellRouteKind !== "hosts"
                || !route.addHostOverlayActive
                || route.nativeGnomeSurface || route.nativePlasmaSurface
                || route.nativeGenericSurface)
            return
        Qt.callLater(function() {
            if (stackView.currentItem === route && route.shellRouteActive
                    && route.addHostOverlayActive && !addPcDialog.visible) {
                addPcDialog.open()
            }
        })
    }

    function activateAddHostOnRoute(route) {
        if (!route || route.shellRouteKind !== "hosts"
                || route.shellRouteActive !== true)
            return false
        var operationId = route.requestAddHostOverlay()
        if (operationId < 0)
            return false
        openAddHostDialogForRoute(route)
        return true
    }

    function fulfillPendingAddHostRequest() {
        if (!pendingAddHostRequest)
            return
        var route = stackView.currentItem
        if (route && route.shellRouteKind === "hosts") {
            pendingAddHostTargetRoute = route
            if (activateAddHostOnRoute(route))
                clearPendingAddHostRequest()
        } else if (!stackView.busy) {
            clearPendingAddHostRequest()
        }
    }

    PinnedStackView {
        id: stackView
        objectName: "starlight.shell.stack"
        anchors.fill: parent
        anchors.margins: window.csdMargin
        focus: true

        // The window size is authoritative; content scrolls within it. Without
        // an explicit implicit size here, ApplicationWindow derives its implicit
        // size from the current StackView item (PcView's tall per-host content,
        // ~800px), and on Wayland that implicit size overrode the explicit/
        // restored window height -- the window opened far too tall. Pinning a
        // modest implicit size stops the content from driving the window.
        // Pinned to the RESOLVED (saved-or-default) size, not the default:
        // this implicit size is what Wayland actually maps on first frame, so
        // resolving it here means the window appears at the user's size
        // immediately — the old default-then-reassert dance mapped at 860×520
        // and visibly snapped to the saved size a few frames later.
        implicitWidth: window.resolvedWidth()
        implicitHeight: window.resolvedHeight()

        // Gentle page motion (was: whatever the Material style's default is —
        // an instant swap next to the app's animated toggles/buttons). A short
        // fade + 12px slide, both directions, cited from the motion tokens.
        pushEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 12; to: 0; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
        }
        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast; easing.type: Easing.OutCubic }
        }
        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: -12; to: 0; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
        }
        popExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast; easing.type: Easing.OutCubic }
        }

        Component.onCompleted: {
            // Perform our early initialization before constructing
            // the initial view and pushing it to the StackView
            doEarlyInit()
            var initialProperties = window.hostSourceOverride
                    ? ({ hostSource: window.hostSourceOverride }) : ({})
            push(window.startupRoute(), initialProperties)
        }

        onCurrentItemChanged: {
            // Route pages own their surface focus (for example, returning
            // from Settings hands focus to the Hosts list).  Raw segues do
            // not, so retain the StackView-level fallback for those items.
            if (currentItem && !currentItem.shellRouteKind) {
                currentItem.forceActiveFocus()
            }
            window.fulfillPendingAddHostRequest()
        }
        onBusyChanged: if (!busy) window.fulfillPendingAddHostRequest()

        // Esc navigates back through pages, and does nothing at the root —
        // desktop apps don't treat Esc as "quit" (that was upstream's
        // TV/gamepad convention, complete with an "Are you sure?" dialog on
        // every press). Quit is the window close button or Ctrl+Q below,
        // and neither needs a confirmation: nothing is in progress on the
        // launcher by definition (an active stream has its own page and
        // its own disconnect handling).
        Keys.onEscapePressed: {
            if (depth > 1) {
                window.requestRouteBack()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1) {
                window.requestRouteBack()
            }
        }

        Keys.onMenuPressed: {
            window.navigateTo(window.settingsRoute, "settings")
        }

        // This is a keypress we've reserved for letting the
        // SdlGamepadKeyNavigation object tell us to show settings
        // when Menu is consumed by a focused control.
        Keys.onHangupPressed: {
            window.navigateTo(window.settingsRoute, "settings")
        }
    }

    Connections {
        target: stackView.currentItem
        ignoreUnknownSignals: true
        function onAddHostOverlayActivated() {
            window.openAddHostDialogForRoute(stackView.currentItem)
        }
        function onWindowEffectRequested(effect) {
            window.consumeWindowEffect(effect, stackView.currentItem)
        }
    }

    Connections {
        target: window.pendingAddHostSourceRoute
        ignoreUnknownSignals: true
        function onRouteDestroyed() {
            window.clearPendingAddHostRequest()
        }
    }

    Connections {
        target: window.pendingAddHostTargetRoute
        ignoreUnknownSignals: true
        function onRouteDestroyed() {
            window.clearPendingAddHostRequest()
        }
    }

    // Global shortcuts that used to live on the legacy toolbar's buttons --
    // the buttons are gone (ported to sidebar affordances: "Pair a new
    // host…" footer row, sidebar cog), but the keyboard shortcuts themselves
    // are still worth keeping since they don't depend on any visible chrome.
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: window.requestAddHost()
    }

    // The application-level dialog remains visually byte-stable, but the
    // active host route owns the one coalesced add-host operation.
    function requestAddHost() {
        var route = stackView.currentItem
        if (route && route.shellRouteKind === "hosts") {
            activateAddHostOnRoute(route)
            return
        }
        if (route && route.shellRouteKind === "settings" && route.page) {
            if (!pendingAddHostRequest) {
                pendingAddHostRequest = true
                pendingAddHostSourceRoute = route
                pendingAddHostTargetRoute = null
                route.page.requestBack(false)
            }
        }
    }

    // StandardKey.Preferences is empty on most Linux themes: Ctrl+, is what GNOME
    // apps use, and KDE's Ctrl+Shift+, comes from the platform theme.
    Shortcut {
        sequences: [StandardKey.Preferences, "Ctrl+,"]
        onActivated: window.navigateTo(window.settingsRoute, "settings")
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        onActivated: Qt.quit()
    }

    // This timer keeps us polling for 5 minutes of inactivity
    // to allow the user to work with Moonlight on a second display
    // while dealing with configuration issues. This will ensure
    // machines come online even if the input focus isn't on Moonlight.
    Timer {
        id: inactivityTimer
        interval: 5 * 60000
        onTriggered: window.applyPollingDecision(
                         pollingLifecycle.inactivityExpired(window.active), false)
    }

    function applyPollingDecision(decision, notifyFocus) {
        if (decision.stopInactivityTimer) {
            inactivityTimer.stop()
        }
        if (decision.startInactivityTimer) {
            inactivityTimer.restart()
        }
        if (decision.stopPolling) {
            ComputerManager.stopPollingAsync()
            pollingLifecycle.commitPollingState(false)
        }
        if (decision.startPolling) {
            ComputerManager.startPolling()
            pollingLifecycle.commitPollingState(true)
        }
        if (notifyFocus) {
            SdlGamepadKeyNavigation.notifyWindowFocus(decision.hasFocus)
        }
    }

    onVisibleChanged: {
        window.applyPollingDecision(
                    pollingLifecycle.visibilityChanged(visible, active), true)
    }

    onActiveChanged: {
        window.applyPollingDecision(
                    pollingLifecycle.activityChanged(active, visible), true)
    }

    function navigateTo(url, routeKind)
    {
        if (pinnedQuitRoute !== null)
            return
        if (routeKind === "settings" && adwaitaSurfaceActive) {
            if (!preferencesDialog.visible) preferencesDialog.open()
            return
        }
        var existingItem = stackView.find(function(item, index) {
            return item && item.shellRouteKind === routeKind
        })

        if (existingItem !== null) {
            // Pop to the existing item
            stackView.pop(existingItem)
        }
        else {
            // Create a new item
            stackView.push(url)
        }
    }

    // The launcher outlives stream routes, including asynchronous final cleanup.
    function reportStreamCleanupFailure(message) {
        streamCleanupErrorDialog.text = message
        window.visible = true
        streamCleanupErrorDialog.open()
    }

    ErrorMessageDialog {
        id: streamCleanupErrorDialog
        objectName: "starlight.stream-cleanup-error-dialog"
        standardButtons: Dialog.Ok
    }

    ErrorMessageDialog {
        id: noHwDecoderDialog
        text: qsTr("No functioning hardware accelerated video decoder was detected by Moonwalk. " +
                   "Your streaming performance may be severely degraded in this configuration.")
        helpText: qsTr("Click the Help button for more information on solving this problem.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    ErrorMessageDialog {
        id: xWaylandDialog
        text: qsTr("Hardware acceleration doesn't work on XWayland. Continuing on XWayland may result in poor streaming performance. " +
                   "Try running with QT_QPA_PLATFORM=wayland or switch to X11.")
        helpText: qsTr("Click the Help button for more information.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    NavigableMessageDialog {
        id: wow64Dialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        text: qsTr("This version of Moonwalk isn't optimized for this computer. Please download the %1 version of Moonwalk for the best streaming performance.").arg(SystemProperties.friendlyNativeArchName)
        onAccepted: {
            Qt.openUrlExternally("https://github.com/joaodriessen/moonwalk/releases");
        }
    }

    ErrorMessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        text: qsTr("Moonwalk detected gamepads without a mapping:") + "\n" + unmappedGamepads
        helpTextSeparator: "\n\n"
        helpText: qsTr("Click the Help button for information on how to map your gamepads.")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Gamepad-Mapping"
    }


    // HACK: This belongs in StreamSegue but keeping a dialog around after the parent
    // dies can trigger bugs in Qt 5.12 that cause the app to crash. For now, we will
    // host this dialog in a QML component that is never destroyed.
    //
    // To repro: Start a stream, cut the network connection to trigger the "Connection
    // terminated" dialog, wait until the app grid times out back to the PC grid, then
    // try to dismiss the dialog.
    ErrorMessageDialog {
        id: streamSegueErrorDialog
        objectName: "starlight.stream-error-dialog"

        property bool quitAfter: false
        property var diagnosticSession: null
        property bool diagnosticAdded: false

        function appendDiagnostic(result) {
            if (!visible || diagnosticAdded || result === 0 || result === -1)
                return
            diagnosticAdded = true
            text += "\n\n" + qsTr("The network test found blocked Internet streaming ports. This may affect connections outside your local network.")
        }

        onDiagnosticSessionChanged: diagnosticAdded = false
        onOpened: {
            if (diagnosticSession && diagnosticSession.connectivityTestResult !== undefined)
                appendDiagnostic(diagnosticSession.connectivityTestResult)
        }
        Connections {
            target: streamSegueErrorDialog.diagnosticSession
            ignoreUnknownSignals: true
            function onConnectionDiagnosticReady(result) {
                streamSegueErrorDialog.appendDiagnostic(result)
            }
        }

        onClosed: {
            if (diagnosticSession && diagnosticSession.cancelConnectivityTest)
                diagnosticSession.cancelConnectivityTest()
            diagnosticSession = null
            if (quitAfter) {
                Qt.quit()
            }

            // StreamSegue assumes its dialog will be re-created each time we
            // start streaming, so fake it by wiping out the text each time.
            text = ""
        }
    }

    // HIG-ified per item 13: a title + one-line description of what to type,
    // instead of the old bare bold label. Same accept/cancel/addNewHostManually
    // path as before.
    NavigableDialog {
        id: addPcDialog
        objectName: "starlight.add-host-dialog"
        readonly property var hostOverlay:
            stackView.currentItem && stackView.currentItem.shellRouteKind === "hosts"
            && stackView.currentItem.page
            && stackView.currentItem.page.baseOverlay.kind === "add-host"
            ? stackView.currentItem.page.baseOverlay : null
        readonly property string addHostError:
            hostOverlay && hostOverlay.payload && hostOverlay.payload.error
            && editText.text.trim() === String(hostOverlay.payload.address || "")
            ? String(hostOverlay.payload.error) : ""

        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Theme.dialogWidth

        onOpened: {
            if (hostOverlay && hostOverlay.payload && hostOverlay.payload.address)
                editText.text = hostOverlay.payload.address
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onRejected: {
            var route = stackView.currentItem
            if (route && route.shellRouteKind === "hosts")
                route.dismissAddHostOverlay()
        }

        onAccepted: {
            if (editText.text.trim()) {
                var route = stackView.currentItem
                if (route && route.shellRouteKind === "hosts")
                    route.submitAddHost(editText.text.trim())
                else
                    ComputerManager.addNewHostManually(editText.text.trim())
            }
            else {
                var emptyRoute = stackView.currentItem
                if (emptyRoute && emptyRoute.shellRouteKind === "hosts")
                    emptyRoute.submitAddHost("")
            }
        }

        // Bound as contentItem (not a bare child) so Layout.fillWidth below
        // actually resolves against the dialog's available width, same as
        // pairSheet in PcView.qml. As a plain child of the default
        // contentItem, ColumnLayout's own implicit width is driven by its
        // unconstrained children instead -- the title/description Labels
        // never wrap and the TextField sizes to its unwrapped placeholder,
        // so the whole layout blows past the dialog's fixed width: 340
        // (most visible at fractional/HiDPI scale factors, e.g. 200% on
        // Wayland, where the wider font metrics make the overflow obvious).
        contentItem: ColumnLayout {
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: qsTr("Pair a New Host")
                font.pixelSize: Theme.sizeHeadline
                font.weight: Font.DemiBold
                color: Theme.label
            }

            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: 12
                text: qsTr("Enter the IP address or hostname of the host running Sunshine (or another compatible server).")
                font.pixelSize: Theme.sizeCaption
                color: Theme.label2
                wrapMode: Text.Wrap
            }

            SLTextField {
                id: editText
                objectName: "starlight.add-host.address"
                Layout.fillWidth: true
                focus: true
                placeholderText: qsTr("e.g. 192.168.1.42")

                onReturnPressed: addPcDialog.accept()
            }
            Label {
                Layout.fillWidth: true
                visible: addPcDialog.addHostError.length > 0
                text: addPcDialog.addHostError
                wrapMode: Text.WordWrap
                color: Theme.label
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }
}
