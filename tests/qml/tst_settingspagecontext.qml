import QtQuick 2.15
import QtTest 1.3

TestCase {
    id: testCase
    name: "SettingsPageContext"

    property var component: null
    property var context: null

    QtObject {
        id: preferences

        property int windowMode: 2
        property bool playAudioOnHost: false
        property int language: 0
        property bool muteOnFocusLoss: false
        property bool quitAppAfter: false
        property int uiDisplayMode: 0
        property bool connectionWarnings: true
        property bool configurationWarnings: true
        property bool richPresence: false
        property bool keepAwake: true
        property int captureSysKeysMode: 1
        property int lastEnabledCaptureSysKeysMode: 1
        property bool absoluteTouchMode: false
        property bool swapMouseButtons: false
        property bool reverseScrollDirection: false
        property bool swapFaceButtons: false
        property bool multiController: true
        property bool gamepadMouse: false
        property bool backgroundGamepad: false
        property bool enableMdns: true
        property bool detectNetworkBlocking: true
        property bool showPerformanceOverlay: false
        property int saveCalls: 0
        property var languageDecision: ({ showRestartNotice: false,
                                          clearOnBack: false,
                                          appliedImmediately: false })

        function save() { saveCalls += 1 }
        function changeLanguage(value) {
            language = value
            return languageDecision
        }
    }

    QtObject {
        id: systemProperties
        property bool rendererAlwaysFullScreen: false
        property bool hasDesktopEnvironment: true
        property bool hasDiscordIntegration: false
    }

    QtObject {
        id: computerManager
        property int startCalls: 0
        property int stopCalls: 0
        function startPolling() { startCalls += 1 }
        function stopPollingAsync() { stopCalls += 1 }
    }

    QtObject {
        id: gamepadNavigation
        property var modes: []
        property int connectedGamepads: 0
        function setUiNavMode(value) { modes = modes.concat([value]) }
        function getConnectedGamepads() { return connectedGamepads }
    }

    function initTestCase() {
        component = Qt.createComponent(Qt.resolvedUrl(
                                           "../../app/gui/adaptive/SettingsPageContext.qml"))
        verify(component.status === Component.Ready, component.errorString())
    }

    function createContext(pollingActive) {
        preferences.windowMode = 2
        preferences.playAudioOnHost = false
        preferences.language = 0
        preferences.muteOnFocusLoss = false
        preferences.quitAppAfter = false
        preferences.uiDisplayMode = 0
        preferences.connectionWarnings = true
        preferences.configurationWarnings = true
        preferences.richPresence = false
        preferences.keepAwake = true
        preferences.lastEnabledCaptureSysKeysMode = 1
        preferences.captureSysKeysMode = 1
        preferences.absoluteTouchMode = false
        preferences.swapMouseButtons = false
        preferences.reverseScrollDirection = false
        preferences.swapFaceButtons = false
        preferences.multiController = true
        preferences.gamepadMouse = false
        preferences.backgroundGamepad = false
        preferences.enableMdns = true
        preferences.detectNetworkBlocking = true
        preferences.showPerformanceOverlay = false
        preferences.languageDecision = ({ showRestartNotice: false,
                                          clearOnBack: false,
                                          appliedImmediately: false })
        preferences.saveCalls = 0
        computerManager.startCalls = 0
        computerManager.stopCalls = 0
        gamepadNavigation.modes = []
        gamepadNavigation.connectedGamepads = 0
        context = component.createObject(testCase, {
                                             preferences: preferences,
                                             systemProperties: systemProperties,
                                             computerManager: computerManager,
                                             gamepadNavigation: gamepadNavigation,
                                             pollingActive: pollingActive === true
                                         })
        verify(context !== null, component.errorString())
        return context
    }

    function cleanup() {
        if (context) {
            context.destroy()
            wait(0)
            context = null
        }
    }

    function test_facade_exports_closed_settings_triplets_and_options() {
        createContext(false)
        var facade = context.facade
        var bases = ["windowMode", "muteHostSpeakers", "language",
                     "muteOnFocusLoss", "quitAppAfter", "uiDisplayMode",
                     "connectionWarnings", "configurationWarnings",
                     "richPresence", "keepAwake", "captureSystemKeysMode",
                     "captureSystemKeys", "virtualTrackpad", "swapMouseButtons",
                     "reverseScroll", "swapFaceButtons", "singleController",
                     "gamepadMouse", "backgroundGamepad", "mdns",
                     "detectNetworkBlocking", "performanceOverlay"]
        compare(bases.length, 22)
        for (var i = 0; i < bases.length; ++i) {
            verify(facade[bases[i] + "Value"] !== undefined)
            verify(facade[bases[i] + "Visible"] !== undefined)
            verify(facade[bases[i] + "Sensitive"] !== undefined)
        }
        verify(facade.windowModeOptions instanceof Array)
        verify(facade.languageOptions instanceof Array)
        verify(facade.uiDisplayModeOptions instanceof Array)
        verify(facade.captureSystemKeysModeOptions instanceof Array)
        compare(facade.muteHostSpeakersValue, true)
        compare(facade.virtualTrackpadValue, true)
        compare(facade.singleControllerValue, false)
        compare(facade.richPresenceVisible, false)
    }

    function test_closed_typed_commands_preserve_inverses_and_reject_unknown_enums() {
        createContext(false)
        var facade = context.facade
        facade.setMuteHostSpeakersEnabled(false)
        compare(preferences.playAudioOnHost, true)
        facade.setVirtualTrackpadEnabled(false)
        compare(preferences.absoluteTouchMode, true)
        facade.setSingleControllerEnabled(true)
        compare(preferences.multiController, false)
        facade.setWindowMode(2)
        compare(preferences.windowMode, 2)
        facade.setWindowMode(99)
        compare(preferences.windowMode, 2)
    }

    function test_capture_toggle_remembers_last_non_off_mode() {
        createContext(false)
        var facade = context.facade
        facade.setCaptureSystemKeysMode(2)
        facade.setCaptureSystemKeysEnabled(false)
        compare(preferences.captureSysKeysMode, 0)
        facade.setCaptureSystemKeysEnabled(true)
        compare(preferences.captureSysKeysMode, 2)
    }

    function test_capture_mode_survives_page_recreation() {
        createContext(false)
        context.facade.setCaptureSystemKeysMode(2)
        context.facade.setCaptureSystemKeysEnabled(false)
        context.destroy()
        context = component.createObject(testCase, {preferences: preferences, systemProperties: systemProperties})
        context.facade.setCaptureSystemKeysEnabled(true)
        compare(preferences.captureSysKeysMode, 2)
    }

    function test_language_emits_effects_and_back_carries_clear_to_root() {
        createContext(false)
        preferences.languageDecision = ({ showRestartNotice: true,
                                          clearOnBack: true,
                                          appliedImmediately: true })
        var effects = []
        context.windowEffectRequested.connect(function(effect) {
            effects = effects.concat([effect])
        })
        context.facade.setLanguage(1)
        compare(preferences.language, 1)
        compare(effects.length, 1)
        compare(effects[0].kind, "toast")
        context.facade.requestBack(false)
        compare(effects.length, 2)
        compare(effects[1].kind, "back")
        compare(effects[1].payload.clearToRoot, true)
    }

    function test_option_snapshots_are_scalar_only_and_detached_from_context() {
        createContext(false)
        var facade = context.facade
        var optionProperties = ["windowModeOptions", "languageOptions",
                                "uiDisplayModeOptions",
                                "captureSystemKeysModeOptions"]
        for (var propertyIndex = 0;
             propertyIndex < optionProperties.length;
             ++propertyIndex) {
            var propertyName = optionProperties[propertyIndex]
            var facadeOptions = facade[propertyName]
            var contextOptions = context[propertyName]
            verify(facadeOptions instanceof Array)
            verify(facadeOptions !== contextOptions)
            compare(facadeOptions.length, contextOptions.length)
            for (var optionIndex = 0;
                 optionIndex < facadeOptions.length;
                 ++optionIndex) {
                compare(typeof facadeOptions[optionIndex].text, "string")
                compare(typeof facadeOptions[optionIndex].value, "number")
                compare(Object.keys(facadeOptions[optionIndex]).sort(),
                        ["text", "value"])
            }

            var originalLength = contextOptions.length
            var originalText = contextOptions[0].text
            facadeOptions.push({ text: "surface mutation", value: 999 })
            facadeOptions[0].text = "surface mutation"
            compare(contextOptions.length, originalLength)
            compare(contextOptions[0].text, originalText)
        }

        context.destroy()
        wait(0)
        context = null
        createContext(false)
        compare(context.facade.windowModeOptions.length,
                context.windowModeOptions.length)
        verify(context.facade.windowModeOptions[0].text !==
               "surface mutation")
    }

    function test_effect_acknowledgement_requires_exact_pair_and_outcome() {
        createContext(false)
        context.routeActivated()
        compare(context.routeGeneration, 1)
        var effects = []
        context.windowEffectRequested.connect(function(effect) {
            effects = effects.concat([effect])
        })
        context.facade.requestBack(false)
        compare(effects.length, 1)
        var effect = effects[0]
        compare(effect.routeGeneration, 1)
        compare(context.acknowledgeWindowEffect(effect.effectId,
                                                effect.routeGeneration + 1,
                                                "consumed"),
                false)
        compare(context.acknowledgeWindowEffect(effect.effectId,
                                                effect.routeGeneration,
                                                "unexpected"),
                false)
        compare(context.acknowledgeWindowEffect(effect.effectId,
                                                effect.routeGeneration,
                                                "consumed"),
                true)
        compare(context.acknowledgeWindowEffect(effect.effectId,
                                                effect.routeGeneration,
                                                "consumed"),
                false)
    }

    function test_emitted_effect_mutation_cannot_change_pending_record() {
        createContext(false)
        context.routeActivated()
        preferences.languageDecision = ({ showRestartNotice: false,
                                          clearOnBack: true,
                                          appliedImmediately: true })
        context.facade.setLanguage(1)
        var effects = []
        context.windowEffectRequested.connect(function(effect) {
            effects = effects.concat([effect])
        })
        context.facade.requestBack(false)
        compare(effects.length, 1)
        var effect = effects[0]
        var effectId = effect.effectId
        var effectGeneration = effect.routeGeneration
        effect.kind = "toast"
        effect.routeGeneration = effectGeneration + 100
        compare(context.acknowledgeWindowEffect(effectId,
                                                effectGeneration,
                                                "consumed"),
                true)
        context.facade.requestBack(false)
        compare(effects.length, 2)
        compare(effects[1].payload.clearToRoot, false)
    }

    function test_route_boundaries_invalidate_unacknowledged_effects() {
        createContext(false)
        context.routeActivated()
        var effects = []
        context.windowEffectRequested.connect(function(effect) {
            effects = effects.concat([effect])
        })
        context.facade.requestBack(false)
        compare(effects.length, 1)
        var staleEffect = effects[0]
        context.routeDeactivating()
        compare(context.routeGeneration, 2)
        compare(context.acknowledgeWindowEffect(staleEffect.effectId,
                                                staleEffect.routeGeneration,
                                                "consumed"),
                false)
        context.routeActivated()
        compare(context.routeGeneration, 3)
    }

    function test_mdns_restarts_only_with_explicit_active_polling_input() {
        createContext(false)
        context.facade.setMdnsEnabled(false)
        compare(computerManager.stopCalls, 0)
        compare(computerManager.startCalls, 0)
        context.destroy()
        wait(0)
        context = null
        createContext(true)
        context.facade.setMdnsEnabled(false)
        compare(computerManager.stopCalls, 1)
        compare(computerManager.startCalls, 1)
    }

    function test_route_lifecycle_preserves_gamepad_and_double_save_contract() {
        createContext(false)
        context.routeActivated()
        compare(gamepadNavigation.modes, [true])
        context.routeDeactivating()
        compare(gamepadNavigation.modes, [true, false])
        compare(preferences.saveCalls, 1)
        context.destroy()
        wait(0)
        context = null
        compare(preferences.saveCalls, 2)
    }

    function test_initial_control_focus_is_conditional_on_a_connected_gamepad() {
        createContext(false)
        gamepadNavigation.connectedGamepads = 0
        verify(!context.shouldFocusInitialControl())
        gamepadNavigation.connectedGamepads = 1
        verify(context.shouldFocusInitialControl())
    }
}
