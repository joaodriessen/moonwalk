import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0
import Starlight 1.0
import Starlight.Test 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "ShellSelection"
    when: windowShown

    // These are the startup context properties supplied by main.cpp.
    property url initialView: Qt.resolvedUrl("../../app/gui/adaptive/HostPageRoute.qml")
    property bool runConfigChecks: false
    property bool adaptiveShellRequested: false
    property bool gnomeDesktopAdapterActive: false
    property bool plasmaDesktopAdapterActive: false

    property var launcherComponent: null
    property var launcherWindow: null
    property var hostSource: null

    function initTestCase() {
        launcherComponent = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/main.qml"))
        verify(launcherComponent.status === Component.Ready,
               launcherComponent.errorString())
    }

    function cleanup() {
        if (launcherWindow) {
            launcherWindow.close()
            launcherWindow.destroy()
            launcherWindow = null
        }
        Appearance.reset()
        gnomeDesktopAdapterActive = false
        plasmaDesktopAdapterActive = false
    }

    function descendants(object) {
        var result = []
        if (!object) {
            return result
        }

        var children = object.children || []
        for (var i = 0; i < children.length; ++i) {
            result.push(children[i])
            result = result.concat(descendants(children[i]))
        }

        var data = object.data || []
        for (var j = 0; j < data.length; ++j) {
            if (!TestCompat.containsIdentity(children, data[j])) {
                result.push(data[j])
                result = result.concat(descendants(data[j]))
            }
        }
        return result
    }

    function findByObjectName(object, objectName) {
        var objects = [object].concat(descendants(object))
        for (var i = 0; i < objects.length; ++i) {
            if (objects[i] && objects[i].objectName === objectName) {
                return objects[i]
            }
        }
        return null
    }

    function routeItems() {
        return [launcherWindow].concat(descendants(launcherWindow)).filter(
                    function(object) {
            return object && object.shellRouteId !== undefined
        })
    }

    function workflowOwners() {
        return [launcherWindow].concat(descendants(launcherWindow)).filter(
                    function(object) {
            return object
                    && object.objectName === "starlight.workflow.host"
        })
    }

    function activeRoutes() {
        return routeItems().filter(function(object) {
            return object.shellRouteActive
        })
    }

    function shellStack() {
        return findByObjectName(launcherWindow, "starlight.shell.stack")
    }

    function directFullGround(item) {
        var items = item ? (item.children || []) : []
        for (var index = 0; index < items.length; ++index) {
            var candidate = items[index]
            if (candidate && candidate.color !== undefined
                    && candidate.width === item.width
                    && candidate.height === item.height)
                return candidate
        }
        return null
    }

    function launcherCsdGround() {
        var objects = descendants(launcherWindow)
        for (var index = 0; index < objects.length; ++index) {
            var candidate = objects[index]
            if (candidate && candidate.color !== undefined
                    && candidate.radius === launcherWindow.csdRadius
                    && candidate.z === -1)
                return candidate
        }
        return null
    }

    function qmlSession() {
        return Qt.createQmlObject("import Session 1.0; Session {}", testCase,
                                  "shell-selection-session")
    }

    function requestFocusForTestWindow() {
        launcherWindow.requestActivate()
        wait(50)
        return launcherWindow.active
    }

    function createLauncher(useAdaptiveShell, eligible, adapterSource) {
        ComputerManager.reset()
        SdlGamepadKeyNavigation.reset()
        StreamingPreferences.reset()
        adaptiveShellRequested =
                useAdaptiveShell && (eligible === undefined ? true : eligible)

        var initialProperties = {}
        hostSource = Qt.createQmlObject('import Starlight.Test 1.0; TestHostSource {}',
                                        testCase, "shell-host-source")
        initialProperties.hostSourceOverride = hostSource
        if (adapterSource !== undefined) {
            initialProperties.adaptiveAdapterSource = adapterSource
        }

        launcherWindow = launcherComponent.createObject(null,
                                                        initialProperties)
        verify(launcherWindow !== null, launcherComponent.errorString())
        wait(0)
        return launcherWindow
    }

    function preparePairing(hostRoute, workflowOwner) {
        hostSource.replaceHosts([{
            name: "Golden host",
            uuid: "golden-host",
            online: true,
            paired: false,
            busy: false,
            wakeable: false,
            serverSupported: true,
            address: "192.0.2.1",
            details: "Test fixture",
            serverSoftware: "Sunshine",
            statusUnknown: false
        }])
        hostSource.pairingStateSource = workflowOwner

        var hostList = findByObjectName(hostRoute, "starlight.hosts.list")
        verify(hostList !== null)
        tryVerify(function() {
            return hostRoute.page.hosts.rowCount() === 1
        })
        hostRoute.page.selectHostByUuid("golden-host")
        tryCompare(hostRoute.page, "selectedHostIndex", 0)
    }

    function exerciseGoldenSlice(useAdaptiveShell) {
        createLauncher(useAdaptiveShell)

        var expectedHostRoute = useAdaptiveShell
                ? "adaptive-host" : "legacy-host"
        var expectedSettingsRoute = useAdaptiveShell
                ? "adaptive-settings" : "legacy-settings"

        compare(activeRoutes().length, 1,
                "Startup must activate one and only one shell route")
        compare(activeRoutes()[0].shellRouteId, expectedHostRoute)
        compare(workflowOwners().length, 1,
                "The selected host route must own exactly one HostWorkflow")

        var hostRoute = activeRoutes()[0]
        var workflowOwner = workflowOwners()[0]
        preparePairing(hostRoute, workflowOwner)

        hostRoute.beginPairing()
        compare(hostRoute.computerModel.pairComputerCalls, 1)
        compare(hostRoute.computerModel.lastPairComputerIndex, 0)
        compare(hostRoute.computerModel.lastPairComputerPin, "1234")
        compare(hostRoute.computerModel.pendingPairingAttemptsAtDispatch.length,
                1)
        var dispatchedAttempt = hostRoute.testPairAttemptId
        compare(hostRoute.computerModel.pendingPairingAttemptsAtDispatch[0],
                JSON.stringify([dispatchedAttempt]),
                "Pairing workflow state must commit before backend dispatch")
        compare(JSON.stringify(workflowOwner.pendingPairingAttempts),
                JSON.stringify([dispatchedAttempt]))

        hostSource.completePairing("golden pairing failure")
        wait(0)
        compare(JSON.stringify(workflowOwner.pendingPairingAttempts), "[]")

        var errorText = ""
        if (useAdaptiveShell) {
            var dialogLayer = findByObjectName(hostRoute, "gnome-host-dialog-layer")
            verify(dialogLayer !== null)
            tryCompare(dialogLayer, "visibleOverlayKind", "error")
            errorText = dialogLayer.state.body
            hostRoute.page.dismissOverlay(hostRoute.page.interruptOverlay.operationId)
        } else {
            var errorDialog = findByObjectName(
                        hostRoute, "starlight.hosts.error-dialog")
            verify(errorDialog !== null)
            tryCompare(errorDialog, "visible", true)
            errorText = errorDialog.text
            errorDialog.close()
        }
        compare(errorText, "golden pairing failure")

        // Native menus live in their popup, outside the item tree this walks.
        if (useAdaptiveShell) {
            hostRoute.page.requestOpenSettings()
        } else {
            var settingsButton = findByObjectName(
                        hostRoute, "starlight.hosts.open-settings")
            verify(settingsButton !== null)
            settingsButton.clicked()
        }
        if (useAdaptiveShell) {
            // Adwaita preferences float over the still-visible hosts route.
            var preferences = launcherWindow.preferencesPopup
            tryCompare(preferences, "opened", true)
            verify(preferences.settingsRoute !== null)
            compare(preferences.settingsRoute.shellRouteId, expectedSettingsRoute)
            tryCompare(preferences.settingsRoute, "shellRouteActive", true)
        } else {
            tryVerify(function() {
                return activeRoutes().length === 1
            })
            compare(activeRoutes().length, 1,
                    "Settings navigation must leave exactly one active route")
            compare(activeRoutes()[0].shellRouteId, expectedSettingsRoute)
        }
        compare(workflowOwners().length, 1,
                "Settings must reuse the inactive host route's sole workflow owner")
        compare(SdlGamepadKeyNavigation.uiNavigation, true,
                "Settings activation must retain controller navigation semantics")

        if (useAdaptiveShell) {
            preferences.settingsRoute.page.requestBack(false)
            tryCompare(preferences, "visible", false)
        } else {
            launcherWindow.goBack()
        }
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteId === expectedHostRoute
        })
        compare(activeRoutes().length, 1)
        compare(activeRoutes()[0].shellRouteId, expectedHostRoute)
        compare(StreamingPreferences.saveCalls, 2,
                "Leaving settings must preserve the characterized deactivate-plus-destroy persistence")
        compare(SdlGamepadKeyNavigation.uiNavigation, false)

        return {
            hostRoute: expectedHostRoute,
            settingsRoute: expectedSettingsRoute,
            pairCalls: hostRoute.computerModel.pairComputerCalls,
            pairIndex: hostRoute.computerModel.lastPairComputerIndex,
            pairPin: hostRoute.computerModel.lastPairComputerPin,
            dispatchState: JSON.parse(
                hostRoute.computerModel.pendingPairingAttemptsAtDispatch[0]).length,
            pendingState: JSON.stringify(
                              workflowOwner.pendingPairingAttempts),
            errorText: errorText,
            saves: StreamingPreferences.saveCalls,
            controllerUiNavigation: SdlGamepadKeyNavigation.uiNavigation,
            workflowOwners: workflowOwners().length
        }
    }

    function test_legacyRemainsTheSingleDefaultRouteAndWorkflowOwner() {
        createLauncher(false)

        compare(activeRoutes().length, 1)
        compare(activeRoutes()[0].shellRouteId, "legacy-host")
        compare(workflowOwners().length, 1)
        compare(launcherWindow.platformAdapter, null,
                "The rollback path must not construct the adaptive adapter")
        compare(launcherWindow.adaptiveShellActive, false)
    }

    function test_adaptiveSelectsOneRouteWithoutDuplicatingTheWorkflowOwner() {
        createLauncher(true)

        compare(activeRoutes().length, 1)
        compare(activeRoutes()[0].shellRouteId, "adaptive-host")
        compare(workflowOwners().length, 1)
        verify(launcherWindow.platformAdapter !== null)
        compare(launcherWindow.adaptiveShellActive, true)
    }

    function test_adaptive_add_host_uses_only_native_dialog() {
        gnomeDesktopAdapterActive = true
        createLauncher(true)
        var route = activeRoutes()[0]
        var legacyDialog = findByObjectName(launcherWindow, "starlight.add-host-dialog")
        verify(legacyDialog !== null)
        route.page.requestAddHostOverlay()
        tryCompare(route.page.baseOverlay, "kind", "add-host")
        tryVerify(function() {
            var nativeLayer = findByObjectName(route.legacySurface,
                                               "gnome-host-dialog-layer")
            return nativeLayer && nativeLayer.visibleOverlayKind === "add-host"
        })
        wait(0)
        compare(legacyDialog.visible, false)
    }

    function test_plasma_add_host_uses_only_native_dialog() {
        if (!kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        plasmaDesktopAdapterActive = true
        createLauncher(true)
        var route = activeRoutes()[0]
        var legacyDialog = findByObjectName(launcherWindow, "starlight.add-host-dialog")
        verify(route.nativePlasmaSurface)
        verify(legacyDialog !== null)
        route.page.requestAddHostOverlay()
        tryCompare(route.page.baseOverlay, "kind", "add-host")
        tryVerify(function() {
            var nativeLayer = findByObjectName(route.legacySurface,
                                               "plasma-host-dialog-layer")
            return nativeLayer && nativeLayer.visibleOverlayKind === "add-host"
        })
        wait(0)
        compare(legacyDialog.visible, false)
    }

    function test_live_switch_keeps_connect_ground_on_current_gnome_token() {
        gnomeDesktopAdapterActive = true
        Appearance.colorScheme = 2
        tryCompare(Appearance, "dark", true)
        createLauncher(true)
        var route = activeRoutes()[0]
        var surface = route.legacySurface
        var stack = shellStack()
        tryVerify(function() { return stack.currentItem === route && !stack.busy })
        var pageGround = directFullGround(surface)
        verify(pageGround !== null)
        compare(pageGround.color, surface.tokens.window)
        compare(launcherWindow.transitionGround, surface.tokens.window)

        Appearance.colorScheme = 1
        tryCompare(Appearance, "dark", false)
        tryVerify(function() {
            return Qt.colorEqual(pageGround.color, surface.tokens.window)
                    && Qt.colorEqual(launcherWindow.transitionGround,
                                     surface.tokens.window)
        })
        compare(activeRoutes()[0], route,
                "live appearance changes must not reconstruct the Hosts route")
        compare(route.legacySurface, surface,
                "live appearance changes must not reconstruct the GNOME surface")

        hostSource.replaceHosts([{ uuid: "golden-host", name: "Golden host",
                                   online: true, paired: true, busy: false,
                                   wakeable: false, serverSupported: true,
                                   statusUnknown: false }])
        hostSource.sessionOverride = qmlSession()
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("golden-host", 0, "Desktop")
        route.page.requestConnect("golden-host")
        var stream = null
        tryVerify(function() {
            stream = findByObjectName(launcherWindow,
                                      "starlight.stream-segue")
            return stream !== null && stack.currentItem === stream && !stack.busy
        })
        var streamGround = directFullGround(stream)
        verify(streamGround !== null)
        compare(streamGround.color, surface.tokens.window,
                "Connect must retain the current live GNOME ground")
        var csdGround = launcherCsdGround()
        verify(csdGround !== null)
        compare(csdGround.color, surface.tokens.window,
                "the GNOME launcher backstop must share the live ground")

        stream.sessionFinished(0)
        tryVerify(function() { return stack.currentItem === route && !stack.busy })
        compare(route.legacySurface, surface)
        compare(pageGround.color, surface.tokens.window)
    }

    function test_ineligibleEntryPointDoesNotConstructTheAdaptiveAdapter() {
        createLauncher(true, false)

        compare(activeRoutes().length, 1)
        compare(activeRoutes()[0].shellRouteId, "legacy-host")
        compare(workflowOwners().length, 1)
        compare(launcherWindow.platformAdapter, null)
        compare(launcherWindow.adaptiveShellActive, false)
    }

    function test_adapterLoadFailureFallsBackToTheLegacyRoute() {
        ignoreWarning(/Adaptive shell adapter unavailable; using legacy shell/)
        createLauncher(true, true,
                       Qt.resolvedUrl("fixtures/MissingPlatformAdapter.qml"))

        compare(activeRoutes().length, 1)
        compare(activeRoutes()[0].shellRouteId, "legacy-host")
        compare(workflowOwners().length, 1)
        compare(launcherWindow.platformAdapter, null)
        compare(launcherWindow.adaptiveShellActive, false,
                "Failed validation must report the effective legacy route")
    }

    // A SettingsView characterization must never bind undefined test doubles:
    // this list mirrors each singleton member used by the global settings page.
    function test_settingsViewPreferenceFixtureProvidesResettableGlobalDefaults() {
        StreamingPreferences.reset()
        var defaults = {
            muteOnFocusLoss: false,
            quitAppAfter: false,
            connectionWarnings: true,
            configurationWarnings: true,
            richPresence: false,
            keepAwake: true,
            absoluteTouchMode: false,
            swapMouseButtons: false,
            reverseScrollDirection: false,
            swapFaceButtons: false,
            multiController: true,
            gamepadMouse: false,
            backgroundGamepad: false,
            detectNetworkBlocking: true,
            showPerformanceOverlay: false,
            playAudioOnHost: false,
            windowMode: StreamingPreferences.WM_WINDOWED,
            recommendedFullScreenMode: StreamingPreferences.WM_FULLSCREEN_DESKTOP,
            captureSysKeysMode: StreamingPreferences.CSK_FULLSCREEN,
            language: StreamingPreferences.LANG_AUTO
        }
        for (var name in defaults) {
            verify(name in StreamingPreferences,
                   "SettingsView requires StreamingPreferences." + name)
            compare(StreamingPreferences[name], defaults[name],
                    "SettingsView default drifted for " + name)
        }
        StreamingPreferences.quitAppAfter = true
        StreamingPreferences.captureSysKeysMode = StreamingPreferences.CSK_ALWAYS
        StreamingPreferences.reset()
        compare(StreamingPreferences.quitAppAfter, false)
        compare(StreamingPreferences.captureSysKeysMode,
                StreamingPreferences.CSK_FULLSCREEN)
    }

    function test_addHostRequestCoalescesAcrossSettingsBackNavigation() {
        createLauncher(false)
        var hostRoute = activeRoutes()[0]

        launcherWindow.navigateTo(launcherWindow.settingsRoute, "settings")
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "settings"
        })

        launcherWindow.requestAddHost()
        launcherWindow.requestAddHost()
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "hosts"
        })
        tryVerify(function() {
            return hostRoute.page.baseOverlay.kind === "add-host"
        })
        compare(launcherWindow.pendingAddHostRequest, false)
        compare(hostRoute.addHostOverlayForwardCount, 1,
                "the settings navigation token must invoke the route forwarding entry exactly once")
        compare(hostRoute.context.queuedBaseOverlays.length, 0,
                "Repeated Ctrl+N must not queue duplicate add-host operations")

        hostRoute.page.dismissOverlay(hostRoute.page.baseOverlay.operationId)
        preparePairing(hostRoute, workflowOwners()[0])
        hostRoute.beginPairing()
        compare(hostRoute.page.baseOverlay.kind, "pairing")
        launcherWindow.requestAddHost()
        launcherWindow.requestAddHost()
        compare(hostRoute.context.queuedBaseOverlays.length, 1,
                "Ctrl+N must enqueue exactly one durable add-host request behind pairing")
        hostRoute.cancelPairing(hostRoute.page.baseOverlay.payload.attemptId)
        tryCompare(hostRoute.page.baseOverlay, "kind", "add-host")
    }

    function test_pending_add_host_token_clears_when_its_navigation_is_destroyed() {
        createLauncher(false)
        launcherWindow.navigateTo(launcherWindow.settingsRoute, "settings")
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "settings"
        })
        var stack = shellStack()
        verify(stack !== null)

        launcherWindow.requestAddHost()
        verify(launcherWindow.pendingAddHostRequest)
        stack.clear(StackView.Immediate)
        tryCompare(launcherWindow, "pendingAddHostRequest", false,
                   5000,
                   "an interrupted/destroyed Settings-to-Hosts transition must not leave a shortcut token behind")
    }

    function test_add_host_shortcut_is_rejected_while_a_stream_or_quit_segue_owns_the_stack() {
        createLauncher(false)
        var stack = shellStack()
        verify(stack !== null)
        var stream = Qt.createQmlObject(
                    'import QtQuick 2.15; Item { property string shellRouteKind: "stream" }',
                    stack, "test-stream-segue")
        stack.push(stream, StackView.Immediate)
        launcherWindow.requestAddHost()
        compare(launcherWindow.pendingAddHostRequest, false)
        compare(stack.currentItem, stream)

        var quit = Qt.createQmlObject(
                    'import QtQuick 2.15; Item { property string shellRouteKind: "quit" }',
                    stack, "test-quit-segue")
        stack.replace(quit, StackView.Immediate)
        launcherWindow.requestAddHost()
        compare(launcherWindow.pendingAddHostRequest, false)
        compare(stack.currentItem, quit)
    }

    function test_settings_back_restores_hosts_focus_after_the_add_host_overlay_closes() {
        createLauncher(false)
        var hostRoute = activeRoutes()[0]
        var hostList = findByObjectName(hostRoute, "starlight.hosts.list")
        verify(hostList !== null)
        var canObserveActiveFocus = requestFocusForTestWindow()
        launcherWindow.navigateTo(launcherWindow.settingsRoute, "settings")
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "settings"
        })
        launcherWindow.requestAddHost()
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0] === hostRoute
        })
        var addHostDialog = findByObjectName(launcherWindow,
                                             "starlight.add-host-dialog")
        verify(addHostDialog !== null)
        tryVerify(function() { return addHostDialog.visible },
                  5000,
                  "the consumed add-host overlay must open its existing dialog")
        // The dialog owns construction of the field. Wait for the dialog's
        // visible lifecycle before resolving its child: this prevents a
        // lookup race from hiding a real focus regression.
        var addressField = null
        tryVerify(function() {
            addressField = findByObjectName(launcherWindow,
                                            "starlight.add-host.address")
            return addressField !== null
        }, 5000, "the visible add-host dialog must expose its address field")

        // Managed X11 can leave a test Window inactive even after an explicit
        // activation request. In that case activeFocus is not observable; the
        // real TextInput's focus preference is the deterministic focus-scope
        // contract. Never assert the wrapper FocusScope instead.
        if (canObserveActiveFocus)
            tryVerify(function() { return addressField.accessibleItem.activeFocus })
        else
            verify(addressField.accessibleItem.focus)

        addHostDialog.reject()
        tryCompare(hostRoute.page.baseOverlay, "kind", "none")
        compare(activeRoutes()[0], hostRoute,
                "dismissing the overlay must return to the same Hosts route")
        if (canObserveActiveFocus)
            tryVerify(function() { return hostList.activeFocus })
        else
            verify(hostList.focus,
                   "the inactive harness must retain the documented Hosts focus preference")
    }

    function test_legacy_add_host_failure_restores_attempted_address() {
        createLauncher(false)
        var hostRoute = activeRoutes()[0]
        launcherWindow.requestAddHost()
        var dialog = findByObjectName(launcherWindow, "starlight.add-host-dialog")
        tryVerify(function() { return dialog && dialog.visible })
        var field = findByObjectName(launcherWindow, "starlight.add-host.address")
        verify(field !== null)
        field.text = "192.0.2.10"
        dialog.accept()
        ComputerManager.manualHostAddCompleted(hostRoute.page.baseOverlay.payload.attemptId, false, false)
        tryVerify(function() { return dialog.visible })
        compare(hostRoute.page.baseOverlay.kind, "add-host")
        compare(field.text, "192.0.2.10")
        verify(dialog.addHostError.length > 0)
        dialog.reject()
    }

    function test_settingsBackConsumesLanguageClearDecision() {
        createLauncher(false)
        launcherWindow.navigateTo(launcherWindow.settingsRoute, "settings")
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "settings"
        })
        var settingsRoute = activeRoutes()[0]
        settingsRoute.context.languageClearOnBack = true

        launcherWindow.requestRouteBack()
        tryVerify(function() {
            return activeRoutes().length === 1
                    && activeRoutes()[0].shellRouteKind === "hosts"
        })
        compare(launcherWindow.clearOnBack, false,
                "The settings back effect must consume and reset clear-to-root state")
    }

    function test_legacyAndAdaptiveGoldenSlicesKeepBehavioralParity() {
        var legacy = exerciseGoldenSlice(false)
        launcherWindow.close()
        launcherWindow.destroy()
        launcherWindow = null

        var adaptive = exerciseGoldenSlice(true)

        compare(adaptive.pairCalls, legacy.pairCalls)
        compare(adaptive.pairIndex, legacy.pairIndex)
        compare(adaptive.pairPin, legacy.pairPin)
        compare(adaptive.dispatchState, legacy.dispatchState)
        compare(adaptive.pendingState, legacy.pendingState)
        compare(adaptive.errorText, legacy.errorText)
        compare(adaptive.saves, legacy.saves)
        compare(adaptive.controllerUiNavigation,
                legacy.controllerUiNavigation)
        compare(adaptive.workflowOwners, legacy.workflowOwners)
    }
}
