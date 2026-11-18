import QtQuick 2.15

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0

// Non-painted owner for global settings behavior. Backend singletons remain
// lexical to this context; surfaces receive only facadeObject.
Item {
    id: root
    width: 0
    height: 0

    property var preferences: StreamingPreferences
    property var systemProperties: SystemProperties
    property var computerManager: ComputerManager
    property var gamepadNavigation: SdlGamepadKeyNavigation
    property bool pollingActive: false
    property bool routeActive: false
    property int routeGeneration: 0
    property int nextEffectId: 1
    property int captureSystemKeysOff: StreamingPreferences.CSK_OFF
    readonly property int lastCaptureSystemKeysMode: preferences.lastEnabledCaptureSysKeysMode
    property bool languageClearOnBack: false
    property var pendingEffects: ({})

    readonly property var facade: facadeObject
    readonly property var windowModeOptions: buildWindowModeOptions()
    readonly property var languageOptions: [
        { text: qsTranslate("SettingsView", "Automatic"), value: StreamingPreferences.LANG_AUTO },
        { text: "Deutsch (partial / English fallback)", value: StreamingPreferences.LANG_DE },
        { text: "English", value: StreamingPreferences.LANG_EN },
        { text: "Français (partial / English fallback)", value: StreamingPreferences.LANG_FR },
        { text: "简体中文", value: StreamingPreferences.LANG_ZH_CN },
        { text: "Norwegian Bokmål", value: StreamingPreferences.LANG_NB_NO },
        { text: "русский", value: StreamingPreferences.LANG_RU },
        { text: "Español", value: StreamingPreferences.LANG_ES },
        { text: "日本語", value: StreamingPreferences.LANG_JA },
        { text: "Tiếng Việt", value: StreamingPreferences.LANG_VI },
        { text: "ภาษาไทย", value: StreamingPreferences.LANG_TH },
        { text: "한국어", value: StreamingPreferences.LANG_KO },
        { text: "Magyar", value: StreamingPreferences.LANG_HU },
        { text: "Nederlands (partial / English fallback)", value: StreamingPreferences.LANG_NL },
        { text: "Svenska", value: StreamingPreferences.LANG_SV },
        { text: "Türkçe", value: StreamingPreferences.LANG_TR },
        { text: "繁體中文", value: StreamingPreferences.LANG_ZH_TW },
        { text: "Português", value: StreamingPreferences.LANG_PT },
        { text: "Português do Brasil", value: StreamingPreferences.LANG_PT_BR },
        { text: "Ελληνικά", value: StreamingPreferences.LANG_EL },
        { text: "Italiano", value: StreamingPreferences.LANG_IT },
        { text: "Język polski", value: StreamingPreferences.LANG_PL },
        { text: "Čeština", value: StreamingPreferences.LANG_CS },
        { text: "Български", value: StreamingPreferences.LANG_BG },
        { text: "தமிழ்", value: StreamingPreferences.LANG_TA }
    ]
    readonly property var uiDisplayModeOptions: [
        { text: qsTranslate("SettingsView", "Windowed"), value: StreamingPreferences.UI_WINDOWED },
        { text: qsTranslate("SettingsView", "Maximized"), value: StreamingPreferences.UI_MAXIMIZED },
        { text: qsTranslate("SettingsView", "Fullscreen"), value: StreamingPreferences.UI_FULLSCREEN }
    ]
    readonly property var captureSystemKeysModeOptions: [
        { text: qsTranslate("SettingsView", "in fullscreen"), value: StreamingPreferences.CSK_FULLSCREEN },
        { text: qsTranslate("SettingsView", "always"), value: StreamingPreferences.CSK_ALWAYS }
    ]

    signal windowEffectRequested(var effect)

    SettingsPageFacade { id: facadeObject }

    function buildWindowModeOptions() {
        var options = [
            { text: qsTranslate("SettingsView", "Fullscreen"), value: StreamingPreferences.WM_FULLSCREEN },
            { text: qsTranslate("SettingsView", "Borderless windowed"), value: StreamingPreferences.WM_FULLSCREEN_DESKTOP },
            { text: qsTranslate("SettingsView", "Windowed"), value: StreamingPreferences.WM_WINDOWED }
        ]
        for (var i = 0; i < options.length; ++i) {
            if (options[i].value === preferences.recommendedFullScreenMode) {
                options[i].text += " " + qsTranslate("SettingsView", "(Recommended)")
                var recommended = options.splice(i, 1)[0]
                options.unshift(recommended)
                break
            }
        }
        return options
    }

    function containsOption(options, value) {
        for (var i = 0; i < options.length; ++i) {
            if (options[i].value === value)
                return true
        }
        return false
    }

    function copyOptionSnapshot(options) {
        var snapshot = []
        for (var i = 0; i < options.length; ++i) {
            snapshot.push({
                              text: String(options[i].text),
                              value: Number(options[i].value)
                          })
        }
        return snapshot
    }

    function setBoolean(propertyName, value) {
        if (typeof value !== "boolean" || preferences[propertyName] === value)
            return false
        preferences[propertyName] = value
        return true
    }

    function emitEffect(kind, payload) {
        var effect = ({ kind: kind, effectId: nextEffectId++, routeGeneration: routeGeneration,
                        payload: payload || ({}) })
        // Keep an internal descriptor rather than the emitted object itself.
        // QML var objects are mutable even when Object.freeze() is requested.
        pendingEffects[effect.effectId] = ({
                                               kind: kind,
                                               effectId: effect.effectId,
                                               routeGeneration: routeGeneration
                                           })
        windowEffectRequested(effect)
    }

    function acknowledgeWindowEffect(effectId, effectRouteGeneration, outcome) {
        if (typeof effectId !== "number" ||
                Math.floor(effectId) !== effectId ||
                typeof effectRouteGeneration !== "number" ||
                Math.floor(effectRouteGeneration) !== effectRouteGeneration ||
                (outcome !== "consumed" && outcome !== "failed"))
            return false
        var effect = pendingEffects[effectId]
        if (!effect ||
                effect.routeGeneration !== effectRouteGeneration ||
                effectRouteGeneration !== routeGeneration)
            return false
        delete pendingEffects[effectId]
        if (effect.kind === "back" && outcome === "consumed")
            languageClearOnBack = false
        return true
    }

    function advanceRouteGeneration() {
        routeGeneration += 1
        pendingEffects = ({})
    }

    function routeActivated() {
        if (!routeActive)
            advanceRouteGeneration()
        routeActive = true
        if (gamepadNavigation && gamepadNavigation.setUiNavMode)
            gamepadNavigation.setUiNavMode(true)
    }
    function shouldFocusInitialControl() {
        return gamepadNavigation && gamepadNavigation.getConnectedGamepads
                && gamepadNavigation.getConnectedGamepads() > 0
    }

    function routeDeactivating() {
        if (routeActive)
            advanceRouteGeneration()
        routeActive = false
        if (gamepadNavigation && gamepadNavigation.setUiNavMode)
            gamepadNavigation.setUiNavMode(false)
        if (preferences && preferences.save)
            preferences.save()
    }

    function setWindowMode(value) {
        if (containsOption(windowModeOptions, value) && !systemProperties.rendererAlwaysFullScreen)
            preferences.windowMode = value
    }
    function setMuteHostSpeakersEnabled(value) {
        if (typeof value === "boolean")
            preferences.playAudioOnHost = !value
    }
    function setLanguage(value) {
        if (!containsOption(languageOptions, value) || !preferences.changeLanguage)
            return
        var decision = preferences.changeLanguage(value)
        if (decision && decision.showRestartNotice)
            emitEffect("toast", ({ text: qsTranslate("SettingsView", "Restart Moonwalk to apply this change.") }))
        if (decision && decision.clearOnBack)
            languageClearOnBack = true
    }
    function setMuteOnFocusLossEnabled(value) {
        if (systemProperties.hasDesktopEnvironment)
            setBoolean("muteOnFocusLoss", value)
    }
    function setQuitAppAfterEnabled(value) { setBoolean("quitAppAfter", value) }
    function setUiDisplayMode(value) {
        if (systemProperties.hasDesktopEnvironment && containsOption(uiDisplayModeOptions, value))
            preferences.uiDisplayMode = value
    }
    function setConnectionWarningsEnabled(value) { setBoolean("connectionWarnings", value) }
    function setConfigurationWarningsEnabled(value) { setBoolean("configurationWarnings", value) }
    function setRichPresenceEnabled(value) {
        if (systemProperties.hasDiscordIntegration)
            setBoolean("richPresence", value)
    }
    function setKeepAwakeEnabled(value) { setBoolean("keepAwake", value) }
    function setCaptureSystemKeysMode(value) {
        if (systemProperties.hasDesktopEnvironment && containsOption(captureSystemKeysModeOptions, value)) {
            preferences.lastEnabledCaptureSysKeysMode = value
            preferences.captureSysKeysMode = value
        }
    }
    function setCaptureSystemKeysEnabled(value) {
        if (typeof value !== "boolean" || !systemProperties.hasDesktopEnvironment)
            return
        if (!value) {
            if (preferences.captureSysKeysMode !== captureSystemKeysOff)
                preferences.lastEnabledCaptureSysKeysMode = preferences.captureSysKeysMode
            preferences.captureSysKeysMode = captureSystemKeysOff
        } else {
            preferences.captureSysKeysMode = lastCaptureSystemKeysMode
        }
    }
    function setVirtualTrackpadEnabled(value) {
        if (typeof value === "boolean")
            preferences.absoluteTouchMode = !value
    }
    function setSwapMouseButtonsEnabled(value) { setBoolean("swapMouseButtons", value) }
    function setReverseScrollEnabled(value) { setBoolean("reverseScrollDirection", value) }
    function setSwapFaceButtonsEnabled(value) { setBoolean("swapFaceButtons", value) }
    function setSingleControllerEnabled(value) {
        if (typeof value === "boolean")
            preferences.multiController = !value
    }
    function setGamepadMouseEnabled(value) { setBoolean("gamepadMouse", value) }
    function setBackgroundGamepadEnabled(value) {
        if (systemProperties.hasDesktopEnvironment)
            setBoolean("backgroundGamepad", value)
    }
    function setMdnsEnabled(value) {
        if (!setBoolean("enableMdns", value) || !pollingActive)
            return
        if (computerManager && computerManager.stopPollingAsync)
            computerManager.stopPollingAsync()
        if (computerManager && computerManager.startPolling)
            computerManager.startPolling()
    }
    function setDetectNetworkBlockingEnabled(value) { setBoolean("detectNetworkBlocking", value) }
    function setPerformanceOverlayEnabled(value) { setBoolean("showPerformanceOverlay", value) }
    function requestBack(clearToRoot) {
        emitEffect("back", ({ clearToRoot: languageClearOnBack || clearToRoot === true }))
    }

    Component.onCompleted: {
        if (preferences.captureSysKeysMode !== captureSystemKeysOff)
            preferences.lastEnabledCaptureSysKeysMode = preferences.captureSysKeysMode
    }
    Component.onDestruction: {
        if (preferences && preferences.save)
            preferences.save()
    }
}
