import QtQuick 2.15
import QtTest 1.3
import "TestCompat.js" as TestCompat
import "../../app/gui/adaptive" as Adaptive
import Starlight.Test 1.0

TestCase {
    id: testCase
    name: "FacadeBoundary"

    Component {
        id: hostContextComponent
        Adaptive.HostPageContext { hostSource: TestHostSource { } }
    }

    Component {
        id: settingsContextComponent
        Adaptive.SettingsPageContext { }
    }

    function expectedFacadeNames(properties, commands) {
        var names = ["objectName", "objectNameChanged"]
        properties.forEach(function(property) {
            names.push(property)
            names.push(property + "Changed")
        })
        commands.forEach(function(command) { names.push(command) })
        return names.sort()
    }

    function assertExactFacadeApi(facade, properties, commands) {
        compare(TestCompat._propertyNames(facade),
                expectedFacadeNames(properties, commands))
    }

    function assertObjectValuesCannotLeakBackend(facade, forbidden) {
        TestCompat.walkQObjectGraph(facade).forEach(function(object) {
            forbidden.forEach(function(name) {
                verify(!TestCompat.hasMethod(object, name),
                       "facade leaked " + name)
            })
        })
    }

    function test_host_facade_has_exact_closed_api_and_no_backend_leak() {
        const hostContext = createTemporaryObject(hostContextComponent, testCase)
        verify(hostContext !== null)
        assertExactFacadeApi(hostContext.facade,
                              ["hosts", "selectedHostUuid", "selectedHostIndex",
                               "selectedHostSnapshot", "selectedHostKind",
                               "resolutionOptions", "frameRateOptions", "displayRefreshRate",
                               "hostPreferenceSnapshot", "streamPlan", "baseOverlay",
                               "interruptOverlay", "busy", "routeActive", "initialDiscoveryElapsed", "waitingForWake", "presets"],
                              ["selectHostByUuid", "selectHostAtIndex", "beginPairing", "cancelPairing",
                               "submitAddHost", "testConnection", "wakeHost",
                               "requestConnect", "requestDetails", "requestOpenSettings",
                               "requestAddHostOverlay", "confirmQuitAndConnect",
                               "quitRunningAppForOperation", "rejectQuit", "dismissOverlay",
                               "requestRename", "submitRename", "requestDelete",
                               "confirmDelete", "requestCustomResolution",
                               "submitCustomResolution", "requestCustomFrameRate",
                               "submitCustomFrameRate", "setResolution", "setFrameRate",
                               "setAutomaticResolution", "setAutomaticFrameRate", "setDisplayTarget", "setAutomaticPointer",
                               "setBitrateKbps", "setAutoAdjustBitrate", "setStreamMode", "setDisplayMode", "setInputMode", "setBandwidthLimit",
                               "setLocalCursorEnabled", "setVideoCodec",
                               "setAbsoluteMouseMode", "cancelWakeConnect",
                               "requestResume", "requestEndSession", "confirmEndSession", "setWakeTimeout",
                               "savePreset", "applyPreset", "renamePreset", "duplicatePreset", "removePreset",
                               "restoreSuccessfulSettings", "resetLearnedPerformance", "requestPresets", "setConnectionAddress", "setDesktopScale", "setViewScale",
                               "setAdaptiveBitrate", "setLocalTextInput", "setQualityMode",
                               "submitConnectionAddress"])
        assertObjectValuesCannotLeakBackend(hostContext.facade,
                                             ["pair", "wake", "deleteComputer",
                                              "createSessionForDesktop", "quitRunningApp"])
    }

    function test_settings_facade_has_exact_closed_api_and_no_backend_leak() {
        const settingsContext = createTemporaryObject(settingsContextComponent, testCase)
        verify(settingsContext !== null)
        const bases = ["windowMode", "muteHostSpeakers", "language",
                       "muteOnFocusLoss", "quitAppAfter", "uiDisplayMode",
                       "connectionWarnings", "configurationWarnings", "richPresence",
                       "keepAwake", "captureSystemKeysMode", "captureSystemKeys",
                       "virtualTrackpad", "swapMouseButtons", "reverseScroll",
                       "swapFaceButtons", "singleController", "gamepadMouse",
                       "backgroundGamepad", "mdns", "detectNetworkBlocking",
                       "performanceOverlay"]
        var properties = ["routeActive", "windowModeOptions", "languageOptions",
                          "uiDisplayModeOptions", "captureSystemKeysModeOptions"]
        bases.forEach(function(base) {
            properties.push(base + "Value")
            properties.push(base + "Visible")
            properties.push(base + "Sensitive")
        })
        assertExactFacadeApi(settingsContext.facade, properties,
                              ["setWindowMode", "setMuteHostSpeakersEnabled", "setLanguage",
                               "setMuteOnFocusLossEnabled", "setQuitAppAfterEnabled",
                               "setUiDisplayMode", "setConnectionWarningsEnabled",
                               "setConfigurationWarningsEnabled", "setRichPresenceEnabled",
                               "setKeepAwakeEnabled", "setCaptureSystemKeysMode",
                               "setCaptureSystemKeysEnabled", "setVirtualTrackpadEnabled",
                               "setSwapMouseButtonsEnabled", "setReverseScrollEnabled",
                               "setSwapFaceButtonsEnabled", "setSingleControllerEnabled",
                               "setGamepadMouseEnabled", "setBackgroundGamepadEnabled",
                               "setMdnsEnabled", "setDetectNetworkBlockingEnabled",
                               "setPerformanceOverlayEnabled", "requestBack"])
        assertObjectValuesCannotLeakBackend(settingsContext.facade,
                                             ["save", "changeLanguage", "startPolling",
                                              "stopPollingAsync", "setUiNavMode"])
    }

    function test_settings_facade_values_have_closed_scalar_types() {
        const settingsContext = createTemporaryObject(settingsContextComponent,
                                                      testCase)
        verify(settingsContext !== null)
        const facade = settingsContext.facade
        compare(typeof facade.routeActive, "boolean")
        const integerValues = ["windowMode", "language", "uiDisplayMode",
                               "captureSystemKeysMode"]
        const booleanValues = ["muteHostSpeakers", "muteOnFocusLoss",
                               "quitAppAfter", "connectionWarnings",
                               "configurationWarnings", "richPresence",
                               "keepAwake", "captureSystemKeys",
                               "virtualTrackpad", "swapMouseButtons",
                               "reverseScroll", "swapFaceButtons",
                               "singleController", "gamepadMouse",
                               "backgroundGamepad", "mdns",
                               "detectNetworkBlocking", "performanceOverlay"]
        integerValues.forEach(function(base) {
            compare(typeof facade[base + "Value"], "number")
        })
        booleanValues.forEach(function(base) {
            compare(typeof facade[base + "Value"], "boolean")
        })
        integerValues.concat(booleanValues).forEach(function(base) {
            compare(typeof facade[base + "Visible"], "boolean")
            compare(typeof facade[base + "Sensitive"], "boolean")
        })
        const optionProperties = ["windowModeOptions", "languageOptions",
                                  "uiDisplayModeOptions",
                                  "captureSystemKeysModeOptions"]
        optionProperties.forEach(function(propertyName) {
            const options = facade[propertyName]
            verify(options instanceof Array)
            options.forEach(function(option) {
                compare(Object.keys(option).sort(), ["text", "value"])
                compare(typeof option.text, "string")
                compare(typeof option.value, "number")
            })
        })
    }

    function test_facade_graph_cannot_reach_backend() {
        const context = createTemporaryObject(hostContextComponent, testCase)
        verify(context !== null)
        const forbidden = ["pair", "wake", "deleteComputer",
                           "createSessionForDesktop", "quitRunningApp"]
        assertObjectValuesCannotLeakBackend(context.facade, forbidden)
        context.destroy()
    }
}
