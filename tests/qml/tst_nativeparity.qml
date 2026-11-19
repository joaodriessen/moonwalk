import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

// The promise behind the two native designs: every desktop offers the same
// host and application settings, because they render one shared declaration.
TestCase {
    id: testCase
    name: "NativeParity"
    when: windowShown

    Component {
        id: windowComponent
        ApplicationWindow { width: 1100; height: 760; visible: true }
    }
    readonly property string gnomeHosts: "../../app/gui/adaptive/gnome/GnomeHostsPage.qml"
    readonly property string plasmaHosts: "../../app/gui/adaptive/plasma/PlasmaHostsPage.qml"

    function allRows(groups) {
        var rows = []
        for (var g = 0; g < groups.length; ++g) {
            for (var r = 0; r < groups[g].rows.length; ++r) {
                rows.push(groups[g].rows[r])
                for (var n = 0; n < groups[g].rows[r].rows.length; ++n)
                    rows.push(groups[g].rows[r].rows[n])
            }
        }
        return rows
    }
    function createFacade() {
        var component = Qt.createComponent(Qt.resolvedUrl("fixtures/GnomeHostFacade.qml"))
        verify(component.status === Component.Ready, component.errorString())
        return createTemporaryObject(component, testCase)
    }
    function createSurface(path, facade) {
        if (path.indexOf("/plasma/") >= 0 && !kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        var win = createTemporaryObject(windowComponent, testCase)
        var component = Qt.createComponent(Qt.resolvedUrl(path))
        verify(component.status === Component.Ready, component.errorString())
        var surface = component.createObject(win.contentItem, { page: facade, width: 1100, height: 760 })
        verify(surface !== null, component.errorString())
        return surface
    }
    // Generated rows are visual children, which findChild does not always reach.
    function itemNamed(item, name) {
        if (item.objectName === name) return item
        for (var index = 0; index < item.children.length; ++index) {
            var hit = itemNamed(item.children[index], name)
            if (hit) return hit
        }
        return null
    }
    function bothDesktops() {
        return [{ tag: "gnome", path: gnomeHosts }, { tag: "plasma", path: plasmaHosts }]
    }

    function test_page_accessibility_landmarks_data() { return bothDesktops() }
    function test_page_accessibility_landmarks(data) {
        var hosts = createSurface(data.path, createFacade())
        compare(hosts.objectName, "starlight.hosts.page")
        compare(hosts.Accessible.role, Accessible.Pane)
        compare(hosts.Accessible.name, "Hosts")
        var list = findChild(hosts, "starlight.hosts.list")
        verify(list !== null)
        compare(list.Accessible.role, Accessible.List)
        compare(list.Accessible.name, data.tag === "gnome" ? "Available streaming hosts" : "Hosts")

        var component = Qt.createComponent(Qt.resolvedUrl("fixtures/SettingsFacade.qml"))
        verify(component.status === Component.Ready, component.errorString())
        var facade = createTemporaryObject(component, testCase)
        var pageName = data.tag === "gnome" ? "GnomeSettingsPage.qml" : "PlasmaSettingsPage.qml"
        var settings = createSurface("../../app/gui/adaptive/" + data.tag + "/" + pageName, facade)
        compare(settings.objectName, "starlight.settings.page")
        compare(settings.Accessible.role, Accessible.Pane)
        compare(settings.Accessible.name, "Settings")
        hosts.destroy()
        settings.destroy()
    }

    function test_every_host_setting_exists_on_every_desktop() {
        var facade = createFacade()
        var gnome = createSurface(gnomeHosts, facade)
        var plasma = createSurface(plasmaHosts, facade)
        var rows = allRows(gnome.presentation.groups)
        verify(rows.length >= 14, "the shared model must describe the whole page")
        rows.forEach(function(row) {
            verify(findChild(gnome, row.controlId) !== null, "GNOME lacks " + row.controlId)
            verify(findChild(plasma, row.controlId) !== null, "Plasma lacks " + row.controlId)
        })
        gnome.destroy()
        plasma.destroy()
    }

    function test_setting_actions_reach_the_facade_data() { return bothDesktops() }
    function test_setting_actions_reach_the_facade(data) {
        var facade = createFacade()
        var surface = createSurface(data.path, facade)
        findChild(surface, "starlight.hosts.saved-settings").clicked()
        compare(facade.lastCommand, "requestPresets")
        facade.dismissOverlay(facade.baseOverlay.operationId)
        findChild(surface, "starlight.hosts.restore-last-working").clicked()
        compare(facade.lastCommand, "restoreSuccessfulSettings")
        findChild(surface, "starlight.hosts.relearn-performance").clicked()
        compare(facade.lastCommand, "resetLearnedPerformance")
        surface.destroy()
    }

    function test_host_limits_are_explained_not_hidden_data() { return bothDesktops() }
    function test_host_limits_are_explained_not_hidden(data) {
        var facade = createFacade()
        facade.hostPreferenceSnapshot = Object.assign({}, facade.hostPreferenceSnapshot, { manualDisplay: true })
        var surface = createSurface(data.path, facade)
        var scale = findChild(surface, "starlight.hosts.desktop-scale")
        compare(scale.visible, true, "on Manual, a limit of this host stays visible")
        compare(scale.enabled, false, "and cannot be changed")
        verify(scale.Accessible.description.indexOf("controls its own text size") >= 0)
        surface.destroy()
    }

    function test_saved_settings_dialog_saves_applies_and_removes_data() { return bothDesktops() }
    function test_saved_settings_dialog_saves_applies_and_removes(data) {
        var facade = createFacade()
        facade.presets = [{ id: "p1", name: "Travel" }]
        var surface = createSurface(data.path, facade)
        findChild(surface, "starlight.hosts.saved-settings").clicked()
        var name = findChild(surface, "starlight.hosts.preset-name")
        tryCompare(name, "visible", true)
        name.text = "Gaming"
        findChild(surface, "starlight.hosts.preset-save").clicked()
        compare(facade.lastCommand, "savePreset")
        compare(facade.lastArguments, ["Gaming"])
        var list = findChild(surface, data.tag + "-host-dialog-presets")
        itemNamed(list, "starlight.hosts.preset-apply.p1").clicked()
        compare(facade.lastCommand, "applyPreset")
        itemNamed(list, "starlight.hosts.preset-remove.p1").clicked()
        compare(facade.lastCommand, "removePreset")
        facade.dismissOverlay(facade.baseOverlay.operationId)
        surface.destroy()
    }

    function test_every_application_preference_exists_on_every_desktop() {
        var component = Qt.createComponent(Qt.resolvedUrl("fixtures/SettingsFacade.qml"))
        verify(component.status === Component.Ready, component.errorString())
        var facade = createTemporaryObject(component, testCase)
        var gnome = createSurface("../../app/gui/adaptive/gnome/GnomeSettingsPage.qml", facade)
        var plasma = createSurface("../../app/gui/adaptive/plasma/PlasmaSettingsPage.qml", facade)
        var rows = allRows(gnome.settings.groups)
        verify(rows.length >= 20)
        rows.forEach(function(row) {
            verify(findChild(gnome, row.controlId) !== null, "GNOME lacks " + row.controlId)
            verify(findChild(plasma, row.controlId) !== null, "Plasma lacks " + row.controlId)
        })
        gnome.destroy()
        plasma.destroy()
    }

    // One Pointer choice replaced two toggles; it has to keep driving the same two commands.
    function test_pointer_choice_drives_the_existing_preferences_data() { return bothDesktops() }
    function test_pointer_choice_drives_the_existing_preferences(data) {
        var facade = createFacade()
        var surface = createSurface(data.path, facade)
        var pointer = findChild(surface, "starlight.hosts.pointer-mode")
        verify(pointer !== null)
        pointer.activated(1)
        compare(facade.lastCommand, "setAbsoluteMouseMode")
        compare(facade.lastArguments, [true])
        pointer.activated(2)
        compare(facade.lastCommand, "setAbsoluteMouseMode")
        compare(facade.lastArguments, [false])
        pointer.activated(0)
        compare(facade.lastCommand, "setAutomaticPointer")
        compare(facade.lastArguments, [true])
        surface.destroy()
    }

    function test_stream_summary_shows_what_automatic_will_actually_be() {
        var facade = createFacade()
        // The stream plan says what Automatic will be; the saved size is stale.
        facade.streamPlan = Object.assign({}, facade.streamPlan, { width: 2880, height: 1800, fps: 60 })
        facade.hostPreferenceSnapshot = Object.assign({}, facade.hostPreferenceSnapshot,
            { automaticResolution: true, width: 1280, height: 720, fps: 60, bitrateKbps: 53000 })
        var surface = createSurface(gnomeHosts, facade)
        var facts = surface.presentation.streamFacts
        compare(facts[0].value, "2880×1800", "not the stale saved size")
        compare(facts[0].note, "Matches this display")
        compare(facts[2].value, "20 Mbps", "the planned budget, not stale saved bitrate")
        compare(facts[2].note, "Planned bitrate budget")
        facade.streamPlan = Object.assign({}, facade.streamPlan, { fps: 30 })
        compare(surface.presentation.streamFacts[1].note, "Adjusted for this stream")
        verify(surface.presentation.streamSummary.indexOf("2880×1800") === 0)
        facade.streamPlan = Object.assign({}, facade.streamPlan, { width: 1920, height: 1200, limitedByHost: true,
                                                                   limitReason: "Mini PC sustains 1920×1200 at 60 fps" })
        compare(surface.presentation.streamFacts[0].value, "1920×1200")
        compare(surface.presentation.streamFacts[0].note, "Limited by host")
        surface.destroy()
    }

    function test_search_filters_preferences_and_clears_data() {
        return [{ tag: "gnome", path: "../../app/gui/adaptive/gnome/GnomeSettingsPage.qml" },
                { tag: "plasma", path: "../../app/gui/adaptive/plasma/PlasmaSettingsPage.qml" }]
    }
    function test_search_filters_preferences_and_clears(data) {
        var component = Qt.createComponent(Qt.resolvedUrl("fixtures/SettingsFacade.qml"))
        var facade = createTemporaryObject(component, testCase)
        var surface = createSurface(data.path, facade)
        var field = findChild(surface, "starlight.settings.search")
        var swap = findChild(surface, "starlight.settings.swap-mouse-buttons")
        var language = findChild(surface, "starlight.settings.language")
        field.text = "mouse"
        tryVerify(function() { return Array.prototype.some.call(surface.settings.groups, function(group) {
            return group.groupId === "general" && group.filteredOut }) })
        compare(surface.settings.searchHasResults, true)
        var rows = allRows(surface.settings.groups)
        compare(rows.filter(function(row) { return row.controlId === "starlight.settings.swap-mouse-buttons" })[0].filteredOut, false)
        compare(rows.filter(function(row) { return row.controlId === "starlight.settings.language" })[0].filteredOut, true)
        // Searching for something a row only knows by another name still finds it.
        field.text = "discovery"
        compare(rows.filter(function(row) { return row.controlId === "starlight.settings.mdns" })[0].filteredOut, false)
        field.text = "no setting is called this"
        compare(surface.settings.searchHasResults, false)
        field.text = ""
        compare(surface.settings.searchHasResults, true)
        rows.forEach(function(row) { compare(row.filteredOut, false) })
        surface.destroy()
    }

    function test_toggle_group_moves_with_arrow_keys_and_honours_what_it_is_told() {
        var component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/adaptive/gnome/GnomeToggleGroup.qml"))
        verify(component.status === Component.Ready, component.errorString())
        var group = createTemporaryObject(component, testCase, { model: ["One", "Two", "Three"], currentIndex: 0, width: 300 })
        var activated = []
        group.activated.connect(function(index) { activated.push(index) })
        group.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(group, "activeFocus", true)
        keyClick(Qt.Key_Right)
        compare(activated, [1])
        compare(group.currentIndex, 0, "the model decides what is chosen, not the control")
        group.currentIndex = 2
        keyClick(Qt.Key_Right)
        compare(activated, [1], "the last option has nothing after it")
        group.currentIndex = -1
        compare(findChild(group, "gnome-toggle-highlight").visible, false, "an option the host lost highlights nothing")
    }

    function test_last_connection_reads_as_words() {
        var facade = createFacade()
        var surface = createSurface(gnomeHosts, facade)
        var now = new Date(2026, 10, 15, 12).getTime()
        var since = surface.presentation.connectedSince
        compare(since(0, now), "", "a host never streamed says nothing")
        compare(since(new Date(2026, 10, 15, 1).getTime() / 1000, now), "Connected today")
        compare(since(new Date(2026, 10, 14, 23).getTime() / 1000, now), "Connected yesterday")
        compare(since(new Date(2026, 10, 5, 12).getTime() / 1000, now), "Connected 10 days ago")
        surface.destroy()
    }

    // First launch has no list to show, but the main menu has to stay reachable.
    function test_no_hosts_gives_the_page_the_window_and_keeps_the_menu() {
        var facade = createFacade()
        facade.hosts.clear()
        var surface = createSurface(gnomeHosts, facade)
        tryCompare(surface, "sidebarCount", 0)
        compare(surface.narrow, true)
        compare(findChild(surface, "gnome-hosts-empty-app-menu-button").visible, true)
        surface.destroy()
    }

    // Details opens focused on its Close, and gamepad A sends Return.
    function test_header_close_answers_return() {
        var facade = createFacade()
        var surface = createSurface(gnomeHosts, facade)
        var closed = 0
        var dialogLayer = findChild(surface, "gnome-host-dialog-layer")
        facade.requestDetails(facade.selectedHostUuid)
        var close = findChild(surface, "gnome-host-dialog-reject")
        tryCompare(close, "visible", true)
        compare(facade.baseOverlay.kind, "details")
        close.Keys.returnPressed(null)
        tryCompare(facade.baseOverlay, "kind", "none")
        surface.destroy()
    }
}
