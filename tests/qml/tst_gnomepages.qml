import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3
import Starlight 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "GnomePages"
    when: windowShown

    property var component: null
    property var root: null
    property var page: null
    property var contract: TestCompat.readJson("qrc:/tests/data/page-contract-v1.json")

    Component {
        id: gateWindowComponent

        ApplicationWindow {
            width: 1200
            height: 820
            visible: true
        }
    }

    function initTestCase() {
        component = Qt.createComponent(Qt.resolvedUrl(
                                           "../../app/gui/adaptive/gnome/GnomeHostsPage.qml"))
    }

    function init() {
        Appearance.reset()
        var facadeComponent = Qt.createComponent(Qt.resolvedUrl(
                                                "fixtures/GnomeHostFacade.qml"))
        verify(facadeComponent.status === Component.Ready,
               facadeComponent.errorString())
        page = facadeComponent.createObject(testCase)
        verify(page !== null)
        root = component.createObject(testCase, { page: page, width: 1100, height: 760 })
        verify(root !== null, component.errorString())
        wait(0)
    }

    function cleanup() {
        if (page && page.interruptOverlay.kind !== "none")
            page.dismissOverlay(page.interruptOverlay.operationId)
        if (page && page.baseOverlay.kind !== "none")
            page.dismissOverlay(page.baseOverlay.operationId)
        wait(0)
        if (root) root.destroy()
        if (page) page.destroy()
        root = null
        page = null
        Appearance.reset()
    }

    function normalizedIds() {
        return TestCompat.semanticIds(root).map(function(id) {
            return id.indexOf("hosts.item.") === 0 ? "hosts.item.<uuid>" : id
        }).sort()
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

    // AdwHeaderBar's default centering policy is LOOSE: keep the title
    // centred when possible, otherwise shift it -- and its title label is
    // ellipsize=end (adw-window-title.ui), so it shrinks rather than growing
    // into the controls. A title can therefore never overlap the buttons on
    // either side, at any width or text scale.
    // The reported desktop puts all three controls on the left. The test
    // mock's default of ":close" leaves the start Row empty, which is why an
    // earlier version of this test could not see the collision at all.
    function withLeftWindowControls() {
        Appearance.windowButtonLayout = "close,minimize,maximize:"
        var start = objectNamed("gnome-hosts-sidebar-start")
        tryVerify(function() { return start.width > 0 })
    }

    function assertSidebarHeaderHasNoOverlap(label) {
        var start = objectNamed("gnome-hosts-sidebar-start")
        var end = objectNamed("gnome-hosts-sidebar-end")
        var title = objectNamed("gnome-hosts-sidebar-title")
        var pane = objectNamed("gnome-hosts-sidebar-header")
        verify(start !== null && end !== null && title !== null && pane !== null)
        // Touching exactly is still a collision once anything rounds, so
        // require the header's own child spacing between them.
        var gap = 1
        verify(title.x >= start.x + start.width + gap,
               label + ": title runs into the window controls by "
               + ((start.x + start.width) - title.x).toFixed(1) + "px")
        verify(title.x + title.width + gap <= end.x,
               label + ": title runs into the header button by "
               + ((title.x + title.width) - end.x).toFixed(1) + "px")
        verify(title.x + title.width <= pane.width,
               label + ": title is clipped by the sidebar edge by "
               + ((title.x + title.width) - pane.width).toFixed(1) + "px")
    }

    // Files sizes its sidebar with sidebar-width-fraction 0.2 and
    // max-sidebar-width 240 (nautilus-window.blp) over AdwOverlaySplitView's
    // 180 minimum -- but the native widget also refuses to shrink the sidebar
    // below the sidebar's own minimum width. Reproducing Files' header with
    // the window controls in it measures a 275px minimum, past its own 240
    // maximum, which is how it stays legible. Starlight's equivalent header
    // (controls + title + one button) comes to 237.
    function test_sidebarWidthFollowsNativeSplitView() {
        withLeftWindowControls()
        var pane = objectNamed("gnome-hosts-sidebar-pane")
        verify(pane !== null)
        ;[760, 825, 1100, 1600].forEach(function(width) {
            root.width = width
            wait(0)
            verify(pane.width >= 200 && pane.width <= 320, "sidebar stays a sidebar at " + width)
            var title = objectNamed("gnome-hosts-sidebar-title")
            compare(title.width, title.implicitWidth, "the app name is not truncated at " + width)
            assertSidebarHeaderHasNoOverlap("width " + width)
        })
        root.width = 1100
        wait(0)
    }

    function test_sidebarTitleNeverOverlapsHeaderControls() {
        withLeftWindowControls()
        assertSidebarHeaderHasNoOverlap("default")

        // The window size this was actually reported at.
        root.width = 825
        wait(0)
        assertSidebarHeaderHasNoOverlap("reported window size")

        // At the narrowest split-view sidebar there is least room of all.
        root.width = 721
        wait(0)
        assertSidebarHeaderHasNoOverlap("minimum sidebar")

        // Accessibility text scaling widens the title without widening the
        // header, which is the same squeeze from the other direction.
        root.width = 1100
        Appearance.fontScale = 2.0
        tryCompare(Appearance, "fontScale", 2.0)
        wait(0)
        assertSidebarHeaderHasNoOverlap("2x text scale")
        Appearance.reset()
    }

    // _sidebars.scss: the sidebar carries a 1px --sidebar-border-color rule on
    // its inner edge, and repaints to --sidebar-backdrop-color when the window
    // is unfocused. Neither existed at all.
    function test_sidebarDrawsBorderAndFollowsBackdrop() {
        var divider = objectNamed("gnome-hosts-sidebar-divider")
        verify(divider !== null, "sidebar has no divider")
        compare(divider.width, 1)
        compare(String(divider.color), String(root.tokens.sidebarBorder))

        // The harness window is not focused, so assert the mapping rather
        // than a particular state.
        var expected = root.windowActive ? root.tokens.sidebar
                                         : root.tokens.sidebarBackdrop
        compare(String(root.sidebarSurfaceColor), String(expected))
        verify(!Qt.colorEqual(root.tokens.sidebar, root.tokens.sidebarBackdrop),
               "backdrop surface must differ from the focused one")
        var surface = objectNamed("gnome-hosts-sidebar-pane")
        verify(surface !== null)
    }

    // The alert response area (_message-dialog.scss, confirmed on a real
    // AdwAlertDialog): filled equal-width buttons with 12px spacing inside
    // 24px padding (12px above), and NO hairlines -- neither above the area
    // nor between the responses. The hairline-separated flat bar this used to
    // draw is the retired AdwMessageDialog anatomy.
    function test_hostDialogResponsesUseAlertAnatomy() {
        page.overlay("add-host", "")
        wait(0)
        var reject = objectNamed("gnome-host-dialog-reject")
        var accept = objectNamed("gnome-host-dialog-accept")
        verify(reject !== null && accept !== null)

        // Filled alert responses, not flat bar segments.
        compare(reject.background.radius, 12)
        verify(reject.background.color.a > 0.05,
               "plain response must carry the neutral fill")
        verify(reject.height >= 40, "response height " + reject.height
               + ", native is 44")
        compare(accept.height, reject.height)

        // Equal widths.
        verify(Math.abs(accept.width - reject.width) <= 1,
               "responses must be equal width: " + reject.width
               + " vs " + accept.width)

        // 12px between them, no 1px separator rectangle.
        var gap = accept.x - (reject.x + reject.width)
        verify(Math.abs(gap - 12) <= 1,
               "response spacing " + gap + ", native is 12")

        // No hairline anywhere in the footer.
        var footer = reject.parent
        while (footer && footer.objectName !== "gnome-host-dialog-footer")
            footer = footer.parent
        verify(footer !== null, "footer must carry a stable name")
        var hairlines = TestCompat.walkQObjectGraph(footer).filter(function(o) {
            return o !== null && o.height === 1 && o.color !== undefined
        })
        compare(hairlines.length, 0)
        page.dismissOverlay(page.baseOverlay.operationId)
        wait(0)
    }

    // A preferences-style page sits on --window-bg-color; --view-bg-color is
    // for content views (file grids, text). In light mode view is #ffffff,
    // which is also the card colour -- painting the page with it was what
    // originally made the boxed lists invisible.
    function test_contentGroundIsWindowColour() {
        var surface = objectNamed("gnome-hosts-content-surface")
        verify(surface !== null)
        compare(String(surface.color), String(root.tokens.window))
    }

    // headerbar:backdrop dims its contents with filter: opacity(0.5) over a
    // 200ms transition (_header-bar.scss:41). The ground swap for the sidebar
    // is covered separately; this is the content dimming.
    function test_headerContentsDimInBackdrop() {
        verify(root.headerContentOpacity !== undefined,
               "page must expose the backdrop dim mapping")
        var expected = root.windowActive ? 1.0 : 0.5
        compare(root.headerContentOpacity, expected)
        var title = objectNamed("gnome-hosts-sidebar-title")
        compare(title.opacity, root.headerContentOpacity)
        var header = objectNamed("gnome-hosts-content-header")
        verify(header !== null)
        compare(header.opacity, root.headerContentOpacity)
    }

    // A flat AdwToolbarView shows the undershoot indicator when content is
    // scrolled behind the top bar: a 1px inset line of --shade-color at 75%
    // plus a 4px gradient (_drawing.scss undershoot mixin). At rest there is
    // nothing.
    function test_contentScrollShowsUndershoot() {
        var undershoot = objectNamed("gnome-hosts-content-undershoot")
        var flickable = objectNamed("gnome-hosts-content-flickable")
        verify(undershoot !== null && flickable !== null)
        compare(undershoot.shown, false)
        // The fixture content is shorter than the viewport, so give the
        // flickable something to scroll or StopAtBounds snaps straight back.
        flickable.contentHeight = flickable.height + 600
        flickable.contentY = 60
        tryCompare(undershoot, "shown", true)
        flickable.contentY = 0
        tryCompare(undershoot, "shown", false)
    }

    // Production PcView syncs its bitrate slider imperatively, with a comment
    // explaining why: dragging self-writes `value`, which permanently breaks
    // a declarative binding. After one drag the GNOME slider stopped
    // following the stored preference -- so switching hosts or the automatic
    // bitrate recalculation left the handle and its label stale.
    function test_bitrateSliderSurvivesDragAndFollowsPreferences() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot, { autoAdjustBitrate: false, manualStream: true })
        var slider = control("hosts.maximum-bitrate")
        verify(slider !== null)
        tryCompare(slider, "value", 20000)

        // What a drag does internally: the slider writes its own value.
        slider.value = 90000

        // The stored preference then changes (host switch / auto bitrate).
        page.hostPreferenceSnapshot = Object.assign(
                    {}, page.hostPreferenceSnapshot, { bitrateKbps: 35000 })
        tryCompare(slider, "value", 35000,
                   5000, "slider must keep following the preference after a drag")
    }

    function objectNamed(name) {
        return findChild(root, name)
    }

    function objectNamedIn(item, name) {
        return findChild(item, name)
    }

    function activeFocusTargets() {
        var focused = TestCompat.walkQObjectGraph(root).filter(function(object) {
            // Item has activeFocusOnTab on every supported Qt version. Popup
            // has activeFocus too, but describes a scope rather than a target.
            return object.activeFocus === true && object.activeFocusOnTab !== undefined
        })
        return focused.filter(function(object) {
            // ListView and the actual popup item are enclosing focus scopes.
            // Only their focused leaf is an additional keyboard target.
            return !focused.some(function(other) {
                for (var ancestor = other.parent; ancestor; ancestor = ancestor.parent) {
                    if (ancestor === object) return true
                }
                return false
            })
        })
    }

    function activeContractFocusTargets() {
        return activeFocusTargets().filter(function(object) {
            return object.enabled === true && object.activeFocusOnTab !== undefined
                    && (object.objectName.indexOf("starlight.") === 0
                        || object.objectName.indexOf("gnome-host-dialog-") === 0
                        || object.objectName === "gnome-hosts-app-menu-button")
        })
    }

    function cardIsDistinctFromPage(card) {
        return !Qt.colorEqual(card.cardColor, card.parentColor)
                || !Qt.colorEqual(card.outlineColor, card.parentColor)
                || !Qt.colorEqual(card.elevationColor, card.parentColor)
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

    function test_page_ground_tracks_live_appearance_without_reconstruction() {
        var originalRoot = root
        var ground = directFullGround(root)
        verify(ground !== null)

        Appearance.colorScheme = 2
        tryCompare(Appearance, "dark", true)
        tryVerify(function() { return Qt.colorEqual(ground.color, root.tokens.window) })

        Appearance.colorScheme = 1
        tryCompare(Appearance, "dark", false)
        tryVerify(function() { return Qt.colorEqual(ground.color, root.tokens.window) })
        compare(root, originalRoot)
    }

    function test_boxed_list_card_stays_distinct_in_light_and_dark() {
        var card = objectNamed("gnome-group-card")
        verify(card !== null)
        verify(card.cardColor !== undefined)
        verify(card.parentColor !== undefined)
        verify(card.outlineColor !== undefined)
        verify(card.elevationColor !== undefined)

        Appearance.colorScheme = 1
        tryCompare(Appearance, "dark", false)
        tryVerify(function() { return cardIsDistinctFromPage(card) })

        Appearance.colorScheme = 2
        tryCompare(Appearance, "dark", true)
        tryVerify(function() { return cardIsDistinctFromPage(card) })
    }

    function test_split_header_separates_app_and_host_menus_and_opens_about() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                    { starlightVersionString: "0.3.0-alpha" })
        var sidebarTitle = objectNamed("gnome-hosts-sidebar-title")
        var contentTitle = objectNamed("gnome-hosts-content-title")
        verify(sidebarTitle !== null)
        verify(contentTitle !== null)
        compare(sidebarTitle.text, "Moonwalk")
        compare(contentTitle.text, page.selectedHostSnapshot.name)

        function menuIds(menu) {
            var ids = []
            for (var index = 0; index < menu.count; ++index) {
                var item = menu.itemAt(index)
                if (item.objectName.length > 0)
                    ids.push(item.objectName)
            }
            return ids
        }
        var appMenu = objectNamed("gnome-hosts-app-menu")
        compare(menuIds(appMenu), ["starlight.hosts.connection-test",
                                   "gnome-diagnostics-action",
                                   "gnome-hosts-app-menu-separator",
                                   "starlight.hosts.open-settings",
                                   "gnome-keyboard-shortcuts-action",
                                   "starlight.hosts.about",
                                   "gnome-hosts-quit"])
        var hostMenu = objectNamed("gnome-hosts-host-menu")
        compare(menuIds(hostMenu), ["starlight.hosts.details",
                                    "starlight.hosts.rename",
                                    "starlight.hosts.resume-running",
                                    "starlight.hosts.end-session",
                                    "starlight.hosts.delete"])

        page.selectedHostUuid = ""
        page.selectedHostSnapshot = ({})
        wait(0)
        ;["hosts.details", "hosts.rename", "hosts.delete"].forEach(function(id) {
            compare(control(id).enabled, false)
        })

        page.selectedHostUuid = "host-a"
        page.selectedHostSnapshot = ({ uuid: "host-a", name: "Apollo" })
        objectNamed("starlight.hosts.about").clicked()
        var about = objectNamed("gnome-about-dialog")
        verify(about !== null)
        tryCompare(about, "opened", true)
        verify(about.applicationName.length > 0)
        compare(about.applicationVersion, "0.3.0-alpha")
    }

    function test_source_faithful_header_metrics_do_not_scale_with_text() {
        compare(root.tokens.headerbarHeight, 46)
        compare(root.tokens.windowControlHitSize, 34)
        compare(root.tokens.windowControlImageSize, 24)
        compare(root.tokens.windowControlSpacing, 3)
        compare(root.headerHeight, 46)

        Appearance.fontScale = 1.75
        tryCompare(root.tokens, "fontScale", 1.75)
        compare(root.tokens.headerbarHeight, 46)
        compare(root.tokens.windowControlHitSize, 34)
        compare(root.tokens.windowControlImageSize, 24)
        compare(root.tokens.windowControlSpacing, 3)
        compare(root.headerHeight, 46)
    }

    function test_sidebar_title_uses_independent_adwaita_allocation() {
        var header = objectNamed("gnome-hosts-sidebar-header")
        var start = objectNamed("gnome-hosts-sidebar-start")
        var end = objectNamed("gnome-hosts-sidebar-end")
        var title = objectNamed("gnome-hosts-sidebar-title")
        verify(header !== null)
        verify(start !== null)
        verify(end !== null)
        verify(title !== null)
        compare(title.font.pixelSize, root.tokens.bodySize)
        compare(title.font.weight, Font.Bold)
        // adw-window-title.ui sets ellipsize=end. This previously asserted
        // ElideNone, which is what allowed the title to grow straight through
        // the header buttons; clamping x alone cannot prevent that.
        compare(title.elide, Text.ElideRight)
        var gap = root.tokens.headerbarChildSpacing
        var bandStart = start.x + start.width + gap
        var bandEnd = end.x - gap
        compare(Math.round(title.width),
                Math.round(Math.max(0, Math.min(title.implicitWidth,
                                                bandEnd - bandStart))))
        var idealX = (header.width - title.width) / 2
        var expectedX = Math.max(bandStart,
                                 Math.min(idealX, bandEnd - title.width))
        compare(Math.round(title.x), Math.round(expectedX))
        assertSidebarHeaderHasNoOverlap("independent allocation")
    }

    function test_secondary_host_actions_are_not_repeated_inline() {
        var inlineLabels = ["Test Connection", "View Details", "Rename…",
                            "Remove Host"]
        var inlineButtons = TestCompat.walkQObjectGraph(root).filter(function(object) {
            return object.flatStyle === true
                    && inlineLabels.indexOf(object.text) >= 0
        })
        compare(inlineButtons.length, 0)
    }

    function test_host_context_menu_duplicates_actions_in_source_order() {
        var contextMenu = objectNamed("gnome-host-context-menu")
        verify(contextMenu !== null)
        root.openHostContextMenu("host-a", root.selectedHostItem(), 8, 8)
        compare(page.selectedHostUuid, "host-a")
        tryCompare(contextMenu, "opened", true)
        var labels = []
        for (var index = 0; index < contextMenu.count; ++index) {
            var item = contextMenu.itemAt(index)
            if (item === contextMenu.accessibleItem)
                continue
            if (item.text !== undefined && item.text.length > 0)
                labels.push(item.text)
            else
                labels.push("separator")
        }
        compare(labels, ["Host Details", "Rename…", "Resume Running App",
                         "separator", "End Remote Session…", "Remove Host"])
        contextMenu.close()
    }

    function test_gnome_hosts_matches_contract() {
        verify(component.status === Component.Ready, component.errorString())
        compare(overlayInventoryIds(), hostContractIds())
    }

    function test_narrow_reflow_preserves_context_and_operation() {
        root.width = 1100
        page.requestRename("host-a")
        const operation = page.baseOverlay.operationId
        root.width = 520
        compare(page.baseOverlay.operationId, operation)
        compare(root.narrow, true)
        verify(activeFocusTargets().length <= 1)
    }

    function test_actions_forward_named_facade_commands() {
        const before = page.commandCount
        control("hosts.add-host").clicked()
        control("hosts.pair").clicked()
        control("hosts.connection-test").clicked()
        control("hosts.details").clicked()
        verify(page.commandCount >= before + 4)
    }

    function test_overlay_layer_dispatches_and_restores_focus() {
        var addHost = control("hosts.add-host")
        addHost.forceActiveFocus()
        addHost.clicked()
        var dialogs = objectNamed("gnome-host-dialog-layer")
        verify(dialogs !== null)
        tryCompare(dialogs, "visibleOverlayKind", "add-host")
        var primary = objectNamed("gnome-host-dialog-primary-field")
        var accept = objectNamed("gnome-host-dialog-accept")
        tryCompare(primary, "activeFocus", true)
        primary.text = "192.0.2.11"
        accept.clicked()
        compare(page.lastCommand, "submitAddHost")
        compare(page.lastArguments, ["192.0.2.11"])
        tryCompare(addHost, "activeFocus", true)
        compare(activeFocusTargets().length, 1)

        control("hosts.rename").clicked()
        tryCompare(dialogs, "visibleOverlayKind", "rename-host")
        compare(dialogs.focusReturnTarget.objectName, "starlight.hosts.item.host-a")
        objectNamed("gnome-host-dialog-reject").clicked()
        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "host-a",
                                   payload: ({ message: "Unavailable" }) })
        tryCompare(dialogs, "visibleOverlayKind", "error")
        objectNamed("gnome-host-dialog-reject").clicked()
        compare(page.lastCommand, "dismissOverlay")
        compare(page.lastArguments, [88])
    }

    function test_add_host_requires_address_and_shows_retry_error() {
        control("hosts.add-host").clicked()
        const primary = objectNamed("gnome-host-dialog-primary-field")
        const accept = objectNamed("gnome-host-dialog-accept")
        const error = objectNamed("gnome-host-dialog-add-error")
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
        var dialogs = objectNamed("gnome-host-dialog-layer")
        page.overlay("confirm-quit", "host-a", ({ runningAppLabel: "Editor" }))
        verify(dialogs.state.body.indexOf("Editor") >= 0)
        verify(dialogs.state.body.indexOf("unsaved") >= 0)
        page.dismissOverlay(41)
        page.overlay("connection-test", "", ({ result: -1, dispatched: true }))
        verify(dialogs.state.body.indexOf("Checking whether this network") >= 0)
        page.dismissOverlay(41)
        page.overlay("rename-host", "host-a", ({ name: "Apollo" }))
        var primary = objectNamed("gnome-host-dialog-primary-field")
        var accept = objectNamed("gnome-host-dialog-accept")
        compare(primary.text, "Apollo")
        primary.text = " "
        compare(accept.enabled, false)
        page.dismissOverlay(41)
        page.overlay("custom-resolution", "host-a", ({ width: 1920, height: 1080, minimum: 256, maximum: 8192 }))
        compare(primary.text, "1920")
        var secondary = objectNamed("gnome-host-dialog-secondary-field")
        compare(secondary.text, "1080")
        primary.text = "99"
        compare(accept.enabled, false)
        primary.text = "2560"
        compare(accept.enabled, true)
        accept.clicked()
        compare(page.lastCommand, "submitCustomResolution")
        compare(page.lastArguments, [41, 2560, 1080])
        page.overlay("custom-resolution", "host-a", ({ width: 1920, height: 1080 }))
        compare(primary.text, "1920")
        compare(secondary.text, "1080")
    }

    function test_unsupported_host_explains_disabled_connect() {
        var gateWindow = gateWindowComponent.createObject(testCase)
        var visualRoot = component.createObject(gateWindow.contentItem,
                                                { page: page, width: 1100, height: 760 })
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
                                                 { serverSupported: false })
        var reason = findChild(visualRoot, "starlight.hosts.unsupported-server")
        verify(reason !== null)
        tryCompare(reason, "visible", true)
        verify(reason.text.indexOf("Check for a newer build") >= 0)
        compare(findChild(visualRoot, "starlight.hosts.connect").enabled, false)
        visualRoot.destroy()
        gateWindow.destroy()
    }


    function test_error_interrupt_preserves_unsubmitted_add_host_address() {
        page.overlay("add-host", "", ({ address: "" }))
        var primary = objectNamed("gnome-host-dialog-primary-field")
        primary.text = "192.0.2.42"
        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "", payload: ({ message: "Temporary error" }) })
        page.dismissOverlay(88)
        compare(primary.text, "192.0.2.42")
    }

    function test_interrupt_focus_resumes_base_then_restores_origin() {
        var connection = control("hosts.connection-test")
        var dialogs = objectNamed("gnome-host-dialog-layer")
        connection.forceActiveFocus()
        connection.clicked()
        tryCompare(dialogs, "visibleOverlayKind", "connection-test")
        // Read-only dialogs present a single Close response; it takes
        // the initial focus.
        tryCompare(objectNamed("gnome-host-dialog-reject"), "activeFocus", true)
        compare(activeContractFocusTargets().length, 1)

        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "",
                                   payload: ({ message: "Interrupted" }) })
        tryCompare(dialogs, "visibleOverlayKind", "error")
        tryCompare(objectNamed("gnome-host-dialog-reject"), "activeFocus", true)
        compare(activeContractFocusTargets().length, 1)

        objectNamed("gnome-host-dialog-reject").clicked()
        tryCompare(dialogs, "visibleOverlayKind", "connection-test")
        tryCompare(objectNamed("gnome-host-dialog-reject"), "activeFocus", true)
        compare(activeContractFocusTargets().length, 1)

        objectNamed("gnome-host-dialog-reject").clicked()
        // The global diagnostic returns to its application-menu trigger.
        tryCompare(objectNamed("gnome-hosts-app-menu-button"),
                   "activeFocus", true)
        // Return focus to exactly one interactive target; enclosing scopes
        // are not additional navigation targets on either Qt version.
        compare(activeContractFocusTargets().length, 1)
    }

    function test_fixture_dismiss_overlay_honors_exact_operation_id() {
        page.overlay("connection-test", "host-a")
        page.interruptOverlay = ({ kind: "error", operationId: 88,
                                   hostUuid: "host-a",
                                   payload: ({ message: "Interrupted" }) })
        var before = page.commandCount
        page.dismissOverlay(999)
        compare(page.commandCount, before)
        compare(page.baseOverlay.kind, "connection-test")
        compare(page.interruptOverlay.kind, "error")

        page.dismissOverlay(88)
        compare(page.baseOverlay.kind, "connection-test")
        compare(page.interruptOverlay.kind, "none")

        page.interruptOverlay = ({ kind: "error", operationId: 89,
                                   hostUuid: "host-a",
                                   payload: ({ message: "Interrupted again" }) })
        page.dismissOverlay(41)
        compare(page.baseOverlay.kind, "none")
        compare(page.interruptOverlay.kind, "error")
        compare(page.interruptOverlay.operationId, 89)
    }

    function test_closed_enum_values_preserve_legacy_mappings() {
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     { videoCodecConfig: 4 })
        wait(0)
        var codec = control("hosts.video-codec")
        compare(codec.model[codec.currentIndex].value, 4)
        codec.activated(1)
        compare(page.lastCommand, "setVideoCodec")
        compare(page.lastArguments, [1])
    }

    function test_selected_preferences_need_a_selected_host() {
        var gateWindow = gateWindowComponent.createObject(testCase)
        var visualRoot = component.createObject(gateWindow.contentItem,
                                                { page: page, width: 1100, height: 760 })
        wait(20)
        var preferences = objectNamedIn(visualRoot, "gnome-host-preferences")
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

    function test_option_indices_match_current_and_custom_preferences() {
        page.resolutionOptions = [{ text: "1920×1080", w: 1920, h: 1080 },
                                  { text: "Custom…", w: 0, h: 0 }]
        page.frameRateOptions = [{ text: "60 fps", value: 60 },
                                 { text: "Custom…", value: -1 }]
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     { width: 1366, height: 768, fps: 75 })
        wait(0)
        compare(control("hosts.resolution").currentIndex, 1)
        compare(control("hosts.frame-rate").currentIndex, 1)
    }

    function test_overlay_uses_immutable_effective_payload() {
        var dialogs = objectNamed("gnome-host-dialog-layer")
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

    function test_hidden_session_actions_do_not_leave_empty_menu_rows() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        var menu = objectNamed("gnome-hosts-host-menu")
        objectNamed("gnome-hosts-host-menu-button").clicked()
        tryCompare(menu, "opened", true)
        var resume = control("hosts.resume-running")
        var end = control("hosts.end-session")
        compare(resume.height, 0, "a hidden action must not reserve a blank row")
        compare(end.height, 0, "a hidden action must not reserve a blank row")
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
            {desktopAppId:42,runningAppId:77,runningAppName:"Editor"})
        tryVerify(function() { return resume.height > 0 && end.height > 0 })
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot, {runningAppId:0})
        tryVerify(function() { return resume.height === 0 && end.height === 0 })
        menu.close()
    }

    function test_menu_button_retains_active_appearance_until_popup_closes() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        var menu = objectNamed("gnome-hosts-app-menu")
        var button = objectNamed("gnome-hosts-app-menu-button")
        // QColor wrappers refer to the live property; retain the original value.
        var idle = String(button.background.color)
        button.clicked()
        tryCompare(menu, "opened", true)
        tryVerify(function() {
            return !Qt.colorEqual(button.background.color, idle)
        }, 1000, "The open menu must retain the header button's active appearance")
        menu.close()
        tryCompare(menu, "opened", false)
        tryVerify(function() { return Qt.colorEqual(button.background.color, idle) })
    }

    function test_contextual_session_actions_resume_without_switching_and_end_with_confirmation() {
        var visualWindow = createTemporaryObject(gateWindowComponent, testCase)
        root.parent = visualWindow.contentItem
        const resume = control("hosts.resume-running")
        const end = control("hosts.end-session")
        compare(resume.visible, false)
        compare(end.visible, false)
        page.selectedHostSnapshot = Object.assign({}, page.selectedHostSnapshot,
            {desktopAppId:42,runningAppId:77,runningAppName:"Editor"})
        var menu = objectNamed("gnome-hosts-host-menu")
        objectNamed("gnome-hosts-host-menu-button").clicked()
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
        const window = createTemporaryObject(gateWindowComponent, testCase)
        verify(window !== null)
        root.parent = window.contentItem
        page.hosts.clear()
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {enableMdns:true})
        const empty = control("hosts.empty-state")
        compare(empty.emptyMessage, "Looking for Hosts")
        page.initialDiscoveryElapsed = true
        compare(empty.emptyMessage, "No Hosts Found Yet")
        verify(empty.guidance.indexOf("same network") >= 0)
        verify(empty.guidance.indexOf("Moonwalk Host on your Mac") >= 0)
        verify(empty.Accessible.description.indexOf("address") >= 0)
        const guide = control("hosts.setup-guide")
        tryCompare(guide, "visible", true)
        verify(guide.enabled)
        compare(guide.text, "Set Up a Host")
        page.hostPreferenceSnapshot = Object.assign({}, page.hostPreferenceSnapshot,
                                                     {enableMdns:false})
        verify(empty.emptyMessage.indexOf("Off") >= 0)
    }

}
