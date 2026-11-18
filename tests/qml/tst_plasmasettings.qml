import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import Starlight 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PlasmaSettings"
    when: windowShown

    property var component: null
    property var root: null
    property var page: null
    property var pageWindow: null
    property var contract: TestCompat.readJson("qrc:/tests/data/page-contract-v1.json")

    Component { id: nativeDropdown; ComboBox { model: ["Fullscreen"] } }
    Component {
        id: windowComponent
        ApplicationWindow { width: 920; height: 760; visible: true }
    }

    function settingsSemanticIds() {
        return contract.rows.filter(function(row) {
            return row.id.indexOf("settings.") === 0
        }).map(function(row) { return row.id }).sort()
    }

    function control(id) {
        return findChild(root, "starlight." + id)
    }

    function canonicalWidgetStyle(styleName) {
        return styleName.toString().split(":")[0].trim().toLowerCase()
    }

    function initTestCase() {
        if (!kirigamiAvailable)
            return
        component = Qt.createComponent(Qt.resolvedUrl(
                                           "../../app/gui/adaptive/plasma/PlasmaSettingsPage.qml"))
    }

    function init() {
        if (!kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        pageWindow = windowComponent.createObject(testCase)
        verify(pageWindow !== null)
        const facade = Qt.createComponent(Qt.resolvedUrl("fixtures/SettingsFacade.qml"))
        verify(facade.status === Component.Ready, facade.errorString())
        page = facade.createObject(testCase)
        verify(page !== null)
        verify(component.status === Component.Ready, component.errorString())
        root = component.createObject(pageWindow.contentItem, { page: page })
        verify(root !== null, component.errorString())
        wait(0)
    }

    function cleanup() {
        if (root) root.destroy()
        if (page) page.destroy()
        if (pageWindow) { pageWindow.close(); pageWindow.destroy() }
        root = null
        page = null
        pageWindow = null
        wait(0)
        PlasmaAppearance.reset()
    }

    function test_livePaletteSnapshotUpdatesExistingPage() {
        var originalRoot = root
        PlasmaAppearance.active = ({
            window: "#291d32", windowText: "#f2e6ff", base: "#35243f",
            alternateBase: "#412c4d", text: "#ead9f5",
            placeholderText: "#917ba1", button: "#4b3458",
            buttonText: "#fff1ff", highlight: "#42d6b0",
            highlightedText: "#071e18", toolTipBase: "#160f1b",
            toolTipText: "#fff5ff", light: "#684a75", midlight: "#593e65",
            mid: "#3e2b48", dark: "#24182b", shadow: "#09060b"
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
        compare(grounds[0].color, "#291d32")
    }

    function test_plasma_settings_matches_contract_and_active_style() {
        compare(TestCompat.semanticIds(root).sort(), settingsSemanticIds())
        // Style-identity assertions are enforced by the focused KDE rows,
        // which declare the expected widget style; the style-agnostic
        // aggregate (Material) cannot satisfy them and skips this block.
        if (plasmaWidgetStyleExpected.length > 0) {
            const dropdown = control("settings.window-mode")
            const nativeControl = nativeDropdown.createObject(pageWindow.contentItem)
            verify(dropdown.background !== null)
            verify(typeof dropdown.background.styleName !== "undefined")
            compare(canonicalWidgetStyle(dropdown.background.styleName),
                    canonicalWidgetStyle(plasmaWidgetStyleExpected))
            compare(dropdown.leftPadding, nativeControl.leftPadding)
            compare(dropdown.rightPadding, nativeControl.rightPadding)
            compare(dropdown.topPadding, nativeControl.topPadding)
            compare(dropdown.bottomPadding, nativeControl.bottomPadding)
            nativeControl.destroy()
        }
    }

    function test_controls_forward_exact_facade_commands() {
        const language = control("settings.language")
        language.activated(1)
        tryCompare(page, "commands", [{ name: "setLanguage", value: 1 }])
        const keepAwake = control("settings.keep-awake")
        keepAwake.controllerActivate()
        tryCompare(page, "commands", [{ name: "setLanguage", value: 1 },
                                        { name: "setKeepAwakeEnabled", value: false }])
    }

    function test_capture_mode_tracks_capture_enablement() {
        const mode = control("settings.capture-system-keys-mode")
        page.captureSystemKeysValue = false
        tryCompare(mode, "enabled", false)
        page.captureSystemKeysValue = true
        tryCompare(mode, "enabled", true)
    }

    function test_named_keyboard_and_network_settings_are_directly_available() {
        compare(control("settings.mdns").visible, control("settings.language").visible)
        compare(control("settings.capture-system-keys").visible, true)
    }

    function test_dropdowns_render_facade_option_text() {
        ["settings.window-mode", "settings.language", "settings.ui-display-mode",
         "settings.capture-system-keys-mode"].forEach(function(id) {
            const dropdown = control(id)
            compare(dropdown.displayText, dropdown.model[dropdown.currentIndex].text, id)
        })
    }

    function test_page_has_no_persistence_lifecycle_seam() {
        verify(!TestCompat.hasMethod(root, "testDeactivate"))
    }

    function test_initial_focus_skips_hidden_window_mode() {
        const next = control("settings.keep-awake")
        page.windowModeVisible = false
        root.routeActivated(true)
        tryCompare(pageWindow, "activeFocusItem", next)
    }

    function test_back_is_composed_through_the_facade() {
        const back = TestCompat.walkQObjectGraph(root).filter(function(object) {
            return object.accessibleName === "Back to hosts"
        })[0]
        back.navigationRequested()
        compare(page.commands, [{ name: "requestBack", value: false }])
    }
}
