import QtQml 2.15

QtObject {
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

    function command(name, value) { commands = commands.concat([{ name: name, value: value }]) }
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
    function requestBack(clearToRoot) { command("requestBack", clearToRoot) }
}
