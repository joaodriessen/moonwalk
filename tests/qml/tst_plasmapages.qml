import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import Starlight 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PlasmaPages"
    when: windowShown

    property var component: null
    property var root: null
    property var page: null
    property var gateWindow: null
    property var contract: TestCompat.readJson("qrc:/tests/data/page-contract-v1.json")

    Component {
        id: gateWindowComponent

        ApplicationWindow {
            width: 1200
            height: 820
            visible: true
        }
    }

    Component {
        id: nativeButtonComponent

        Button { }
    }

    function initTestCase() {
        if (!kirigamiAvailable)
            return
        component = Qt.createComponent(Qt.resolvedUrl(
                                           "../../app/gui/adaptive/plasma/PlasmaHostsPage.qml"))
    }

    function init() {
        if (!kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        var facadeComponent = Qt.createComponent(Qt.resolvedUrl(
                                                "fixtures/GnomeHostFacade.qml"))
        verify(facadeComponent.status === Component.Ready,
               facadeComponent.errorString())
        page = facadeComponent.createObject(testCase)
        verify(page !== null)
        gateWindow = gateWindowComponent.createObject(testCase)
        verify(gateWindow !== null)
        recreateRoot()
    }

    function cleanup() {
        if (page && page.interruptOverlay.kind !== "none")
            page.dismissOverlay(page.interruptOverlay.operationId)
        if (page && page.baseOverlay.kind !== "none")
            page.dismissOverlay(page.baseOverlay.operationId)
        wait(0)
        if (root) root.destroy()
        if (page) page.destroy()
        if (gateWindow) {
            gateWindow.close()
            gateWindow.destroy()
        }
        // Dialog close and QML destruction both defer focus-chain work. Let
        // those callbacks complete before the next case creates a new page.
        wait(0)
        root = null
        page = null
        gateWindow = null
        PlasmaAppearance.reset()
    }

    function test_livePaletteSnapshotUpdatesExistingPage() {
        var originalRoot = root
        PlasmaAppearance.active = ({
            window: "#17212b", windowText: "#dbe7f3", base: "#223344",
            alternateBase: "#2a4054", text: "#cbd9e6",
            placeholderText: "#708090", button: "#30485d",
            buttonText: "#e8f0f7", highlight: "#8a5cff",
            highlightedText: "#130a24", toolTipBase: "#101820",
            toolTipText: "#f0f7ff", light: "#415d73", midlight: "#385166",
            mid: "#263b4d", dark: "#132331", shadow: "#05090d"
        })
        PlasmaAppearance.inactive = PlasmaAppearance.active
        PlasmaAppearance.disabled = PlasmaAppearance.active
        PlasmaAppearance.ready = true
        PlasmaAppearance.revision += 1
        wait(0)

        compare(root, originalRoot)
        var grounds = TestCompat.walkQObjectGraph(root).filter(function(object) {
            return object.objectName === "plasmaPageGround"
        })
        compare(grounds.length, 1)
        compare(grounds[0].color, "#17212b")
    }

    function normalizedIds() {
        return TestCompat.semanticIds(root).map(function(id) {
            return id.indexOf("hosts.item.") === 0 ? "hosts.item.<uuid>" : id
        }).sort()
    }

    function recreateRoot() {
        root = component.createObject(gateWindow.contentItem,
                                      { page: page, width: 1100, height: 760 })
        verify(root !== null, component.errorString())
        wait(0)
    }

    function createPreexistingOverlayRoot(baseKind, interruptKind) {
        var facadeComponent = Qt.createComponent(Qt.resolvedUrl(
                                                "fixtures/GnomeHostFacade.qml"))
        verify(facadeComponent.status === Component.Ready,
               facadeComponent.errorString())
        var overlayPage = facadeComponent.createObject(testCase)
        verify(overlayPage !== null)
        overlayPage.overlay(baseKind, "host-a")
        if (interruptKind !== "none") {
            overlayPage.interruptOverlay = ({ kind: interruptKind,
                                               operationId: 88,
                                               hostUuid: "host-a",
                                               payload: ({ message: "Interrupted" }) })
        }
        var overlayWindow = gateWindowComponent.createObject(testCase)
        verify(overlayWindow !== null)
        var overlayRoot = component.createObject(overlayWindow.contentItem,
                                                 { page: overlayPage,
                                                   width: 1100, height: 760 })
        verify(overlayRoot !== null, component.errorString())
        wait(0)
        return ({ page: overlayPage, root: overlayRoot, window: overlayWindow })
    }

    function destroyPreexistingOverlayRoot(instance) {
        if (instance.page.interruptOverlay.kind !== "none")
            instance.page.dismissOverlay(instance.page.interruptOverlay.operationId)
        if (instance.page.baseOverlay.kind !== "none")
            instance.page.dismissOverlay(instance.page.baseOverlay.operationId)
        wait(0)
        instance.root.destroy()
        instance.page.destroy()
        instance.window.close()
        instance.window.destroy()
        wait(0)
    }

    function canonicalWidgetStyle(styleName) {
        return styleName.toString().split(":")[0].trim().toLowerCase()
    }

    function overlayInventoryIds() {
        var ids = normalizedIds()
        ;["confirm-quit", "custom-resolution", "custom-frame-rate", "details"].forEach(
                    function(kind) {
            page.overlay(kind, "host-a")
            wait(0)
            normalizedIds().forEach(function(id) {
                if (ids.indexOf(id) < 0) ids.push(id)
            })
        })
        page.dismissOverlay(page.baseOverlay.operationId)
        return ids.sort()
    }

    function hostContractIds() {
        return contract.rows.filter(function(row) {
            return row.id.indexOf("hosts.") === 0
        }).map(function(row) { return row.id }).sort()
    }

    function control(id) {
        return findChild(root, "starlight." + id)
    }

    function objectNamed(name) {
        return findChild(root, name)
    }

    function objectNamedIn(item, name) {
        return findChild(item, name)
    }

    function activeContractFocusTargets() {
        return TestCompat.walkQObjectGraph(root).filter(function(object) {
            return object.activeFocus === true && object.activeFocusOnTab !== undefined
                    && object.enabled === true
                    && object.visible === true
                    && (object.objectName.indexOf("starlight.") === 0
                        || object.objectName.indexOf("plasma-host-dialog-") === 0
                        || object.objectName === "plasma-app-menu-button")
        })
    }

    function test_plasma_hosts_matches_contract_and_active_style() {
        verify(component.status === Component.Ready, component.errorString())
        compare(overlayInventoryIds(), hostContractIds())
        // Style-identity assertions are enforced by the focused KDE rows,
        // which declare the expected widget style; the style-agnostic
        // aggregate (Material) cannot satisfy them and skips this block.
        if (plasmaWidgetStyleExpected.length > 0) {
            var primaryAction = control("hosts.connect")
            var nativeButton = nativeButtonComponent.createObject(gateWindow.contentItem)
            verify(nativeButton !== null)
            verify(primaryAction.background !== null,
                   "the page action must retain the selected native button delegate")
            verify(typeof primaryAction.background.styleName !== "undefined",
                   "the page action must come from the active KDE Controls style")
            compare(canonicalWidgetStyle(primaryAction.background.styleName),
                    canonicalWidgetStyle(plasmaWidgetStyleExpected))
            compare(primaryAction.leftPadding, nativeButton.leftPadding)
            compare(primaryAction.rightPadding, nativeButton.rightPadding)
            compare(primaryAction.topPadding, nativeButton.topPadding)
            compare(primaryAction.bottomPadding, nativeButton.bottomPadding)
            nativeButton.destroy()
        }
    }

    function test_about_surface_opens_the_native_about_dialog() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                    { starlightVersionString: "0.3.0-alpha" })
        var about = control("hosts.about")
        verify(about !== null,
               "the footer version line must expose the About surface")
        about.clicked()
        var dialog = objectNamed("plasma-about-dialog")
        verify(dialog !== null)
        tryCompare(dialog, "opened", true)
        verify(dialog.applicationName.length > 0)
        compare(dialog.applicationVersion, "0.3.0-alpha")
        dialog.close()
        tryCompare(dialog, "opened", false)
    }

    function test_narrow_reflow_preserves_context_and_operation() {
        root.width = 1100
        page.requestRename("host-a")
        var operation = page.baseOverlay.operationId
        root.width = 520
        compare(page.baseOverlay.operationId, operation)
        compare(root.narrow, true)
    }

    function test_actions_forward_named_facade_commands() {
        var before = page.commandCount
        control("hosts.add-host").clicked()
        control("hosts.pair").clicked()
        control("hosts.connection-test").clicked()
        control("hosts.details").clicked()
        verify(page.commandCount >= before + 4)
    }

    function test_host_overview_prioritizes_connection_and_groups_preferences() {
        var primary = control("hosts.connect")
        verify(primary !== null)
        verify(primary.width >= 140, "Connect should be an obvious primary action")

        var menuButton = objectNamed("plasma-host-actions-button")
        var menu = objectNamed("plasma-host-actions-menu")
        verify(menuButton !== null && menuButton.visible)
        verify(menuButton.width >= 40 && menuButton.height >= 40,
               "host actions need a usable pointer target")
        verify(menu !== null)
        compare(objectNamed("plasma-host-toolbar"), null,
                "the host name should appear only in the overview, not a second header")
        var primaryPosition = primary.mapToItem(root, 0, 0)
        var menuPosition = menuButton.mapToItem(root, 0, 0)
        verify(menuPosition.x > primaryPosition.x + primary.width - 1,
               "host actions should sit beside the primary action")
        verify(Math.abs(menuPosition.y - primaryPosition.y) < 12,
               "host actions should share the primary action row")
        menuButton.clicked()
        tryCompare(menu, "opened", true)
        ;["hosts.connection-test", "hosts.details", "hosts.rename",
          "hosts.delete"].forEach(function(id) {
            verify(control(id) !== null, id + " must remain available")
        })
        menu.close()

        var visibleAddressLabels = TestCompat.walkQObjectGraph(root).filter(
                    function(object) {
                        return object.text === page.selectedHostSnapshot.address
                                && object.visible === true
                    })
        compare(visibleAddressLabels.length, 0,
                "connection details belong behind View Details")

        compare(objectNamed("plasma-host-preferences").objectName, "plasma-host-preferences")
    }

    function test_narrow_host_view_keeps_actions_accessible() {
        root.width = 520
        root.selectHost("host-a")
        wait(0)
        compare(root.narrow, true)
        compare(root.narrowShowsDetails, true)
        var menuButton = objectNamed("plasma-host-actions-button")
        verify(menuButton.visible && menuButton.enabled)
        var backButtons = TestCompat.walkQObjectGraph(root).filter(function(object) {
            return object.accessibleName === qsTranslate("PcView", "Back")
                    && object.visible === true
        })
        compare(backButtons.length, 1)
        menuButton.clicked()
        tryCompare(objectNamed("plasma-host-actions-menu"), "opened", true)
    }

    function test_dialog_layer_dispatches_and_restores_focus() {
        var addHost = control("hosts.add-host")
        addHost.forceActiveFocus()
        addHost.clicked()
        var dialogs = objectNamed("plasma-host-dialog-layer")
        verify(dialogs !== null)
        tryCompare(dialogs, "visibleOverlayKind", "add-host")
        var primary = objectNamed("plasma-host-dialog-primary-field")
        var accept = objectNamed("plasma-host-dialog-accept")
        tryCompare(primary, "activeFocus", true)
        primary.text = "192.0.2.11"
        accept.clicked()
        compare(page.lastCommand, "submitAddHost")
        compare(page.lastArguments, ["192.0.2.11"])
        tryCompare(addHost, "activeFocus", true)
        compare(activeContractFocusTargets().length, 1)
    }

    function test_add_host_requires_address_and_shows_retry_error() {
        control("hosts.add-host").clicked()
        const primary = objectNamed("plasma-host-dialog-primary-field")
        const accept = objectNamed("plasma-host-dialog-accept")
        const error = objectNamed("plasma-host-dialog-add-error")
        verify(primary !== null && accept !== null && error !== null)
        compare(accept.enabled, false)
        primary.text = "192.0.2.10"
        compare(accept.enabled, true)
        page.baseOverlay = ({ kind: "add-host", operationId: 41, hostUuid: "",
                              payload: ({ address: "192.0.2.10", error: "Could not reach this computer" }) })
        compare(error.visible, true)
        compare(primary.text, "192.0.2.10")
        primary.text = "192.0.2.11"
        compare(error.visible, false)
    }

    function test_native_overlay_copy_and_edit_validation() {
        var dialogs = objectNamed("plasma-host-dialog-layer")
        page.overlay("confirm-quit", "host-a", ({ runningAppLabel: "Editor" }))
        verify(dialogs.state.body.indexOf("Editor") >= 0)
        verify(dialogs.state.body.indexOf("unsaved") >= 0)
        page.dismissOverlay(41)
        page.overlay("connection-test", "", ({ result: -1, dispatched: true }))
        verify(dialogs.state.body.indexOf("Checking whether this network") >= 0)
        page.dismissOverlay(41)
        page.overlay("rename-host", "host-a", ({ name: "Apollo" }))
        var primary = objectNamed("plasma-host-dialog-primary-field")
        var accept = objectNamed("plasma-host-dialog-accept")
        compare(primary.text, "Apollo")
        primary.text = " "
        compare(accept.enabled, false)
        page.dismissOverlay(41)
        page.overlay("custom-frame-rate", "host-a", ({ fps: 60, minimum: 10, maximum: 9999 }))
        compare(primary.text, "60")
        primary.text = "9"
        compare(accept.enabled, false)
        primary.text = "120"
        compare(accept.enabled, true)
        accept.clicked()
        compare(page.lastCommand, "submitCustomFrameRate")
        compare(page.lastArguments, [41, 120])
        page.overlay("custom-frame-rate", "host-a", ({ fps: 60 }))
        compare(primary.text, "60")
    }

    function test_discovery_off_stops_searching_empty_state() {
        page.hosts.clear()
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                    { enableMdns: false })
        var empty = objectNamed("starlight.hosts.empty-state")
        tryCompare(empty, "visible", true)
        compare(empty.discoveryOff, true)
        verify(empty.emptyMessage.indexOf("Off") >= 0)
    }

    function test_unsupported_host_explains_disabled_connect() {
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                 { serverSupported: false })
        var reason = control("hosts.unsupported-server")
        verify(reason !== null)
        compare(reason.visible, true)
        verify(reason.text.indexOf("Check for a newer build") >= 0)
        compare(control("hosts.connect").enabled, false)
    }

    function test_error_interrupt_preserves_unsubmitted_add_host_address() {
        page.overlay("add-host", "", ({ address: "" }))
        var primary = objectNamed("plasma-host-dialog-primary-field")
        primary.text = "192.0.2.42"
        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "", payload: ({ message: "Temporary error" }) })
        page.dismissOverlay(88)
        compare(primary.text, "192.0.2.42")
    }

    function test_interrupt_focus_resumes_base_then_restores_selected_host() {
        var connection = control("hosts.connection-test")
        var dialogs = objectNamed("plasma-host-dialog-layer")
        connection.forceActiveFocus()
        connection.clicked()
        tryCompare(dialogs, "visibleOverlayKind", "connection-test")
        // Read-only dialogs present a single Close response; it takes
        // the initial focus.
        tryCompare(objectNamed("plasma-host-dialog-reject"), "activeFocus", true)
        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "", payload: ({ message: "Interrupted" }) })
        tryCompare(dialogs, "visibleOverlayKind", "error")
        tryCompare(objectNamed("plasma-host-dialog-reject"), "activeFocus", true)
        objectNamed("plasma-host-dialog-reject").clicked()
        tryCompare(dialogs, "visibleOverlayKind", "connection-test")
        tryCompare(objectNamed("plasma-host-dialog-reject"), "activeFocus", true)
        objectNamed("plasma-host-dialog-reject").clicked()
        tryCompare(objectNamed("plasma-app-menu-button"), "activeFocus", true)
        var focusedTargets = activeContractFocusTargets()
        compare(focusedTargets.map(function(item) { return item.objectName }).sort(),
                ["plasma-app-menu-button"])
    }

    function test_constructed_with_preexisting_base_overlay_synchronizes_dialog() {
        var instance = createPreexistingOverlayRoot("add-host", "none")

        var dialogs = objectNamedIn(instance.root, "plasma-host-dialog-layer")
        tryCompare(dialogs, "visibleOverlayKind", "add-host")
        tryCompare(objectNamedIn(instance.root, "plasma-host-dialog-primary-field"),
                   "activeFocus", true)
        destroyPreexistingOverlayRoot(instance)
    }

    function test_constructed_with_preexisting_interrupt_overlay_synchronizes_dialog() {
        var instance = createPreexistingOverlayRoot("connection-test", "error")

        var dialogs = objectNamedIn(instance.root, "plasma-host-dialog-layer")
        tryCompare(dialogs, "visibleOverlayKind", "error")
        tryCompare(objectNamedIn(instance.root, "plasma-host-dialog-reject"),
                   "activeFocus", true)
        destroyPreexistingOverlayRoot(instance)
    }

    function test_narrow_delete_cancel_keeps_the_details_page_and_a_visible_focus() {
        root.width = 520
        root.selectHost("host-a")
        compare(root.narrowShowsDetails, true)
        control("hosts.delete").clicked()

        var dialogs = objectNamed("plasma-host-dialog-layer")
        tryCompare(dialogs, "visibleOverlayKind", "confirm-delete")
        objectNamed("plasma-host-dialog-reject").clicked()

        compare(root.narrowShowsDetails, true)
        tryCompare(gateWindow, "activeFocusItem", objectNamed("plasma-host-actions-button"))
    }

    function test_narrow_overlay_dismiss_keeps_the_details_page_and_a_visible_focus() {
        root.width = 520
        root.selectHost("host-a")
        compare(root.narrowShowsDetails, true)
        control("hosts.rename").clicked()

        var dialogs = objectNamed("plasma-host-dialog-layer")
        tryCompare(dialogs, "visibleOverlayKind", "rename-host")
        objectNamed("plasma-host-dialog-reject").clicked()

        compare(root.narrowShowsDetails, true)
        tryCompare(gateWindow, "activeFocusItem", objectNamed("plasma-host-actions-button"))
    }

    function test_closed_enum_values_and_custom_indices_preserve_legacy_mappings() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     { videoCodecConfig: 4,
                                                       width: 1366, height: 768, fps: 75 })
        wait(0)
        var codec = control("hosts.video-codec")
        compare(codec.model[codec.currentIndex].value, 4)
        compare(control("hosts.resolution").currentIndex, 1)
        compare(control("hosts.frame-rate").currentIndex, 1)
        codec.activated(1)
        compare(page.lastCommand, "setVideoCodec")
    }

    function test_selected_preferences_need_a_selected_host() {
        var gateWindow = gateWindowComponent.createObject(testCase)
        var visualRoot = component.createObject(gateWindow.contentItem,
                                                { page: page, width: 1100, height: 760 })
        wait(20)
        var preferences = objectNamedIn(visualRoot, "plasma-host-preferences")
        verify(preferences !== null)
        page.selectedHostUuid = ""
        page.selectedHostSnapshot = ({})
        wait(0)
        compare(preferences.enabled, false)
        page.selectedHostUuid = "host-a"
        page.selectedHostSnapshot = ({ uuid: "host-a", name: "Apollo" })
        wait(0)
        compare(preferences.enabled, true)
        visualRoot.destroy()
        gateWindow.destroy()
    }

    function test_bitrateFollowsPreferencesAfterPointerInput() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, { autoAdjustBitrate: false, manualStream: true })
        var slider = control("hosts.maximum-bitrate")
        verify(slider !== null)
        tryCompare(slider, "value", 20000)
        slider.forceActiveFocus()
        wait(0)
        // Focus reveal is queued; its timing and the row's position vary
        // with the Qt version and desktop font. Establish a real pointer
        // target inside the clipped viewport before exercising the slider.
        var viewport = objectNamed("plasma-hosts-viewport")
        verify(viewport !== null)
        var position = slider.mapToItem(viewport.contentItem, 0, 0)
        viewport.contentY = Math.max(0, Math.min(
                    position.y - (viewport.height - slider.height) / 2,
                    Math.max(0, viewport.contentHeight - viewport.height)))
        tryVerify(function() {
            var target = slider.mapToItem(viewport, 0, 0)
            return slider.width > 0 && slider.height > 0
                    && target.x >= 0 && target.x + slider.width <= viewport.width
                    && target.y >= 0 && target.y + slider.height <= viewport.height
        }, 5000, "the real slider must be wholly inside its pointer viewport")
        mouseDrag(slider, slider.width * 0.2, slider.height / 2,
                  slider.width * 0.4, 0)
        verify(slider.value !== 20000, "pointer input must move the real slider")
        compare(page.lastCommand, "setBitrateKbps")
        page.hostPreferenceSnapshot = Object.assign(
                    {}, page.hostPreferenceSnapshot, { bitrateKbps: 35000 })
        tryCompare(slider, "value", 35000)
    }

    function test_preference_controls_forward_exact_values() {
        var resolution = control("hosts.resolution")
        var frameRate = control("hosts.frame-rate")
        var bitrate = control("hosts.maximum-bitrate")
        var codec = control("hosts.video-codec")
        var limit = control("hosts.limit-bandwidth")

        resolution.activated(0)
        compare(page.lastCommand, "setResolution")
        compare(page.lastArguments, [1920, 1080])
        frameRate.activated(0)
        compare(page.lastCommand, "setFrameRate")
        compare(page.lastArguments, [60])
        bitrate.value = 47500
        bitrate.moved()
        compare(page.lastCommand, "setBitrateKbps")
        compare(page.lastArguments, [47500])
        codec.activated(2)
        compare(page.lastCommand, "setVideoCodec")
        compare(page.lastArguments, [2])

        ;[ ["hosts.local-cursor", "setLocalCursorEnabled"],
           ["hosts.limit-bandwidth", "setBandwidthLimit"] ].forEach(
                    function(entry) {
            var toggle = control(entry[0])
            var expected = !toggle.checked
            toggle.controllerActivate()
            compare(page.lastCommand, entry[1])
            compare(page.lastArguments, [expected])
        })
    }

    function test_overlay_uses_immutable_effective_payload() {
        var dialogs = objectNamed("plasma-host-dialog-layer")
        page.overlay("connection-test", "host-a", ({ result: 0, blockedPorts: "" }))
        tryCompare(dialogs, "visibleOverlayKind", "connection-test")
        verify(dialogs.state.body.indexOf("port check passed") >= 0)
        page.overlay("details", "host-a", ({ details: "Frozen detail" }))
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                   { details: "Live changed" })
        tryCompare(dialogs, "visibleOverlayKind", "details")
        compare(dialogs.state.body, "Frozen detail")
    }
    function test_frame_rate_can_follow_display_or_use_a_manual_choice() {
        page.frameRateOptions = [{text:"Automatic (match display)",value:0,automatic:true},
                                 {text:"60 FPS",value:60}, {text:"Custom…",value:-1}]
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {automaticFrameRate:true,fps:144})
        const fps = control("hosts.frame-rate")
        compare(fps.currentIndex, 0)
        fps.activated(1)
        compare(page.lastCommand, "setFrameRate")
        compare(page.lastArguments, [60])
        fps.activated(0)
        compare(page.lastCommand, "setAutomaticFrameRate")
        compare(page.lastArguments, [])
    }

    function test_custom_values_stay_visible_and_keep_the_edit_action() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {width:1366,height:768,fps:120})
        const resolution = control("hosts.resolution")
        const fps = control("hosts.frame-rate")
        compare(resolution.displayText, "1366×768")
        compare(fps.displayText, "120 FPS")
        verify(resolution.Accessible.description.indexOf("1366×768") >= 0)
        verify(fps.Accessible.description.indexOf("120") >= 0)
        resolution.activated(resolution.model.length - 1)
        compare(page.baseOverlay.kind, "custom-resolution")
        page.dismissOverlay(page.baseOverlay.operationId)
        fps.activated(fps.model.length - 1)
        compare(page.baseOverlay.kind, "custom-frame-rate")
        page.dismissOverlay(page.baseOverlay.operationId)
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {width:1920,height:1080,fps:60})
        compare(resolution.displayText, resolution.currentText)
        compare(fps.displayText, fps.currentText)
    }

    function test_display_and_keyboard_preferences_use_native_rows() {
        const scale = control("hosts.desktop-scale")
        const typing = control("hosts.local-text-input")
        compare(scale.enabled, false)
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot, {desktopScaleSupported:true})
        compare(scale.enabled, true)
        scale.activated(2)
        compare(page.lastCommand, "setDesktopScale")
        compare(page.lastArguments, ["2"])
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, {desktopScale:"1"})
        compare(scale.currentIndex, 1)
        compare(typing.checked, false)
        compare(typing.Accessible.name, "Type with this computer’s keyboard layout")
        verify(typing.Accessible.description.indexOf("Turn off for games") >= 0)
        typing.controllerActivate()
        compare(page.lastCommand, "setLocalTextInput")
        compare(page.lastArguments, [true])
    }

    function test_contextual_session_actions_resume_without_switching_and_end_with_confirmation() {
        const resume = control("hosts.resume-running")
        const end = control("hosts.end-session")
        compare(resume.visible, false)
        compare(end.visible, false)
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
            {desktopAppId:42,runningAppId:77,runningAppName:"Editor"})
        var menu = objectNamed("plasma-host-actions-menu")
        objectNamed("plasma-host-actions-button").clicked()
        tryCompare(menu, "opened", true)
        compare(resume.visible, true)
        compare(end.visible, true)
        resume.clicked()
        compare(page.lastCommand, "requestResume")
        compare(page.lastArguments, ["host-a"])
        end.clicked()
        compare(page.lastCommand, "requestEndSession")
        compare(page.lastArguments, ["host-a"])
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot, {runningAppId:42})
        compare(resume.visible, false)
        compare(end.visible, true)
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot, {online:false})
        compare(end.visible, false)
        menu.close()
    }

    function test_connect_action_matches_the_running_target() {
        const button = control("hosts.connect")
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                  {busy:true,desktopAppId:42,runningAppId:42,runningAppName:"Desktop"})
        compare(button.text, "Resume Desktop")
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                  {runningAppId:77,runningAppName:"Editor"})
        compare(button.text, "Switch to Desktop")
        verify(button.Accessible.description.indexOf("Editor") >= 0)
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                  {busy:false,runningAppId:0,runningAppName:""})
        compare(button.text, "Connect")
    }


    function test_home_shows_quality_first_and_still_connects_directly() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        root.selectHost("host-a")
        wait(0)
        // No tabs to open: Automatic or Manual is on the page, above everything else.
        const mode = control("hosts.stream-mode")
        const display = control("hosts.display-mode")
        compare(mode.visible, true)
        verify(mode.mapToItem(root, 0, 0).y < display.mapToItem(root, 0, 0).y)
        compare(control("hosts.resolution").visible, false, "Manual choices start hidden")
        const before = page.commandCount
        control("hosts.connect").clicked()
        compare(page.lastCommand, "requestConnect")
        compare(page.commandCount, before + 1)
    }

    // Settings that could only make a stream worse are gone, not hidden; the
    // automatic ones have nothing to choose.
    function test_removed_settings_are_absent() {
        ;["hosts.vsync", "hosts.frame-pacing", "hosts.decoder", "hosts.hdr", "hosts.yuv444",
          "hosts.unlock-bitrate", "hosts.audio-configuration", "hosts.video-advanced",
          "hosts.quality-mode"].forEach(function(id) {
            compare(control(id), null, id + " must not exist")
        })
    }

    function test_automatic_says_what_it_chose_and_manual_reveals_the_choices() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        const mode = control("hosts.stream-mode")
        verify(mode !== null)
        compare(mode.displayText, "Automatic")
        verify(mode.Accessible.description.indexOf("1920×1080 at 60 fps") >= 0)
        page.streamPlan = Object.assign({}, page.streamPlan, { width: 1280, height: 800, limitedByHost: true,
                                                               limitReason: "Mini PC sustains 1280×800 at 60 fps" })
        verify(mode.Accessible.description.indexOf("Mini PC sustains 1280×800 at 60 fps") >= 0)
        const manualOnly = ["hosts.resolution", "hosts.frame-rate", "hosts.video-codec", "hosts.limit-bandwidth"]
        manualOnly.forEach(function(id) { compare(control(id).visible, false, id + " waits for Manual") })
        compare(control("hosts.maximum-bitrate").visible, false)
        mode.activated(1)
        compare(page.lastCommand, "setStreamMode")
        compare(page.lastArguments, ["manual"])
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, { manualStream: true })
        compare(mode.displayText, "Manual")
        manualOnly.forEach(function(id) { tryCompare(control(id), "visible", true) })
        compare(control("hosts.maximum-bitrate").visible, false, "the cap waits for Limit bandwidth")
        const limit = control("hosts.limit-bandwidth")
        compare(limit.checked, false)
        limit.controllerActivate()
        compare(page.lastCommand, "setBandwidthLimit")
        compare(page.lastArguments, [true])
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, { autoAdjustBitrate: false })
        tryCompare(control("hosts.maximum-bitrate"), "visible", true)
        mode.activated(0)
        compare(page.lastArguments, ["automatic"])
    }

    // Display and Keyboard and Mouse are Automatic or Manual like the stream:
    // Automatic says in one line what it does, Manual shows the choices.
    function test_display_and_input_are_automatic_until_manual_is_chosen() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
            { desktopScaleSupported: true, virtualDisplaySupported: true, virtualDisplayReady: true })
        const groups = [
            { mode: "hosts.display-mode", flag: "manualDisplay", command: "setDisplayMode",
              rows: ["hosts.desktop-scale", "hosts.view-scale", "hosts.display-target"],
              summary: "Fits the picture to the window and requests the closest supported text size. Requests a virtual display when the host allows it." },
            // This Sunshine host draws its own pointer too, so Automatic says how to hide it.
            { mode: "hosts.input-mode", flag: "manualInput", command: "setInputMode",
              rows: ["hosts.pointer-mode", "hosts.local-cursor"],
              summary: "Uses desktop or game pointer movement and draws the cursor locally. If you see two pointers, press Ctrl+Alt+Shift+N" }
        ]
        groups.forEach(function(group) {
            const mode = control(group.mode)
            verify(mode !== null, group.mode)
            compare(mode.visible, true)
            compare(mode.displayText, "Automatic")
            verify(mode.Accessible.description.indexOf(group.summary) >= 0, mode.Accessible.description)
            group.rows.forEach(function(id) { compare(control(id).visible, false, id + " waits for Manual") })
            const before = page.commandCount
            mode.activated(1)
            compare(page.lastCommand, group.command)
            compare(page.lastArguments, ["manual"])
            compare(page.commandCount, before + 1)
            const manual = {}
            manual[group.flag] = true
            page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, manual)
            compare(mode.displayText, "Manual")
            group.rows.forEach(function(id) { tryCompare(control(id), "visible", true) })
            mode.activated(0)
            compare(page.lastCommand, group.command)
            compare(page.lastArguments, ["automatic"])
            compare(page.commandCount, before + 2)
        })
        // Typing with this computer's layout is the person's own choice, always in view.
        compare(control("hosts.local-text-input").visible, true)
    }

    function test_plan_warnings_sit_beside_their_control_and_update() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     { manualStream: true, autoAdjustBitrate: false })
        const fpsWarning = objectNamed("warning.hosts.frame-rate")
        const sizeWarning = objectNamed("warning.hosts.resolution")
        const codecWarning = objectNamed("warning.hosts.video-codec")
        const bitrateWarning = objectNamed("warning.hosts.maximum-bitrate")
        verify(fpsWarning !== null && sizeWarning !== null && codecWarning !== null && bitrateWarning !== null)
        compare(fpsWarning.visible, false)
        page.streamPlan = Object.assign({}, page.streamPlan, { fpsWarning: "Mini PC managed about 25 fps at this size last time" })
        tryCompare(fpsWarning, "visible", true)
        compare(fpsWarning.text, "Mini PC managed about 25 fps at this size last time")
        verify(control("hosts.frame-rate").Accessible.description.indexOf("about 25 fps") >= 0)
        compare(sizeWarning.visible, false)
        compare(codecWarning.visible, false)
        // A different choice gets a different answer from the plan, at once.
        page.streamPlan = Object.assign({}, page.streamPlan, { fpsWarning: "",
            codecWarning: "This computer can't decode HEVC in hardware, so the picture will stutter" })
        tryCompare(fpsWarning, "visible", false)
        tryCompare(codecWarning, "visible", true)
        verify(codecWarning.text.indexOf("HEVC") >= 0)
        page.streamPlan = Object.assign({}, page.streamPlan, { bitrateWarning: "Too little for this size" })
        tryCompare(bitrateWarning, "visible", true)
    }

    function test_bandwidth_limit_never_goes_below_the_plan_floor() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     { manualStream: true, autoAdjustBitrate: false, bitrateKbps: 2000 })
        var bitrate = control("hosts.maximum-bitrate")
        compare(bitrate.from, 4000)
        compare(bitrate.to, 150000)
        tryCompare(bitrate, "value", 4000)
        page.streamPlan = Object.assign({}, page.streamPlan, { floorKbps: 12000 })
        compare(bitrate.from, 12000)
    }

    function test_nondefault_summary_tracks_saved_choices_and_local_view_scale() {
        const view = control("hosts.view-scale")
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, {viewScale:"actual"})
        compare(view.currentIndex, 1)
        view.activated(0)
        compare(page.lastCommand, "setViewScale")
        compare(page.lastArguments, ["fit"])
        view.activated(1)
        compare(page.lastArguments, ["actual"])
    }

    function test_sidebar_status_tracks_availability_and_pairing() {
        const row = control("hosts.item.host-a")
        verify(row !== null)
        compare(row.statusText, "Ready")
        verify(row.Accessible.name.indexOf("Ready") >= 0)
        page.hosts.setProperty(0, "paired", false)
        compare(row.statusText, "Needs pairing")
        page.hosts.setProperty(0, "online", false)
        compare(row.statusText, "Offline")
        verify(row.Accessible.name.indexOf("Offline") >= 0)
        page.hosts.setProperty(0, "statusUnknown", true)
        compare(row.statusText, "Checking…")
    }

    function test_empty_discovery_explains_the_host_to_client_handoff() {
        page.hosts.clear()
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {enableMdns:true})
        const empty = control("hosts.empty-state")
        compare(empty.emptyMessage, "Looking for Hosts")
        page.initialDiscoveryElapsed = true
        compare(empty.emptyMessage, "No Hosts Found Yet")
        verify(empty.guidance.indexOf("same network") >= 0)
        verify(empty.Accessible.description.indexOf("address") >= 0)
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {enableMdns:false})
        verify(empty.emptyMessage.indexOf("Off") >= 0)
    }

}
