import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import Starlight.Test 1.0

TestCase {
    id: testCase
    name: "WindowEffects"
    when: windowShown

    property url initialView: Qt.resolvedUrl("fixtures/WindowEffectRoute.qml")
    property url productionInitialView:
        Qt.resolvedUrl("../../app/gui/adaptive/HostPageRoute.qml")
    property bool runConfigChecks: false
    property bool adaptiveShellRequested: false
    property var launcherComponent: null
    property var launcherWindow: null
    property var hostSource: null
    // Test-context-only construction seam. main.qml does not declare this
    // property, so production cannot alter the stream component source.
    property var streamSegueComponentOverride: null
    // Like the stream factory seam, this exists only in the test engine
    // context; main.qml declares no production-writable override.
    property var quitSegueComponentOverride: null

    function initTestCase() {
        launcherComponent = Qt.createComponent(Qt.resolvedUrl("../../app/gui/main.qml"))
        verify(launcherComponent.status === Component.Ready,
               launcherComponent.errorString())
    }

    function cleanup() {
        SystemProperties.decoderProbeReady = false
        SystemProperties.decoderProbeError = ""
        SystemProperties.decoderProbeRunning = false
        if (launcherWindow) {
            launcherWindow.close()
            launcherWindow.destroy()
            launcherWindow = null
        }
        hostSource = null
        streamSegueComponentOverride = null
        quitSegueComponentOverride = null
        adaptiveShellRequested = false
    }

    function findItem(root, predicate) {
        if (!root)
            return null
        if (predicate(root))
            return root
        const children = root.children || []
        for (let index = 0; index < children.length; ++index) {
            const found = findItem(children[index], predicate)
            if (found)
                return found
        }
        return null
    }

    function hostRoute() {
        return findItem(launcherWindow.contentItem, function(item) {
            return item.shellRouteKind === "hosts"
        })
    }

    function namedItem(objectName) {
        return findItem(launcherWindow.contentItem, function(item) {
            return item.objectName === objectName
        })
    }

    function countNamedItems(root, objectName) {
        if (!root)
            return 0
        let count = root.objectName === objectName ? 1 : 0
        const children = root.children || []
        for (let index = 0; index < children.length; ++index)
            count += countNamedItems(children[index], objectName)
        return count
    }

    function stackContains(item) {
        const stack = namedItem("starlight.shell.stack")
        for (let index = 0; index < stack.depth; ++index) {
            if (stack.get(index) === item)
                return true
        }
        return false
    }

    function launchEffect(route, resume, useQmlSession) {
        route.lastEffect = null
        hostSource.sessionOverride = useQmlSession === false ? null : qmlSession()
        hostSource.setDesktopAppId(resume === true ? 1 : 0)
        hostSource.setRunningApp("host-a", resume === true ? 1 : 0, "Desktop")
        route.context.facade.requestConnect("host-a")
        verify(route.lastEffect !== null)
        return route.lastEffect
    }

    function qmlSession() {
        return Qt.createQmlObject("import Session 1.0; Session {}", testCase,
                                  "window-effects-session")
    }

    function quitEffect(route, useQmlSession) {
        route.lastEffect = null
        hostSource.sessionOverride = useQmlSession === false ? null : qmlSession()
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 77, "Runner")
        route.context.facade.requestConnect("host-a")
        compare(route.context.baseOverlay.kind, "confirm-quit")
        route.context.facade.confirmQuitAndConnect(
                    route.context.baseOverlay.operationId)
        verify(route.lastEffect !== null)
        return route.lastEffect
    }

    function createLauncher() {
        ComputerManager.reset()
        SdlGamepadKeyNavigation.reset()
        StreamingPreferences.reset()
        hostSource = Qt.createQmlObject("import Starlight.Test 1.0; TestHostSource {}",
                                        testCase, "window-effects-host-source")
        hostSource.replaceHosts([{ uuid: "host-a", name: "Host A" }])
        launcherWindow = launcherComponent.createObject(null, {
                                                          hostSourceOverride: hostSource
                                                      })
        verify(launcherWindow !== null, launcherComponent.errorString())
        tryVerify(function() {
            return hostRoute() !== null
        })
    }

    function createProductionLauncher() {
        const savedInitialView = initialView
        initialView = productionInitialView
        createLauncher()
        initialView = savedInitialView
    }

    function launchFromProductionHostRoute() {
        adaptiveShellRequested = true
        createProductionLauncher()
        const route = hostRoute()
        const stack = namedItem("starlight.shell.stack")
        tryVerify(function() { return stack.currentItem === route && !stack.busy })

        hostSource.sessionOverride = qmlSession()
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 0, "Desktop")
        route.page.requestConnect("host-a")
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null && !stack.busy
        })
        return namedItem("starlight.stream-segue")
    }

    function streamGround(streamSegue) {
        return findItem(streamSegue, function(item) {
            return item.parent === streamSegue
                    && item.width === streamSegue.width
                    && item.height === streamSegue.height
                    && item.radius !== undefined
                    && item.color !== undefined
        })
    }

    // The GNOME CSD window edge is fully specified by libadwaita 1.9.2,
    // src/stylesheet/widgets/_window.scss -- byte-identical to the stylesheet
    // compiled into the installed libadwaita-1.so.0:
    //
    //   window.csd { box-shadow: 0 0 14px 5px RGB(0 0 0/15%),
    //                            0 0 5px 2px RGB(0 0 0/10%),
    //                            0 0 0 1px RGB(0 0 0/5%);
    //                border-radius: var(--window-radius);      /* 15px */
    //                outline: 1px solid RGB(255 255 255/7%);
    //                outline-offset: -1px; }
    //
    // All three box-shadow layers -- including the 1px black ring, which is a
    // shadow layer and not a border -- are baked into the nine-patch that
    // scripts/design/gnome-shadows.py crops out of a native GTK 4.22.4
    // render. 19px is that render's measured reach, so the transparent CSD
    // margin cannot be smaller without clipping the shadow. The inset outline
    // is the only ring drawn separately, and none of these values has a
    // light/dark variant: the source defines each one exactly once.
    function test_gnome_window_edge_matches_adwaita_source() {
        if (!gnomeDesktopAdapterActive)
            skip("requires the GNOME CSD adapter fixture")
        createLauncher()
        compare(launcherWindow.csdRadius, 15)
        compare(launcherWindow.csdMargin, 19)

        const shadow = namedItem("window.csd.shadow")
        verify(shadow !== null)
        compare(shadow.opacity, 1.0)
        compare(shadow.anchors.margins, -launcherWindow.csdMargin)
        compare(shadow.border.left, 42)
        compare(shadow.border.right, 42)
        compare(shadow.border.top, 42)
        compare(shadow.border.bottom, 42)
        compare(shadow.source, launcherWindow.csdShadowSource)
        tryVerify(function() { return shadow.status === Image.Ready })

        // Both focus states must exist on one shared geometry: libadwaita
        // deliberately keeps the extents equal between active and backdrop so
        // the window does not jump when focus changes. Probe both explicitly,
        // because only one of them is bound at any moment.
        const probes = ["qrc:/res/window_shadow.png",
                        "qrc:/res/window_shadow_backdrop.png"].map(function(url) {
            return Qt.createQmlObject(
                        'import QtQuick 2.15; Image { source: "' + url + '" }',
                        testCase, "window-edge-probe")
        })
        tryVerify(function() {
            return probes.every(function(p) { return p.status === Image.Ready })
        })
        compare(probes[0].sourceSize.width, probes[1].sourceSize.width)
        compare(probes[0].sourceSize.height, probes[1].sourceSize.height)
        compare(shadow.sourceSize.width, probes[0].sourceSize.width)
        probes.forEach(function(p) { p.destroy() })

        // Exactly one separately drawn ring, at the source colour.
        compare(countNamedItems(launcherWindow.contentItem, "window.csd.outline"), 1)
        const outline = namedItem("window.csd.outline")
        compare(outline.radius, launcherWindow.csdRadius)
        compare(outline.border.width, 1)
        compare(outline.border.color.r, 1)
        compare(outline.border.color.g, 1)
        compare(outline.border.color.b, 1)
        fuzzyCompare(outline.border.color.a, 0.07, 0.005)
    }

    function test_gnome_stream_segue_ground_mirrors_floating_window_chrome() {
        if (!gnomeDesktopAdapterActive)
            skip("requires the GNOME CSD adapter fixture")
        const streamSegue = launchFromProductionHostRoute()
        const route = hostRoute()
        const stack = namedItem("starlight.shell.stack")
        compare(launcherWindow.csdRadius, 15)
        compare(stack.currentItem, streamSegue)
        compare(streamSegue.StackView.status, StackView.Active)
        verify(streamSegue.providesWindowChrome !== true)
        const ground = streamGround(streamSegue)
        verify(ground !== null)
        compare(ground.radius, launcherWindow.csdRadius)

        streamSegue.stageFailed("connect", 1, "47984")
        streamSegue.sessionFinished(0)
        // The recovery screen intentionally retains a terminal launch failure
        // until the user returns, so retry/diagnostics remain available.
        tryCompare(streamSegue, "failed", true)
        compare(stack.currentItem, streamSegue)
        launcherWindow.goBack()
        tryVerify(function() { return stack.currentItem === route && !stack.busy })
    }

    function startProductionQuit() {
        createProductionLauncher()
        const route = hostRoute()
        hostSource.sessionOverride = qmlSession()
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 77, "Runner")
        route.context.facade.requestConnect("host-a")
        compare(route.context.baseOverlay.kind, "confirm-quit")
        route.context.facade.confirmQuitAndConnect(
                    route.context.baseOverlay.operationId)
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
                    && launcherWindow.pinnedQuitRoute === route
        })
        return route
    }

    function test_duplicate_launch_effect_consumed_once() {
        createLauncher()
        const route = hostRoute()
        const effect = launchEffect(route, false)

        verify(launcherWindow.consumeWindowEffect(effect, route))
        verify(!launcherWindow.consumeWindowEffect(effect, route))

        compare(countNamedItems(launcherWindow.contentItem,
                                "starlight.stream-segue"), 1)
    }

    function test_unacknowledged_session_is_disposed() {
        createLauncher()
        const route = hostRoute()
        const effect = launchEffect(route, false, false)

        route.context.routeActive = false

        compare(hostSource.sessionReleaseCount, 1)
        verify(!launcherWindow.consumeWindowEffect(effect, route))
    }

    function test_quit_chain_pins_route_until_terminal() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)

        verify(launcherWindow.consumeWindowEffect(effect, route))
        launcherWindow.navigateTo(launcherWindow.settingsRoute, "settings")
        launcherWindow.goBack()
        verify(stackContains(route))
        compare(launcherWindow.pinnedQuitRoute, route)
        verify(route.context.pendingQuitOperations[effect.payload.operationId] !== undefined)

        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })

        namedItem("starlight.quit-segue").quitAppCompleted("test quit failure")

        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
        })
        verify(stackContains(route))
        compare(Object.keys(route.context.pendingQuitOperations).length, 0)
    }

    function test_open_settings_acknowledges_before_route_deactivation() {
        createLauncher()
        const route = hostRoute()
        route.lastEffect = null
        route.context.facade.requestOpenSettings()
        verify(route.lastEffect !== null)

        verify(launcherWindow.consumeWindowEffect(route.lastEffect, route))
        tryVerify(function() {
            return namedItem("starlight.settings.context") !== null
        })
    }

    function test_facade_session_creation_failure_is_rejected_without_segue() {
        createLauncher()
        const route = hostRoute()
        route.lastEffect = null
        hostSource.setSessionAvailable(false)
        route.context.facade.requestConnect("host-a")

        compare(hostSource.sessionDispatchCount, 1)
        compare(route.lastEffect, null)
        compare(route.context.interruptOverlay.kind, "error")
        compare(countNamedItems(launcherWindow.contentItem,
                                "starlight.stream-segue"), 0)
    }

    function test_invalid_quit_dispatch_recovers_the_pinned_route() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        delete route.context.pendingQuitOperations[effect.payload.operationId]

        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
        })
        verify(stackContains(route))
        compare(hostSource.quitDispatchCount, 0)
    }

    function test_duplicate_quit_success_hands_off_once() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })
        const quitSegue = namedItem("starlight.quit-segue")

        quitSegue.quitAppCompleted(undefined)
        quitSegue.quitAppCompleted(undefined)
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null
                    && launcherWindow.pinnedQuitRoute === null
        })
        compare(countNamedItems(launcherWindow.contentItem,
                                "starlight.stream-segue"), 1)
    }

    function test_duplicate_quit_failure_recovers_once() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })
        const quitSegue = namedItem("starlight.quit-segue")

        quitSegue.quitAppCompleted("test quit failure")
        quitSegue.quitAppCompleted("duplicate failure")
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
        })
        verify(stackContains(route))
    }

    function test_handoff_failure_recovers_without_a_stream() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })
        const quitSegue = namedItem("starlight.quit-segue")
        streamSegueComponentOverride = Qt.createComponent(
                    Qt.resolvedUrl("fixtures/FailingStreamSegue.qml"))
        compare(streamSegueComponentOverride.status, Component.Ready)

        quitSegue.quitAppCompleted(undefined)
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
        })
        verify(stackContains(route))
        compare(countNamedItems(launcherWindow.contentItem,
                                "starlight.stream-segue"), 0)
    }

    function test_disconnect_returns_before_remote_cleanup_finishes() {
        const segue = launchFromProductionHostRoute()
        const session = segue.session
        session.connectionWasEstablished = true
        session.cleanupComplete = false
        launcherWindow.visible = false
        session.quitStarting()
        tryVerify(function() { return !stackContains(segue) }, 1500)
        verify(!session.cleanupComplete)
        verify(launcherWindow.visible)
        verify(hostRoute() !== null)
        session.hostCleanupFailed("Remote cleanup could not be confirmed.")
        const dialog = findChild(launcherWindow, "starlight.stream-cleanup-error-dialog")
        tryCompare(dialog, "visible", true)
        compare(dialog.text, "Remote cleanup could not be confirmed.")
    }

    function test_cleanup_failure_survives_returning_to_hosts() {
        const segue = launchFromProductionHostRoute()
        adaptiveShellRequested = false // Exercise the non-Adwaita message body too.
        const session = segue.session
        launcherWindow.goBack()
        tryVerify(function() { return !stackContains(segue) })
        wait(300)
        session.hostCleanupFailed("The host could not confirm that its streaming session ended.")
        const dialog = findChild(launcherWindow, "starlight.stream-cleanup-error-dialog")
        verify(dialog !== null, "Late cleanup failure must reach the surviving launcher")
        tryCompare(dialog, "visible", true)
        compare(dialog.text, "The host could not confirm that its streaming session ended.")
        const label = findChild(dialog, "starlight.stream-cleanup-error-dialog.message")
        const body = label.mapToItem(dialog.contentItem, 0, 0)
        verify(body.x + label.width <= dialog.availableWidth + 1,
               "The message must fit inside its dialog surface: " + label.width + "/" + dialog.availableWidth)
    }

    function test_reconnect_waits_for_cleanup_and_cancel_invalidates_retry() {
        const segue = launchFromProductionHostRoute()
        const original = segue.session
        const next = qmlSession()
        original.nextRetrySession = next
        original.cleanupComplete = false
        original.connectionWasEstablished = true
        original.retryableTermination = true
        original.sessionFinished(0)
        verify(segue.retryPending, JSON.stringify({ activated: segue.activated, abandoned: segue.abandoned, terminal: segue.terminalDelivered, failed: segue.failed, quitAfter: segue.quitAfter, current: segue.session === original }))
        wait(150)
        compare(original.retryCreateCalls, 0)
        original.cleanupComplete = true
        tryVerify(function() { return segue.session === next }, 2500)
        compare(original.retryCreateCalls, 1)
        verify(original.lastResumeOnly)
        compare(original.releasedCount, 1)
        compare(next.retainedCount, 1)
        next.connectionWasEstablished = true
        next.retryableTermination = true
        next.nextRetrySession = qmlSession()
        next.sessionFinished(0)
        verify(segue.retryPending)
        launcherWindow.goBack()
        wait(1200)
        compare(next.retryCreateCalls, 0)
        compare(next.releasedCount, 1)
    }

    function test_safe_retry_is_explicit_and_keeps_trial_only_after_video() {
        const segue = launchFromProductionHostRoute()
        const original = segue.session
        const trial = qmlSession()
        trial.safeSettingsTrial = true
        original.nextRetrySession = trial
        segue.stageFailed("video stream establishment", 1, "")
        original.sessionFinished(0)
        verify(segue.failed)
        compare(original.retryCreateCalls, 0)
        verify(segue.retryWithSettings(true, false))
        verify(original.lastSafeSettings)
        verify(!original.lastResumeOnly)
        verify(!trial.keepSafeSettings())
        trial.hasVideoFrame = true
        trial.connectionWasEstablished = true
        trial.cleanupComplete = false
        trial.quitStarting()
        verify(segue.trialOutcome)
        verify(!trial.cleanupComplete)
        verify(trial.keepSafeSettings())
    }

    function test_reconnect_budget_does_not_reset_after_a_short_stream() {
        const segue = launchFromProductionHostRoute()
        const session = segue.session
        session.connectionWasEstablished = true
        session.retryableTermination = true
        session.stableMilliseconds = 10
        segue.retryAttempts = 5
        session.sessionFinished(0)
        verify(segue.failed)
        compare(session.retryCreateCalls, 0)
    }
    function test_explicitly_applied_optimization_has_a_separate_handoff_deadline() {
        const segue = launchFromProductionHostRoute()
        const session = segue.session
        session.connectionWasEstablished = true
        session.retryableTermination = true
        session.adjustingForHost = true
        session.cleanupComplete = false
        segue.retryAttempts = 5
        segue.retryDeadline = Date.now() - 1
        session.sessionFinished(0)
        verify(!segue.failed)
        verify(segue.retryPending)
        compare(segue.retryAttempts, 0)
        verify(segue.retryDeadline > Date.now())
        launcherWindow.goBack()
    }

    function test_last_working_retry_is_available_only_after_failure_and_waits_for_cleanup() {
        const segue = launchFromProductionHostRoute()
        const source = segue.session
        const retry = namedItem("starlight.stream.retry-last-working")
        verify(retry !== null)
        verify(!retry.visible)
        source.lastSuccessfulAvailable = true
        source.cleanupComplete = false
        segue.stageFailed("video stream establishment", 1, "")
        source.sessionFinished(0)
        verify(segue.failed)
        verify(retry.visible)
        verify(!retry.enabled)
        compare(source.retryCreateCalls, 0)
        source.nextRetrySession = qmlSession()
        source.cleanupComplete = true
        verify(retry.enabled)
        retry.clicked()
        compare(source.retryCreateCalls, 1)
        verify(source.lastSuccessfulRequested)
        verify(source.lastManualRetry)
        verify(!source.lastSafeSettings)
        verify(segue.session === source.nextRetrySession)
    }

    function test_reconnect_deadline_cancels_inflight_work_and_manual_retry_remains_explicit() {
        const segue = launchFromProductionHostRoute()
        const source = segue.session
        source.connectionWasEstablished = true
        source.retryableTermination = true
        source.cleanupComplete = false
        source.sessionFinished(0)
        const budget = findChild(segue, "starlight.stream-retry-budget")
        verify(budget !== null)
        budget.interval = 1
        budget.restart()
        tryVerify(function() { return segue.failed })
        compare(source.cancelPendingStartCalls, 1)
        verify(!segue.retryPending)
        const timeout = segue.lastFailure
        source.displayLaunchError("late failure must not replace timeout")
        compare(segue.lastFailure, timeout)
        source.cleanupComplete = true
        source.nextRetrySession = qmlSession()
        verify(segue.retryWithSettings(false, false))
        verify(source.lastManualRetry)
        verify(source.lastResumeOnly)
    }

    function test_stream_error_terminal_leaves_the_host_route_recoverable() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, false), route))
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null
        })
        const streamSegue = namedItem("starlight.stream-segue")

        streamSegue.stageFailed("connect", 1, "47984")
        streamSegue.sessionFinished(0)
        verify(stackContains(route))
    }

    function test_stream_adjustments_survive_return_and_stay_with_their_host() {
        const segue = launchFromProductionHostRoute()
        const route = hostRoute()
        segue.recordLaunchWarnings(["Requested HDR; using SDR", "Requested surround; using stereo"])
        segue.session.launchWarnings = ["Requested HDR; using SDR", "Requested surround; using stereo",
                                       "Reconnect to enable local cursor"]
        const session = segue.session
        segue.sessionFinished(0)
        const stack = namedItem("starlight.shell.stack")
        tryVerify(function() { return stack.currentItem === route && !stack.busy })
        compare(session.cancelPendingStartCalls, 1) // Returning also finalizes any retained remote target.
        route.page.requestDetails("host-a")
        verify(route.context.baseOverlay.payload.details.indexOf("Requested HDR; using SDR") >= 0)
        verify(route.context.baseOverlay.payload.details.indexOf("Requested surround; using stereo") >= 0)
        verify(route.context.baseOverlay.payload.details.indexOf("Reconnect to enable local cursor") >= 0)
        route.page.dismissOverlay(route.context.baseOverlay.operationId)
        hostSource.replaceHosts([{uuid: "host-a", name: "Host A"}, {uuid: "host-b", name: "Host B"}])
        route.page.requestDetails("host-b")
        verify(route.context.baseOverlay.payload.details.indexOf("Requested HDR") < 0)
        route.page.dismissOverlay(route.context.baseOverlay.operationId)
        launcherWindow.retainSessionWarnings("host-a", [])
        route.page.requestDetails("host-a")
        verify(route.context.baseOverlay.payload.details.indexOf("Requested HDR") < 0)
    }

    function test_application_shortcuts_open_native_surfaces() {
        adaptiveShellRequested = true
        createProductionLauncher()
        const route = hostRoute()
        tryVerify(function() { return route.page !== null && launcherWindow.active })
        keyClick(Qt.Key_N, Qt.ControlModifier)
        tryCompare(route.page.baseOverlay, "kind", "add-host")
        route.page.dismissOverlay(route.page.baseOverlay.operationId)
        tryCompare(route.page.baseOverlay, "kind", "none")
        // A modal dialog keeps window shortcuts blocked until its exit finishes.
        wait(400)
        keySequence("Ctrl+,")
        tryCompare(launcherWindow.preferencesPopup, "visible", true)
        keyClick(Qt.Key_Escape)
        tryCompare(launcherWindow.preferencesPopup, "visible", false)
        adaptiveShellRequested = false
    }

    function test_compositor_size_wins_over_the_remembered_size() {
        createLauncher()
        launcherWindow.width = 1500
        launcherWindow.height = 900
        // Longer than the settle delay: what a tiling compositor assigned must stay.
        wait(300)
        compare(launcherWindow.width, 1500)
        compare(launcherWindow.height, 900)
    }

    function test_long_toast_wraps_within_the_window() {
        createLauncher()
        launcherWindow.showToast(new Array(30).join("A lengthy stream adjustment explanation. "))
        const toast = launcherWindow.toastItem
        verify(toast !== null)
        verify(toast.width <= launcherWindow.width - 32)
        verify(toast.height > 44)
    }

    function test_toast_action_runs_once_and_dismisses() {
        createLauncher()
        let runs = 0
        launcherWindow.showToast("Paired with Lounge", "Connect", function() { ++runs })
        const toast = launcherWindow.toastItem
        verify(toast.parent !== null, "a toast without a parent is never drawn")
        tryVerify(function() { return toast.opacity > 0.9 })
        compare(toast.actionText, "Connect")
        findChild(toast, "starlight.toast.action").clicked()
        compare(runs, 1)
        compare(toast.shown, false)
        launcherWindow.showToast("Copied")
        compare(toast.actionText, "", "a plain toast offers nothing to press")
    }

    function test_legacy_host_details_keep_long_adjustments_reviewable() {
        createProductionLauncher()
        const route = hostRoute()
        launcherWindow.retainSessionWarnings("host-a", [new Array(40).join("Stream adjustment\n")])
        route.page.requestDetails("host-a")
        const scroll = findChild(launcherWindow, "starlight.host-details-dialog.scroll")
        verify(scroll !== null)
        tryVerify(function() { return scroll.visible && scroll.contentHeight > scroll.height })
        const dialog = findChild(launcherWindow, "starlight.host-details-dialog")
        verify(dialog.height < launcherWindow.height)
        scroll.contentItem.contentY = scroll.contentHeight - scroll.height
        verify(scroll.contentItem.contentY > 0)
        dialog.close()
    }

    function test_async_diagnostic_updates_only_the_open_error_and_cancels_on_close() {
        createLauncher()
        const dialog = findChild(launcherWindow, "starlight.stream-error-dialog")
        verify(dialog !== null)
        const diagnostic = Qt.createQmlObject('import QtQuick 2.15; QtObject {'
            + 'property int connectivityTestResult: -1; property int cancellations: 0; '
            + 'signal connectionDiagnosticReady(int result); '
            + 'function cancelConnectivityTest() { cancellations += 1 } }', testCase)
        dialog.text = "Primary failure"
        dialog.diagnosticSession = diagnostic
        dialog.open()
        tryVerify(function() { return dialog.visible })
        diagnostic.connectionDiagnosticReady(2)
        verify(dialog.text.indexOf("blocked Internet streaming ports") >= 0)
        const annotated = dialog.text
        diagnostic.connectionDiagnosticReady(2)
        compare(dialog.text, annotated)
        dialog.close()
        tryCompare(dialog, "visible", false)
        tryCompare(diagnostic, "cancellations", 1)
        compare(dialog.diagnosticSession, null)
        diagnostic.connectionDiagnosticReady(2)
        compare(dialog.text, "")
        diagnostic.destroy()
    }

    function test_video_check_timeout_keeps_cancel_and_never_starts_a_live_probe() {
        createProductionLauncher()
        const route = hostRoute()
        const session = qmlSession()
        hostSource.sessionOverride = session
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 0, "Desktop")
        SystemProperties.decoderProbeRunning = true
        route.page.requestConnect("host-a")
        tryVerify(function() { return namedItem("starlight.stream-segue") !== null })
        const segue = namedItem("starlight.stream-segue")
        SystemProperties.decoderProbeError = "Video check timed out; return to hosts."
        tryVerify(function() { return segue.failed })
        compare(segue.lastFailure, SystemProperties.decoderProbeError)
        const retry = findChild(segue, "starlight.stream.retry-video-check")
        verify(retry !== null)
        verify(!retry.enabled)
        verify(launcherWindow.visible)
        const stack = namedItem("starlight.shell.stack")
        launcherWindow.goBack()
        tryVerify(function() { return !stack.busy && stack.currentItem === route })
        verify(segue.abandoned)
        compare(session.cancelPendingStartCalls, 1)
        verify(stackContains(route))
    }

    function test_video_probe_late_success_requires_deliberate_continuation() {
        createProductionLauncher()
        const route = hostRoute()
        const session = qmlSession()
        session.initializeResult = false
        hostSource.sessionOverride = session
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 0, "Desktop")
        SystemProperties.decoderProbeRunning = true
        route.page.requestConnect("host-a")
        tryVerify(function() { return namedItem("starlight.stream-segue") !== null })
        const segue = namedItem("starlight.stream-segue")
        SystemProperties.decoderProbeError = "Video check timed out."
        tryVerify(function() { return segue.decoderCheckFailed })
        SystemProperties.decoderProbeRunning = false
        SystemProperties.decoderProbeError = ""
        SystemProperties.decoderProbeReady = true
        wait(0)
        verify(segue.failed)
        compare(segue.lastFailure, "Video check timed out.")
        const retry = findChild(segue, "starlight.stream.retry-video-check")
        verify(retry.enabled)
        retry.clicked()
        tryVerify(function() { return segue.failed && !segue.decoderCheckFailed })
        verify(segue.lastFailure.indexOf("prepare video") >= 0)
        launcherWindow.goBack()
    }

    function test_failed_local_initialization_keeps_launcher_and_explains_failure() {
        createProductionLauncher()
        const route = hostRoute()
        const session = qmlSession()
        session.initializeResult = false
        hostSource.sessionOverride = session
        hostSource.setDesktopAppId(0)
        hostSource.setRunningApp("host-a", 0, "Desktop")
        SystemProperties.decoderProbeReady = true
        route.page.requestConnect("host-a")
        tryVerify(function() { return namedItem("starlight.stream-segue") !== null })
        const segue = namedItem("starlight.stream-segue")
        tryVerify(function() { return segue.failed })
        verify(segue.lastFailure.length > 0)
        verify(stackContains(route))
        verify(launcherWindow.visible)
        launcherWindow.goBack()
    }

    function test_stream_return_terminal_leaves_the_host_route_recoverable() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, false), route))
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null
        })

        namedItem("starlight.stream-segue").sessionFinished(0)
        verify(stackContains(route))
    }

    function test_back_cancels_a_queued_stream_start() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, false), route))
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null
        })
        const session = namedItem("starlight.stream-segue").session
        compare(session.cancelPendingStartCalls, 0)

        launcherWindow.goBack()
        tryCompare(session, "cancelPendingStartCalls", 1)
    }

    function test_abandoned_stream_ignores_late_start_and_terminal_signals() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, false), route))
        const stack = namedItem("starlight.shell.stack")
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null && !stack.busy
        })
        const segue = namedItem("starlight.stream-segue")
        const session = segue.session
        launcherWindow.goBack()
        tryVerify(function() { return !stack.busy && stack.currentItem === route })
        compare(segue.activated, false)
        verify(segue.abandoned)
        const depth = stack.depth
        verify(launcherWindow.visible)
        session.connectionStarted()
        verify(launcherWindow.visible, "late worker success must not hide the recovered launcher")
        session.sessionFinished(0)
        compare(stack.depth, depth, "late completion must not pop the recovered host route")
        compare(stack.currentItem, route)
        verify(launcherWindow.visible)
        // A completion from the abandoned session also cannot disturb a
        // newer route opened after returning to Hosts.
        route.context.facade.requestOpenSettings()
        verify(launcherWindow.consumeWindowEffect(route.lastEffect, route))
        tryVerify(function() { return !stack.busy && stack.depth === depth + 1 })
        const settings = stack.currentItem
        session.connectionStarted()
        session.sessionFinished(0)
        compare(stack.currentItem, settings)
        compare(stack.depth, depth + 1)
        verify(launcherWindow.visible)
    }

    function test_stream_terminal_delivery_is_one_shot() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, false), route))
        const stack = namedItem("starlight.shell.stack")
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null && !stack.busy
        })
        const segue = namedItem("starlight.stream-segue")
        const session = segue.session
        session.sessionFinished(0)
        tryVerify(function() { return !stack.busy && stack.currentItem === route })
        const depth = stack.depth
        session.sessionFinished(0)
        session.connectionStarted()
        compare(stack.currentItem, route)
        compare(stack.depth, depth)
        verify(launcherWindow.visible)
    }

    function test_resume_effect_preserves_resume_mode() {
        createLauncher()
        const route = hostRoute()
        verify(launcherWindow.windowEffectConsumerOverride === undefined)
        verify(launcherWindow.consumeWindowEffect(launchEffect(route, true), route))
        tryVerify(function() {
            return namedItem("starlight.stream-segue") !== null
        })
        compare(namedItem("starlight.stream-segue").isResume, true)
    }

    function test_pinned_quit_blocks_direct_stack_clear_until_terminal_unpin() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })

        const stack = namedItem("starlight.shell.stack")
        stack.clear()
        verify(stackContains(route))
        verify(namedItem("starlight.quit-segue") !== null)

        namedItem("starlight.quit-segue").quitAppCompleted("terminal failure")
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
                    && !stackContains(route)
        })
    }

    function test_pinned_quit_blocks_direct_stack_replace_until_terminal_unpin() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })

        const replacement = Qt.createQmlObject(
                    "import QtQuick 2.15; Item { objectName: 'deferred-replacement' }",
                    testCase, "deferred-replacement")
        const stack = namedItem("starlight.shell.stack")
        stack.replace(replacement, StackView.Immediate)
        verify(namedItem("starlight.quit-segue") !== null)

        namedItem("starlight.quit-segue").quitAppCompleted("terminal failure")
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
                    && namedItem("deferred-replacement") !== null
        })
    }

    function test_pinned_quit_blocks_direct_stack_pop_until_terminal_unpin() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })

        const stack = namedItem("starlight.shell.stack")
        stack.pop()
        verify(namedItem("starlight.quit-segue") !== null)
        compare(stack.deferredMutations.length, 1)

        namedItem("starlight.quit-segue").quitAppCompleted("terminal failure")
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
                    && stack.deferredMutations.length === 0
        })
    }

    function test_pinned_quit_rejects_direct_stack_push() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route)
        verify(launcherWindow.consumeWindowEffect(effect, route))
        tryVerify(function() {
            return namedItem("starlight.quit-segue") !== null
        })

        const pushedItem = Qt.createQmlObject(
                    "import QtQuick 2.15; Item { objectName: 'rejected-push' }",
                    testCase, "rejected-push")
        const stack = namedItem("starlight.shell.stack")
        stack.push(pushedItem, StackView.Immediate)
        verify(namedItem("starlight.quit-segue") !== null)
        verify(namedItem("rejected-push") === null)

        namedItem("starlight.quit-segue").quitAppCompleted("terminal failure")
        tryVerify(function() {
            return launcherWindow.pinnedQuitRoute === null
                    && namedItem("rejected-push") === null
        })
    }

    function test_failed_quit_lease_push_rolls_back_the_pin() {
        createLauncher()
        const route = hostRoute()
        const stack = namedItem("starlight.shell.stack")
        const invalidSegue = Qt.resolvedUrl("fixtures/missing-quit-segue.qml")

        // A missing source is non-null, but StackView rejects it. Exercise the
        // real native rejection rather than an injected wrapper seam.
        compare(stack.beginQuitLease(route, invalidSegue), null)
        compare(stack.pinnedOrigin, null)
        compare(launcherWindow.pinnedQuitRoute, null)
        verify(stackContains(route))
        compare(route.context.quitRoutePinned, false)
        compare(hostSource.sessionReleaseCount, 0)
    }

    function test_acknowledged_quit_lease_push_failure_is_terminal() {
        createLauncher()
        const route = hostRoute()
        const effect = quitEffect(route, false)
        quitSegueComponentOverride = {
            status: Component.Ready,
            createObject: function() {
                return Qt.resolvedUrl("fixtures/missing-quit-segue.qml")
            },
            errorString: function() { return "test quit lease source failure" }
        }

        // The normal effect path acknowledges first; native StackView then
        // rejects the non-Item segue. Ownership must be released exactly once.
        verify(!launcherWindow.consumeWindowEffect(effect, route))
        compare(hostSource.sessionReleaseCount, 1)
        compare(Object.keys(route.context.pendingQuitOperations).length, 0)
        compare(launcherWindow.pinnedQuitRoute, null)
        compare(namedItem("starlight.shell.stack").pinnedOrigin, null)
        compare(route.context.quitRoutePinned, false)
        verify(stackContains(route))
        compare(namedItem("starlight.quit-segue"), null)
    }

    function test_direct_pinned_origin_destruction_is_rejected_without_stranding() {
        const route = startProductionQuit()
        var rejected = false
        try {
            route.destroy()
        } catch (error) {
            rejected = true
        }

        verify(rejected)
        compare(launcherWindow.pinnedQuitRoute, route)
        verify(stackContains(route))
        verify(namedItem("starlight.quit-segue") !== null)

        namedItem("starlight.quit-segue").quitAppCompleted("terminal failure")
        tryVerify(function() { return launcherWindow.pinnedQuitRoute === null })
        verify(stackContains(route))
    }

    function test_unpinning_an_inactive_pinned_context_disposes_route_state() {
        createLauncher()
        const route = hostRoute()
        route.context.facade.beginPairing()
        compare(route.context.baseOverlay.kind, "pairing")

        route.context.setQuitRoutePinned(true)
        route.context.routeActive = false
        compare(route.context.baseOverlay.kind, "pairing")
        route.context.setQuitRoutePinned(false)

        compare(route.context.baseOverlay.kind, "none")
        compare(Object.keys(route.context.pendingQuitOperations).length, 0)
    }

    function test_destroyed_route_churn_prunes_consumed_effect_records() {
        createProductionLauncher()
        for (let iteration = 0; iteration < 3; ++iteration) {
            const route = hostRoute()
            route.context.facade.requestOpenSettings()
            tryVerify(function() {
                return namedItem("starlight.settings.context") !== null
            })
            namedItem("starlight.shell.stack").clear()
            tryVerify(function() {
                return launcherWindow.consumedWindowEffects.length === 0
            })
            if (iteration < 2) {
                namedItem("starlight.shell.stack").push(productionInitialView,
                                                         ({ hostSource: hostSource }))
                tryVerify(function() { return hostRoute() !== null })
            }
        }
        compare(launcherWindow.consumedWindowEffects.length, 0)
    }
}
