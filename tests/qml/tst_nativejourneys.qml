import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import Starlight.Test 1.0
import Starlight 1.0
import ComputerManager 1.0
import StreamingPreferences 1.0
import "../../app/gui/adaptive" as Adaptive

// Assembled native routes with real contexts, facade commands and key events.
// These checks cover QML semantics; desktop AT-SPI/speech needs a real session.
TestCase {
    id: testCase
    name: "NativeJourneys"
    when: windowShown
    property bool gnomeDesktopAdapterActive: false
    property bool plasmaDesktopAdapterActive: false
    property var stack: null
    property var route: null
    property var harness: null
    property string platform: ""
    property var window: QtObject {
        property int copied: 0
        property bool pollingActive: false
        function copyDiagnosticSummary() { copied += 1; return true }
    }

    Component { id: stackComponent; StackView { anchors.fill: parent } }
    Component {
        id: hostComponent
        Adaptive.HostPageRoute {
            shellRouteId: "adaptive-host"
            hostSource: TestHostSource { }
        }
    }
    Component {
        id: settingsComponent
        Adaptive.SettingsPageRoute { shellRouteId: "adaptive-settings" }
    }

    function init() {
        harness = testCase.Window.window
        harness.width = 1100
        harness.height = 500
        harness.requestActivate()
        tryCompare(harness, "active", true)
        window.copied = 0
        Appearance.reset()
        ComputerManager.reset()
        StreamingPreferences.setLastSelectedHost("")
    }
    function cleanup() {
        if (stack) { stack.clear(); stack.destroy(); wait(0) }
        route = null
        stack = null
        gnomeDesktopAdapterActive = false
        plasmaDesktopAdapterActive = false
        Appearance.reset()
    }
    function openRoute(desktop, settings) {
        if (desktop === "plasma" && !kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        platform = desktop
        gnomeDesktopAdapterActive = desktop === "gnome"
        plasmaDesktopAdapterActive = desktop === "plasma"
        stack = stackComponent.createObject(harness.contentItem)
        verify(stack !== null)
        route = stack.push(settings ? settingsComponent : hostComponent)
        verify(route !== null)
        tryCompare(stack, "busy", false)
        verify(desktop === "gnome" ? route.nativeGnomeSurface : route.nativePlasmaSurface,
               "the assembled route must select the requested native surface")
        if (!settings) {
            route.computerModel.replaceHosts([{uuid: "alpha", name: "Alpha", paired: true,
                                               online: true, serverSupported: true,
                                               serverSoftware: "Sunshine"}])
            tryCompare(route.page, "selectedHostUuid", "alpha")
        }
        waitForRendering(route)
        wait(0)
        return route.legacySurface
    }
    function control(name) {
        const item = findChild(route, name)
        verify(item !== null, name)
        return item
    }
    function nativeControl(suffix) { return control(platform + "-host-dialog-" + suffix) }
    function dialogLayer() { return control(platform + "-host-dialog-layer") }

    function test_long_stream_adjustments_remain_scrollable_data() {
        return [{tag: "gnome", desktop: "gnome"}, {tag: "plasma", desktop: "plasma"}]
    }
    function test_long_stream_adjustments_remain_scrollable(data) {
        openRoute(data.desktop, false)
        route.context.sessionWarningsByHost = {alpha: new Array(25).join("Requested HDR; using SDR.\n").split("\n")}
        route.page.requestDetails("alpha")
        const scroll = control(data.desktop + "-host-dialog-details-scroll")
        tryVerify(function() { return scroll.visible && scroll.contentHeight > scroll.height })
        const accept = nativeControl("accept")
        const point = accept.mapToItem(harness.contentItem, 0, 0)
        verify(point.y >= 0 && point.y + accept.height <= harness.height)
        scroll.contentItem.contentY = scroll.contentHeight - scroll.height
        verify(scroll.contentItem.contentY > 0)
        accept.clicked()
        tryCompare(route.page.baseOverlay, "kind", "none")
    }
    function assertFocusVisible(viewport) {
        const focused = harness.activeFocusItem
        verify(focused !== null)
        let ancestor = focused
        while (ancestor && ancestor !== viewport.contentItem) ancestor = ancestor.parent
        verify(ancestor !== null, "Tab must remain in the scrolling form, got " + focused.objectName)
        tryVerify(function() {
            const point = focused.mapToItem(viewport, 0, 0)
            return point.y >= -0.5 && point.y + focused.height <= viewport.height + 0.5
        }, 1000, focused.objectName + " must be inside the viewport")
    }

    function test_tab_reveals_focused_controls_data() {
        return [{ tag: "gnome-hosts", desktop: "gnome", settings: false },
                { tag: "plasma-hosts", desktop: "plasma", settings: false },
                { tag: "gnome-settings", desktop: "gnome", settings: true },
                { tag: "plasma-settings", desktop: "plasma", settings: true }]
    }
    function test_tab_reveals_focused_controls(data) {
        const surface = openRoute(data.desktop, data.settings)
        const viewportName = data.desktop === "gnome"
                ? (data.settings ? "gnome-settings-flickable" : "gnome-hosts-content-flickable")
                : (data.settings ? "plasma-settings-viewport" : "plasma-hosts-viewport")
        const viewport = control(viewportName)
        const first = control(data.settings ? "starlight.settings.window-mode"
                                             : "starlight.hosts.stream-mode")
        const last = control(data.settings ? "starlight.settings.configuration-warnings"
                                            : "starlight.hosts.restore-last-working")
        first.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(first, "activeFocus", true)
        assertFocusVisible(viewport)
        let steps = 0
        while (!last.activeFocus && steps < 60) {
            keyClick(Qt.Key_Tab)
            wait(0)
            assertFocusVisible(viewport)
            steps += 1
        }
        verify(last.activeFocus, "forward Tab reaches the final form control")
        verify(steps > (data.settings ? 5 : 1))
        if (data.settings)
            verify(viewport.contentY > 100, "keyboard navigation must actually scroll the long settings form")
        const bottom = viewport.contentY
        while (!first.activeFocus && steps < 120) {
            keyClick(Qt.Key_Tab, Qt.ShiftModifier)
            wait(0)
            assertFocusVisible(viewport)
            steps += 1
        }
        verify(first.activeFocus, "reverse Tab returns to the first form control")
        if (data.settings)
            verify(viewport.contentY < bottom, "reverse navigation must reveal earlier rows")
    }

    function test_geometry_reflow_reveals_existing_focus() {
        const surface = openRoute("gnome", true)
        waitForRendering(surface)
        const viewport = control("gnome-settings-flickable")
        const focused = control("starlight.settings.capture-system-keys-mode")
        focused.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(focused, "activeFocus", true)
        assertFocusVisible(viewport)
        const initialHeight = viewport.contentHeight
        // A geometry change after the focus callback has run must reveal the
        // same focused control again; no second focus event comes to rescue it.
        Appearance.fontScale = 1.6
        waitForRendering(surface)
        tryVerify(function() { return viewport.contentHeight > initialHeight })
        compare(harness.activeFocusItem, focused)
        assertFocusVisible(viewport)
    }

    function test_destructive_dialogs_cancel_on_initial_activation_data() {
        return [{ tag: "gnome-remove", desktop: "gnome", remove: true },
                { tag: "plasma-remove", desktop: "plasma", remove: true },
                { tag: "gnome-quit", desktop: "gnome", remove: false },
                { tag: "plasma-quit", desktop: "plasma", remove: false }]
    }
    function test_destructive_dialogs_cancel_on_initial_activation(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        if (!data.remove) source.setRunningApp("alpha", 77, "Editor")
        const trigger = control(data.remove ? "starlight.hosts.delete" : "starlight.hosts.connect")
        if (data.remove) {
            control(data.desktop === "gnome" ? "gnome-hosts-host-menu-button"
                                              : "plasma-host-actions-button").clicked()
            const menu = control(data.desktop === "gnome" ? "gnome-hosts-host-menu"
                                                           : "plasma-host-actions-menu")
            tryCompare(menu, "opened", true)
        }
        trigger.forceActiveFocus()
        tryCompare(trigger, "activeFocus", true)
        keyClick(Qt.Key_Space)
        const kind = data.remove ? "confirm-delete" : "confirm-quit"
        tryCompare(dialogLayer(), "visibleOverlayKind", kind)
        const popup = control("starlight.hosts." + kind)
        tryCompare(popup, "opened", true)
        const cancel = nativeControl("reject")
        const accept = nativeControl("accept")
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Return)
        tryCompare(route.page.baseOverlay, "kind", "none")
        compare(source.rowCount(), 1)
        compare(source.quitDispatchCount, 0)
        // The context clears before the popup's exit transition finishes.
        // Reopening during that transition can retain the old Cancel focus
        // and let the next opened callback race the explicit Tab below.
        tryCompare(popup, "visible", false)
        tryCompare(cancel, "activeFocus", false)
        if (data.remove) route.page.requestDelete("alpha")
        else route.page.requestConnect("alpha")
        tryCompare(popup, "opened", true)
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Tab, data.desktop === "plasma" ? Qt.ShiftModifier : Qt.NoModifier)
        tryCompare(accept, "activeFocus", true)
        keyClick(Qt.Key_Space)
        tryCompare(route.page.baseOverlay, "kind", "none")
        if (data.remove) compare(source.rowCount(), 0)
        else {
            // A quit launch is a window effect; the context must emit it once
            // and await the window owner rather than dispatch it twice.
            compare(Object.keys(route.context.pendingQuitOperations).length, 1)
        }
    }

    function test_native_accessible_pair_error_retry_and_settings_data() {
        return [{ tag: "gnome", desktop: "gnome" }, { tag: "plasma", desktop: "plasma" }]
    }
    function test_native_accessible_pair_error_retry_and_settings(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        const originalFocus = control("starlight.hosts.connect")
        source.setDesktopAppId(-1)
        originalFocus.forceActiveFocus()
        originalFocus.Accessible.pressAction()
        tryCompare(dialogLayer(), "visibleOverlayKind", "error")
        const error = control("starlight.hosts.error")
        compare(error.accessibleItem.Accessible.role, Accessible.Dialog)
        const description = error.accessibleItem.Accessible.description
        verify(description.indexOf("Alpha") >= 0)
        verify(description.indexOf("named Desktop") >= 0)
        verify(description.indexOf("connect again") >= 0)
        compare(Object.keys(route.context.pendingEffects).length, 0, "no launch on missing Desktop")
        tryCompare(nativeControl("reject"), "activeFocus", true)
        keyClick(Qt.Key_Space)
        tryCompare(originalFocus, "activeFocus", true)
        source.setDesktopAppId(0)
        originalFocus.Accessible.pressAction()
        compare(Object.keys(route.context.pendingEffects).length, 1, "corrected host can retry")
        route.context.invalidatePendingEffects()

        route.page.beginPairing()
        tryCompare(dialogLayer(), "visibleOverlayKind", "pairing")
        const pairing = control("starlight.hosts.pairing")
        compare(pairing.accessibleItem.Accessible.role, Accessible.Dialog)
        verify(pairing.accessibleItem.Accessible.description.indexOf(route.page.baseOverlay.payload.pin) >= 0)
        const attempt = route.page.baseOverlay.payload.attemptId
        nativeControl("reject").Accessible.pressAction()
        tryCompare(route.page.baseOverlay, "kind", "none")
        compare(ComputerManager.cancelledPairingAttempts, [attempt])
        source.completePairingAttempt(attempt, "late cancelled failure")
        compare(route.page.interruptOverlay.kind, "none")

        route.page.requestAddHostOverlay()
        const field = nativeControl("primary-field")
        tryCompare(field, "activeFocus", true)
        field.text = "192.0.2.55"
        route.context.updateBaseOverlay(route.page.baseOverlay.operationId,
                                       {address: "192.0.2.55", error: "Cannot reach this host"})
        const addDialog = control("starlight.hosts.add-dialog")
        verify(addDialog.accessibleItem.Accessible.description.indexOf("Cannot reach") >= 0)
        compare(field.text, "192.0.2.55")
        nativeControl("reject").Accessible.pressAction()
        tryCompare(originalFocus, "activeFocus", true)

        route = stack.push(settingsComponent)
        tryCompare(stack, "busy", false)
        verify(data.desktop === "gnome" ? route.nativeGnomeSurface : route.nativePlasmaSurface)
        const capture = control("starlight.settings.capture-system-keys")
        verify(capture.Accessible.description.indexOf("Ctrl+Alt+Shift+O") >= 0)
        verify(capture.Accessible.description.indexOf("Ctrl+Alt+Shift+Z") >= 0)
        verify(capture.Accessible.description.indexOf("Ctrl+Alt+Shift+Q") >= 0)
        compare(control("starlight.settings.language").Accessible.name, "Language")
    }

    function test_shortcuts_and_diagnostics_available_without_hosts_data() {
        return [{ tag: "gnome", desktop: "gnome" }, { tag: "plasma", desktop: "plasma" }]
    }
    function test_shortcuts_and_diagnostics_available_without_hosts(data) {
        openRoute(data.desktop, false)
        route.computerModel.replaceHosts([])
        control(data.desktop === "gnome" ? "gnome-hosts-app-menu-button"
                                          : "plasma-app-menu-button").clicked()
        const menu = control(data.desktop === "gnome" ? "gnome-hosts-app-menu" : "plasma-app-menu")
        tryCompare(menu, "opened", true)
        const shortcut = control(data.desktop + "-keyboard-shortcuts-action")
        verify(shortcut.enabled)
        shortcut.forceActiveFocus()
        tryCompare(shortcut, "activeFocus", true)
        keyClick(Qt.Key_Space)
        const dialog = control(data.desktop + "-keyboard-shortcuts-dialog")
        tryCompare(dialog, "opened", true)
        verify(dialog.accessibleDescription.indexOf("Q — Disconnect") >= 0)
        verify(dialog.accessibleDescription.indexOf("Z — Release") >= 0)
        verify(dialog.accessibleDescription.indexOf("V — Type clipboard") >= 0)
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "visible", false)
        control(data.desktop === "gnome" ? "gnome-hosts-app-menu-button"
                                          : "plasma-app-menu-button").clicked()
        tryCompare(menu, "opened", true)
        const copy = control(data.desktop + "-diagnostics-action")
        verify(copy.enabled)
        copy.forceActiveFocus()
        tryCompare(copy, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(window.copied, 1)
    }

    function nativeDesktopCases() {
        return [{tag: "gnome", desktop: "gnome"}, {tag: "plasma", desktop: "plasma"}]
    }
    function test_host_list_keys_select_and_activate_data() {
        return [{tag: "gnome-short", desktop: "gnome", count: 3},
                {tag: "gnome-scroll", desktop: "gnome", count: 30},
                {tag: "plasma-short", desktop: "plasma", count: 3},
                {tag: "plasma-scroll", desktop: "plasma", count: 30}]
    }
    function test_host_list_keys_select_and_activate(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        let hosts = []
        for (let i = 0; i < data.count; ++i)
            hosts.push({uuid: "host-" + i, name: "Host " + i, paired: true,
                        online: true, serverSupported: true})
        source.replaceHosts(hosts)
        const list = control("starlight.hosts.list")
        list.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(list, "activeFocus", true)
        keyClick(Qt.Key_Down)
        tryCompare(route.page, "selectedHostUuid", "host-1")
        compare(list.currentIndex, 1)
        keyClick(Qt.Key_End)
        tryCompare(route.page, "selectedHostUuid", "host-" + (data.count - 1))
        verify(list.currentItem.y >= list.contentY)
        verify(list.currentItem.y + list.currentItem.height <= list.contentY + list.height + 1)
        keyClick(Qt.Key_Up)
        tryCompare(route.page, "selectedHostUuid", "host-" + (data.count - 2))
        keyClick(Qt.Key_Home)
        tryCompare(route.page, "selectedHostUuid", "host-0")
        let launch = null
        route.windowEffectRequested.connect(function(effect) { if (effect.kind === "launch") launch = effect })
        keyClick(Qt.Key_Return)
        tryVerify(function() { return launch !== null })
        compare(launch.payload.hostUuid, "host-0")
        compare(source.sessionDispatchCount, 1)
    }
    function test_host_switch_reveals_identity_data() { return nativeDesktopCases() }
    function test_host_switch_reveals_identity(data) {
        const surface = openRoute(data.desktop, false)
        route.computerModel.appendHost({uuid: "beta", name: "Beta", paired: true,
                                       online: true, serverSupported: true})
        const viewport = control(data.desktop === "gnome" ? "gnome-hosts-content-flickable"
                                                           : "plasma-hosts-viewport")
        viewport.contentY = 300
        verify(viewport.contentY > 0)
        tryVerify(function() { return findChild(route, "starlight.hosts.item.beta") !== null })
        mouseClick(control("starlight.hosts.item.beta"))
        tryCompare(route.page, "selectedHostUuid", "beta")
        tryCompare(viewport, "contentY", 0)
        viewport.contentY = 100
        route.page.setViewScale(route.page.hostPreferenceSnapshot.viewScale === "actual" ? "fit" : "actual")
        compare(viewport.contentY, 100, "preference edits must preserve scroll position")
    }
    function test_dialog_commit_survives_other_host_reorder_data() { return nativeDesktopCases() }
    function test_dialog_commit_survives_other_host_reorder(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        source.appendHost({uuid: "beta", name: "Beta", paired: true, online: true})
        route.page.requestRename("alpha")
        const field = nativeControl("primary-field")
        tryCompare(field, "activeFocus", true)
        field.text = "Renamed Alpha"
        source.reorderHosts(["beta", "alpha"])
        keyClick(Qt.Key_Return)
        tryCompare(route.page.baseOverlay, "kind", "none")
        compare(source.nameForIndex(1), "Renamed Alpha")
        compare(source.nameForIndex(0), "Beta")
        route.page.requestCustomResolution("alpha")
        nativeControl("primary-field").text = "1600"
        nativeControl("secondary-field").text = "900"
        source.reorderHosts(["alpha", "beta"])
        nativeControl("accept").clicked()
        tryCompare(route.page.baseOverlay, "kind", "none")
        compare(route.page.hostPreferenceSnapshot.width, 1600)
        route.page.requestDelete("alpha")
        source.reorderHosts(["beta", "alpha"])
        nativeControl("accept").clicked()
        tryCompare(route.page.baseOverlay, "kind", "none")
        compare(source.rowCount(), 1)
        compare(source.uuidForIndex(0), "beta")
    }
    function test_replaced_dialog_target_is_explained_without_mutation_data() { return nativeDesktopCases() }
    function test_replaced_dialog_target_is_explained_without_mutation(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        route.page.requestRename("alpha")
        nativeControl("primary-field").text = "Must not apply"
        source.replaceHosts([{uuid: "alpha", name: "Replacement", online: true, paired: true}])
        nativeControl("accept").clicked()
        tryCompare(route.page.baseOverlay, "kind", "none")
        tryCompare(route.page.interruptOverlay, "kind", "error")
        compare(source.nameForIndex(0), "Replacement")
    }
    function test_global_port_test_is_available_without_hosts_data() { return nativeDesktopCases() }
    function test_global_port_test_is_available_without_hosts(data) {
        openRoute(data.desktop, false)
        route.computerModel.replaceHosts([])
        const menuButton = control(data.desktop === "gnome" ? "gnome-hosts-app-menu-button"
                                                             : "plasma-app-menu-button")
        verify(menuButton.visible)
        menuButton.clicked()
        const menu = control(data.desktop === "gnome" ? "gnome-hosts-app-menu" : "plasma-app-menu")
        tryCompare(menu, "opened", true)
        const action = control("starlight.hosts.connection-test")
        verify(action.enabled)
        verify(action.text.indexOf("Internet") >= 0)
        action.forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(route.page.baseOverlay, "kind", "connection-test")
        compare(route.computerModel.connectionTestDispatchCount, 1)
        keyClick(Qt.Key_Escape)
        tryCompare(route.page.baseOverlay, "kind", "none")
    }
    function test_reduced_motion_settles_host_immediately_data() {
        return [{ tag: "gnome", desktop: "gnome" }, { tag: "plasma", desktop: "plasma" }]
    }
    function test_reduced_motion_settles_host_immediately(data) {
        openRoute(data.desktop, false)
        route.computerModel.appendHost({uuid: "beta", name: "Beta", paired: true, online: true})
        Appearance.reducedMotion = true
        tryVerify(function() { return findChild(route, "starlight.hosts.item.beta") !== null })
        mouseClick(control("starlight.hosts.item.beta"))
        // The host is usable in its final position in the same event turn.
        compare(route.page.selectedHostUuid, "beta")
        compare(control(data.desktop + "-host-identity").opacity, 1)
    }

    function test_host_list_keyboard_pairs_and_wakes_data() { return nativeDesktopCases() }
    function test_host_list_keyboard_pairs_and_wakes(data) {
        openRoute(data.desktop, false)
        const source = route.computerModel
        source.replaceHosts([{uuid: "new", name: "New", online: true, paired: false},
                             {uuid: "sleeping", name: "Sleeping", online: false,
                              paired: true, wakeable: true}])
        const list = control("starlight.hosts.list")
        list.forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(route.page.baseOverlay, "kind", "pairing")
        compare(source.pairDispatchCount, 1)
        keyClick(Qt.Key_Escape)
        tryCompare(route.page.baseOverlay, "kind", "none")
        list.forceActiveFocus()
        keyClick(Qt.Key_Down)
        tryCompare(route.page, "selectedHostUuid", "sleeping")
        keyClick(Qt.Key_Return)
        compare(source.wakeDispatchCount, 1)
        compare(source.sessionDispatchCount, 0)
    }
}
