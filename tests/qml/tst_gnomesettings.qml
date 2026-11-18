import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3
import Starlight 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "GnomeSettings"
    when: windowShown

    property var component: null
    property var root: null
    property var contract: TestCompat.readJson("qrc:/tests/data/page-contract-v1.json")
    property int testLanguage: 1

    QtObject {
        id: settingsStub

        property int windowModeValue: 2
        property bool windowModeVisible: true
        property bool windowModeSensitive: true
        property bool muteHostSpeakersValue: true
        property bool muteHostSpeakersVisible: true
        property bool muteHostSpeakersSensitive: true
        property int languageValue: 0
        property bool languageVisible: true
        property bool languageSensitive: true
        property bool muteOnFocusLossValue: false
        property bool muteOnFocusLossVisible: true
        property bool muteOnFocusLossSensitive: true
        property bool quitAppAfterValue: false
        property bool quitAppAfterVisible: true
        property bool quitAppAfterSensitive: true
        property int uiDisplayModeValue: 0
        property bool uiDisplayModeVisible: true
        property bool uiDisplayModeSensitive: true
        property bool connectionWarningsValue: true
        property bool connectionWarningsVisible: true
        property bool connectionWarningsSensitive: true
        property bool configurationWarningsValue: true
        property bool configurationWarningsVisible: true
        property bool configurationWarningsSensitive: true
        property bool richPresenceValue: false
        property bool richPresenceVisible: true
        property bool richPresenceSensitive: true
        property bool keepAwakeValue: true
        property bool keepAwakeVisible: true
        property bool keepAwakeSensitive: true
        property int captureSystemKeysModeValue: 1
        property bool captureSystemKeysModeVisible: true
        property bool captureSystemKeysModeSensitive: true
        property bool captureSystemKeysValue: true
        property bool captureSystemKeysVisible: true
        property bool captureSystemKeysSensitive: true
        property bool virtualTrackpadValue: true
        property bool virtualTrackpadVisible: true
        property bool virtualTrackpadSensitive: true
        property bool swapMouseButtonsValue: false
        property bool swapMouseButtonsVisible: true
        property bool swapMouseButtonsSensitive: true
        property bool reverseScrollValue: false
        property bool reverseScrollVisible: true
        property bool reverseScrollSensitive: true
        property bool swapFaceButtonsValue: false
        property bool swapFaceButtonsVisible: true
        property bool swapFaceButtonsSensitive: true
        property bool singleControllerValue: false
        property bool singleControllerVisible: true
        property bool singleControllerSensitive: true
        property bool gamepadMouseValue: false
        property bool gamepadMouseVisible: true
        property bool gamepadMouseSensitive: true
        property bool backgroundGamepadValue: false
        property bool backgroundGamepadVisible: true
        property bool backgroundGamepadSensitive: true
        property bool mdnsValue: true
        property bool mdnsVisible: true
        property bool mdnsSensitive: true
        property bool detectNetworkBlockingValue: true
        property bool detectNetworkBlockingVisible: true
        property bool detectNetworkBlockingSensitive: true
        property bool performanceOverlayValue: false
        property bool performanceOverlayVisible: true
        property bool performanceOverlaySensitive: true

        property var windowModeOptions: [{ text: "Fullscreen", value: 2 }]
        property var languageOptions: [{ text: "Automatic", value: 0 },
                                       { text: "English", value: 1 }]
        property var uiDisplayModeOptions: [{ text: "Windowed", value: 0 }]
        property var captureSystemKeysModeOptions: [{ text: "in fullscreen", value: 1 },
                                                     { text: "always", value: 2 }]
        property var commands: []

        function command(name, value) {
            commands = commands.concat([{ name: name, value: value }])
        }
        function setWindowMode(value) { command("setWindowMode", value) }
        function setMuteHostSpeakersEnabled(value) { command("setMuteHostSpeakersEnabled", value) }
        function setLanguage(value) { languageValue = value; command("setLanguage", value) }
        function setMuteOnFocusLossEnabled(value) { command("setMuteOnFocusLossEnabled", value) }
        function setQuitAppAfterEnabled(value) { command("setQuitAppAfterEnabled", value) }
        function setUiDisplayMode(value) { command("setUiDisplayMode", value) }
        function setConnectionWarningsEnabled(value) { command("setConnectionWarningsEnabled", value) }
        function setConfigurationWarningsEnabled(value) { command("setConfigurationWarningsEnabled", value) }
        function setRichPresenceEnabled(value) { command("setRichPresenceEnabled", value) }
        function setKeepAwakeEnabled(value) { command("setKeepAwakeEnabled", value) }
        function setCaptureSystemKeysMode(value) { command("setCaptureSystemKeysMode", value) }
        function setCaptureSystemKeysEnabled(value) { command("setCaptureSystemKeysEnabled", value) }
        function setVirtualTrackpadEnabled(value) { command("setVirtualTrackpadEnabled", value) }
        function setSwapMouseButtonsEnabled(value) { command("setSwapMouseButtonsEnabled", value) }
        function setReverseScrollEnabled(value) { command("setReverseScrollEnabled", value) }
        function setSwapFaceButtonsEnabled(value) { command("setSwapFaceButtonsEnabled", value) }
        function setSingleControllerEnabled(value) { command("setSingleControllerEnabled", value) }
        function setGamepadMouseEnabled(value) { command("setGamepadMouseEnabled", value) }
        function setBackgroundGamepadEnabled(value) { command("setBackgroundGamepadEnabled", value) }
        function setMdnsEnabled(value) { command("setMdnsEnabled", value) }
        function setDetectNetworkBlockingEnabled(value) { command("setDetectNetworkBlockingEnabled", value) }
        function setPerformanceOverlayEnabled(value) { command("setPerformanceOverlayEnabled", value) }
        function requestBack(clearToRoot) {
            command("requestBack", clearToRoot)
        }
    }

    function settingsSemanticIds() {
        return contract.rows.filter(function(row) {
            return row.id.indexOf("settings.") === 0
        }).map(function(row) { return row.id }).sort()
    }

    function control(id) {
        return findChild(root, "starlight." + id)
    }

    function objectNamed(name) {
        return findChild(root, name)
    }

    function cardIsDistinctFromPage(card) {
        return !Qt.colorEqual(card.cardColor, card.parentColor)
                || !Qt.colorEqual(card.outlineColor, card.parentColor)
                || !Qt.colorEqual(card.elevationColor, card.parentColor)
    }

    function initTestCase() {
        component = Qt.createComponent(Qt.resolvedUrl(
                                           "../../app/gui/adaptive/gnome/GnomeSettingsPage.qml"))
    }

    function init() {
        Appearance.reset()
        verify(component.status === Component.Ready, component.errorString())
        root = component.createObject(testCase,
                                      { page: settingsStub, width: 920, height: 760 })
        verify(root !== null, component.errorString())
        wait(0)
    }

    function cleanup() {
        if (root)
            root.destroy()
        root = null
        settingsStub.commands = []
        settingsStub.languageValue = 0
        Appearance.reset()
        wait(0)
    }

    function test_boxed_list_card_stays_distinct_in_light_and_dark() {
        const card = objectNamed("gnome-group-card")
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

    // list.boxed-list @extend %card, and .card is defined once in libadwaita
    // 1.9.2 src/stylesheet/widgets/_misc.scss:
    //
    //   background-color: var(--card-bg-color);
    //   border-radius: $card_radius;                /* 12px */
    //   box-shadow: 0 0 0 1px RGB(0 0 6 / 3%),
    //               0 1px 3px 1px RGB(0 0 6 / 7%),
    //               0 2px 6px 2px RGB(0 0 6 / 3%);
    //
    // A card therefore has no border at ordinary contrast -- the 1px ring is
    // the first shadow layer -- and it is lifted by a real blurred shadow
    // rather than an offset slab. Row separators are --card-shade-color
    // (_lists.scss %boxed_list_row), which is not --shade-color.
    function test_boxed_list_card_matches_adwaita_source() {
        const card = objectNamed("gnome-group-card")
        verify(card !== null)
        compare(card.radius, 12)
        compare(card.border.width, 0)

        const shadow = objectNamed("gnome-group-card-shadow")
        verify(shadow !== null)
        compare(shadow.opacity, 1.0)
        verify(String(shadow.source).indexOf("card_shadow.png") >= 0)
        compare(shadow.anchors.leftMargin, -6)
        compare(shadow.anchors.rightMargin, -6)
        compare(shadow.anchors.topMargin, -4)
        compare(shadow.anchors.bottomMargin, -8)
        compare(shadow.border.left, 22)
        compare(shadow.border.right, 22)
        compare(shadow.border.top, 22)
        compare(shadow.border.bottom, 22)
        tryVerify(function() { return shadow.status === Image.Ready })

        // The card sits on the page ground (--window-bg-color), so that is
        // what it must stand apart from -- not --view-bg-color, which is not
        // behind it, and which happens to equal the card in light mode.
        Appearance.colorScheme = 1
        tryCompare(Appearance, "dark", false)
        tryVerify(function() {
            return String(card.parentColor) === "#fafafb"
        })
        verify(!Qt.colorEqual(card.cardColor, card.parentColor))

        Appearance.colorScheme = 2
        tryCompare(Appearance, "dark", true)
        tryVerify(function() {
            return String(card.parentColor) === "#222226"
        })
        verify(!Qt.colorEqual(card.cardColor, card.parentColor))
        Appearance.reset()
    }

    function test_settingsScrollShowsUndershootAndHeaderDims() {
        var undershoot = objectNamed("gnome-settings-undershoot")
        var flickable = objectNamed("gnome-settings-flickable")
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

    function test_gnome_settings_matches_contract() {
        compare(TestCompat.semanticIds(root).sort(), settingsSemanticIds())
    }

    function test_controls_forward_their_named_facade_commands() {
        const language = control("settings.language")
        verify(language !== null)
        language.activated(1)
        tryVerify(function() { return settingsStub.commands.length === 1 })
        compare(settingsStub.commands[0].name, "setLanguage")
        compare(settingsStub.commands[0].value, testLanguage)

        const keepAwake = control("settings.keep-awake")
        verify(keepAwake !== null)
        keepAwake.controllerActivate()
        tryVerify(function() { return settingsStub.commands.length === 2 })
        compare(settingsStub.commands[1].name, "setKeepAwakeEnabled")
        compare(settingsStub.commands[1].value, false)
    }

    function test_capture_mode_is_inactive_until_capture_is_enabled() {
        const capture = control("settings.capture-system-keys")
        const mode = control("settings.capture-system-keys-mode")
        verify(capture !== null)
        verify(mode !== null)
        settingsStub.captureSystemKeysValue = false
        wait(0)
        compare(mode.enabled, false)
        settingsStub.captureSystemKeysValue = true
        wait(0)
        compare(mode.enabled, true)
    }

    function test_named_keyboard_and_network_settings_are_directly_available() {
        compare(control("settings.mdns").visible, control("settings.language").visible)
        compare(control("settings.capture-system-keys").visible,
                control("settings.language").visible)
    }

    function test_dropdowns_render_their_facade_option_text() {
        ["settings.window-mode", "settings.language",
         "settings.ui-display-mode", "settings.capture-system-keys-mode"]
                .forEach(function(id) {
            const dropdown = control(id)
            verify(dropdown !== null, id)
            compare(dropdown.displayText,
                    dropdown.model[dropdown.currentIndex].text,
                    id + " must display its facade text role")
        })
    }

    function test_page_has_no_persistence_lifecycle_seam() {
        verify(!TestCompat.hasMethod(root, "testDeactivate"))
    }

    function test_initial_focus_skips_hidden_window_mode() {
        const next = control("settings.keep-awake")
        verify(next !== null)
        settingsStub.windowModeVisible = false
        wait(0)
        root.routeActivated(true)
        tryCompare(next, "activeFocus", true)
    }

    function test_language_and_back_are_composed_through_facade() {
        const language = control("settings.language")
        verify(language !== null)
        language.activated(1)

        objectNamed("gnome-settings-close").clicked()
        compare(settingsStub.commands,
                [{ name: "setLanguage", value: testLanguage },
                 { name: "requestBack", value: false }])
    }
}
