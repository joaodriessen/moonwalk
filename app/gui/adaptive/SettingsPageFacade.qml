import QtQml 2.15

// This is the only settings object that a painted adaptive surface may receive.
// It deliberately exposes scalar snapshots, option arrays, and closed commands;
// neither the context nor any backend singleton is reachable through it.
QtObject {
    readonly property bool routeActive: root.routeActive

    readonly property int windowModeValue: root.preferences.windowMode
    readonly property bool windowModeVisible: root.systemProperties.hasDesktopEnvironment
    readonly property bool windowModeSensitive: !root.systemProperties.rendererAlwaysFullScreen

    readonly property bool muteHostSpeakersValue: !root.preferences.playAudioOnHost
    readonly property bool muteHostSpeakersVisible: true
    readonly property bool muteHostSpeakersSensitive: true

    readonly property int languageValue: root.preferences.language
    readonly property bool languageVisible: true
    readonly property bool languageSensitive: true

    readonly property bool muteOnFocusLossValue: root.preferences.muteOnFocusLoss
    readonly property bool muteOnFocusLossVisible: root.systemProperties.hasDesktopEnvironment
    readonly property bool muteOnFocusLossSensitive: root.systemProperties.hasDesktopEnvironment

    readonly property bool quitAppAfterValue: root.preferences.quitAppAfter
    readonly property bool quitAppAfterVisible: true
    readonly property bool quitAppAfterSensitive: true

    readonly property int uiDisplayModeValue: root.preferences.uiDisplayMode
    readonly property bool uiDisplayModeVisible: root.systemProperties.hasDesktopEnvironment
    readonly property bool uiDisplayModeSensitive: root.systemProperties.hasDesktopEnvironment

    readonly property bool connectionWarningsValue: root.preferences.connectionWarnings
    readonly property bool connectionWarningsVisible: true
    readonly property bool connectionWarningsSensitive: true

    readonly property bool configurationWarningsValue: root.preferences.configurationWarnings
    readonly property bool configurationWarningsVisible: true
    readonly property bool configurationWarningsSensitive: true

    readonly property bool richPresenceValue: root.preferences.richPresence
    readonly property bool richPresenceVisible: root.systemProperties.hasDiscordIntegration
    readonly property bool richPresenceSensitive: root.systemProperties.hasDiscordIntegration

    readonly property bool keepAwakeValue: root.preferences.keepAwake
    readonly property bool keepAwakeVisible: true
    readonly property bool keepAwakeSensitive: true

    readonly property int captureSystemKeysModeValue: root.preferences.captureSysKeysMode
    readonly property bool captureSystemKeysModeVisible: true
    readonly property bool captureSystemKeysModeSensitive:
        root.systemProperties.hasDesktopEnvironment && captureSystemKeysValue

    readonly property bool captureSystemKeysValue:
        root.preferences.captureSysKeysMode !== root.captureSystemKeysOff || !root.systemProperties.hasDesktopEnvironment
    readonly property bool captureSystemKeysVisible: true
    readonly property bool captureSystemKeysSensitive: root.systemProperties.hasDesktopEnvironment

    readonly property bool virtualTrackpadValue: !root.preferences.absoluteTouchMode
    readonly property bool virtualTrackpadVisible: true
    readonly property bool virtualTrackpadSensitive: true

    readonly property bool swapMouseButtonsValue: root.preferences.swapMouseButtons
    readonly property bool swapMouseButtonsVisible: true
    readonly property bool swapMouseButtonsSensitive: true

    readonly property bool reverseScrollValue: root.preferences.reverseScrollDirection
    readonly property bool reverseScrollVisible: true
    readonly property bool reverseScrollSensitive: true

    readonly property bool swapFaceButtonsValue: root.preferences.swapFaceButtons
    readonly property bool swapFaceButtonsVisible: true
    readonly property bool swapFaceButtonsSensitive: true

    readonly property bool singleControllerValue: !root.preferences.multiController
    readonly property bool singleControllerVisible: true
    readonly property bool singleControllerSensitive: true

    readonly property bool gamepadMouseValue: root.preferences.gamepadMouse
    readonly property bool gamepadMouseVisible: true
    readonly property bool gamepadMouseSensitive: true

    readonly property bool backgroundGamepadValue: root.preferences.backgroundGamepad
    readonly property bool backgroundGamepadVisible: root.systemProperties.hasDesktopEnvironment
    readonly property bool backgroundGamepadSensitive: root.systemProperties.hasDesktopEnvironment

    readonly property bool mdnsValue: root.preferences.enableMdns
    readonly property bool mdnsVisible: true
    readonly property bool mdnsSensitive: true

    readonly property bool detectNetworkBlockingValue: root.preferences.detectNetworkBlocking
    readonly property bool detectNetworkBlockingVisible: true
    readonly property bool detectNetworkBlockingSensitive: true

    readonly property bool performanceOverlayValue: root.preferences.showPerformanceOverlay
    readonly property bool performanceOverlayVisible: true
    readonly property bool performanceOverlaySensitive: true

    // Qt 6.4 QML does not enforce Object.freeze() on var arrays. Each facade
    // therefore owns a detached scalar-only snapshot: a surface can mutate its
    // local copy without changing context state or a later facade instance.
    readonly property var windowModeOptions:
        root.copyOptionSnapshot(root.windowModeOptions)
    readonly property var languageOptions:
        root.copyOptionSnapshot(root.languageOptions)
    readonly property var uiDisplayModeOptions:
        root.copyOptionSnapshot(root.uiDisplayModeOptions)
    readonly property var captureSystemKeysModeOptions:
        root.copyOptionSnapshot(root.captureSystemKeysModeOptions)

    function setWindowMode(value) { root.setWindowMode(value) }
    function setMuteHostSpeakersEnabled(value) { root.setMuteHostSpeakersEnabled(value) }
    function setLanguage(value) { root.setLanguage(value) }
    function setMuteOnFocusLossEnabled(value) { root.setMuteOnFocusLossEnabled(value) }
    function setQuitAppAfterEnabled(value) { root.setQuitAppAfterEnabled(value) }
    function setUiDisplayMode(value) { root.setUiDisplayMode(value) }
    function setConnectionWarningsEnabled(value) { root.setConnectionWarningsEnabled(value) }
    function setConfigurationWarningsEnabled(value) { root.setConfigurationWarningsEnabled(value) }
    function setRichPresenceEnabled(value) { root.setRichPresenceEnabled(value) }
    function setKeepAwakeEnabled(value) { root.setKeepAwakeEnabled(value) }
    function setCaptureSystemKeysMode(value) { root.setCaptureSystemKeysMode(value) }
    function setCaptureSystemKeysEnabled(value) { root.setCaptureSystemKeysEnabled(value) }
    function setVirtualTrackpadEnabled(value) { root.setVirtualTrackpadEnabled(value) }
    function setSwapMouseButtonsEnabled(value) { root.setSwapMouseButtonsEnabled(value) }
    function setReverseScrollEnabled(value) { root.setReverseScrollEnabled(value) }
    function setSwapFaceButtonsEnabled(value) { root.setSwapFaceButtonsEnabled(value) }
    function setSingleControllerEnabled(value) { root.setSingleControllerEnabled(value) }
    function setGamepadMouseEnabled(value) { root.setGamepadMouseEnabled(value) }
    function setBackgroundGamepadEnabled(value) { root.setBackgroundGamepadEnabled(value) }
    function setMdnsEnabled(value) { root.setMdnsEnabled(value) }
    function setDetectNetworkBlockingEnabled(value) { root.setDetectNetworkBlockingEnabled(value) }
    function setPerformanceOverlayEnabled(value) { root.setPerformanceOverlayEnabled(value) }
    function requestBack(clearToRoot) { root.requestBack(clearToRoot) }
}
