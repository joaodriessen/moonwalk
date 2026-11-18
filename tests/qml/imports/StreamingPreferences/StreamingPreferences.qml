pragma Singleton
import QtQml 2.15

// Value-only boundary adapter. Preference persistence is characterized by the
// C++ suite using an explicit temporary INI store.
QtObject {
    enum UiDisplayMode {
        UI_WINDOWED,
        UI_FULLSCREEN,
        UI_MAXIMIZED
    }
    enum VideoDecoderSelection {
        VDS_AUTO,
        VDS_FORCE_SOFTWARE,
        VDS_FORCE_HARDWARE
    }
    enum VideoCodecConfig {
        VCC_AUTO,
        VCC_FORCE_H264,
        VCC_FORCE_HEVC,
        VCC_FORCE_AV1
    }
    enum AudioConfig {
        AC_STEREO,
        AC_51_SURROUND,
        AC_71_SURROUND
    }
    enum WindowMode {
        WM_FULLSCREEN,
        WM_FULLSCREEN_DESKTOP,
        WM_WINDOWED
    }
    enum CaptureSysKeysMode {
        CSK_OFF,
        CSK_FULLSCREEN,
        CSK_ALWAYS
    }
    enum Language {
        LANG_AUTO, LANG_EN, LANG_FR, LANG_ZH_CN, LANG_DE, LANG_NB_NO,
        LANG_RU, LANG_ES, LANG_JA, LANG_VI, LANG_TH, LANG_KO, LANG_HU,
        LANG_NL, LANG_SV, LANG_TR, LANG_UK, LANG_ZH_TW, LANG_PT,
        LANG_PT_BR, LANG_EL, LANG_IT, LANG_HI, LANG_PL, LANG_CS, LANG_HE,
        LANG_CKB, LANG_LT, LANG_ET, LANG_BG, LANG_EO, LANG_TA
    }

    property int width: 1920
    property int height: 1080
    property int fps: 60
    property int bitrateKbps: 10000
    property bool unlockBitrate: false
    property bool autoAdjustBitrate: true
    property bool enableVsync: false
    property bool framePacing: true
    property bool localCursor: false
    property bool absoluteMouseMode: false
    property bool automaticResolution: false
    property bool automaticFrameRate: false
    property bool automaticPointer: false
    property string displayTarget: "host"
    property int audioConfig: 0
    property int videoCodecConfig: 0
    property int videoDecoderSelection: 0
    property bool enableHdr: false
    property bool enableYUV444: false
    property string connectionAddress: ""
    property string viewScale: "fit"
    property string desktopScale: "auto"
    property bool adaptiveBitrate: false
    property bool manualStream: false
    property bool manualDisplay: false
    property bool manualInput: false
    property bool localTextInput: false
    property string activePresetName: "Custom"
    property int lastConnectedAt: 0
    property var presetList: []
    property var presetValues: ({})
    function hostPresets() { return presetList }
    function applyHostPreset(id) {
        var values = presetValues[id]
        if (!values) return false
        for (var key in values) this[key] = values[key]
        save()
        return true
    }
    property bool enableMdns: true
    property int uiDisplayMode: 0
    property bool muteOnFocusLoss: false
    property bool quitAppAfter: false
    property bool connectionWarnings: true
    property bool configurationWarnings: true
    property bool richPresence: false
    property bool keepAwake: true
    property bool absoluteTouchMode: false
    property bool swapMouseButtons: false
    property bool reverseScrollDirection: false
    property bool swapFaceButtons: false
    property bool multiController: true
    property bool gamepadMouse: false
    property bool backgroundGamepad: false
    property bool detectNetworkBlocking: true
    property bool showPerformanceOverlay: false
    property bool playAudioOnHost: false
    property int windowMode: 2 // WM_WINDOWED
    property int recommendedFullScreenMode: 1 // WM_FULLSCREEN_DESKTOP
    property int lastEnabledCaptureSysKeysMode: 1
    property int captureSysKeysMode: 1 // CSK_FULLSCREEN
    property int language: 0 // LANG_AUTO
    property int stubDefaultBitrate: 10000
    property int defaultBitrateCalls: 0
    property int lastBitrateWidth: -1
    property int lastBitrateHeight: -1
    property int lastBitrateFps: -1
    property bool lastBitrateYuv444: false
    property int saveCalls: 0
    property string currentHost: ""
    property var currentHostHistory: []
    property var storedResolutions: ({})
    property var hostWidths: ({})
    property var hostHeights: ({})
    property var hostViewScales: ({})
    property var hostLocalCursors: ({})
    property var hostAbsoluteMouseModes: ({})
    property var initializationHistory: []
    property var operationHistory: []

    function reset() {
        lastHost = ""
        automaticResolution = false
        automaticFrameRate = false
        automaticPointer = false
        displayTarget = "host"
        width = 1920
        height = 1080
        fps = 60
        bitrateKbps = 10000
        unlockBitrate = false
        autoAdjustBitrate = true
        enableVsync = false
        framePacing = true
        localCursor = false
        absoluteMouseMode = false
        audioConfig = 0
        videoCodecConfig = 0
        videoDecoderSelection = 0
        enableHdr = false
        enableYUV444 = false
        connectionAddress = ""
        viewScale = "fit"
        desktopScale = "auto"
        adaptiveBitrate = false
        manualStream = false
        manualDisplay = false
        manualInput = false
        localTextInput = false
        activePresetName = "Custom"
        presetList = []
        presetValues = ({})
        enableMdns = true
        uiDisplayMode = 0
        muteOnFocusLoss = false
        quitAppAfter = false
        connectionWarnings = true
        configurationWarnings = true
        richPresence = false
        keepAwake = true
        absoluteTouchMode = false
        swapMouseButtons = false
        reverseScrollDirection = false
        swapFaceButtons = false
        multiController = true
        gamepadMouse = false
        backgroundGamepad = false
        detectNetworkBlocking = true
        showPerformanceOverlay = false
        playAudioOnHost = false
        windowMode = 2 // WM_WINDOWED
        recommendedFullScreenMode = 1 // WM_FULLSCREEN_DESKTOP
        lastEnabledCaptureSysKeysMode = 1
        captureSysKeysMode = 1 // CSK_FULLSCREEN
        language = 0 // LANG_AUTO
        stubDefaultBitrate = 10000
        defaultBitrateCalls = 0
        lastBitrateWidth = -1
        lastBitrateHeight = -1
        lastBitrateFps = -1
        lastBitrateYuv444 = false
        saveCalls = 0
        currentHost = ""
        currentHostHistory = []
        storedResolutions = ({})
        hostWidths = ({})
        hostHeights = ({})
        hostViewScales = ({})
        hostLocalCursors = ({})
        hostAbsoluteMouseModes = ({})
        initializationHistory = []
        operationHistory = []
    }

    function effectiveBitrate(value) { return Math.max(500, Math.min(value, unlockBitrate ? 500000 : 150000)) }

    function getDefaultBitrate(width, height, fps, yuv444) {
        defaultBitrateCalls += 1
        lastBitrateWidth = width
        lastBitrateHeight = height
        lastBitrateFps = fps
        lastBitrateYuv444 = yuv444
        return stubDefaultBitrate
    }

    function hasStoredResolution(uuid) {
        return storedResolutions[uuid] === true
    }

    property string lastHost: ""
    function lastSelectedHost() { return lastHost }
    function setLastSelectedHost(uuid) { lastHost = uuid }

    function setCurrentHost(uuid) {
        operationHistory = operationHistory.concat(["host:" + uuid])
        currentHost = uuid
        currentHostHistory = currentHostHistory.concat([uuid])
        if (uuid && hostWidths[uuid] !== undefined) {
            width = hostWidths[uuid]
            height = hostHeights[uuid]
        } else {
            width = 1920
            height = 1080
        }
        viewScale = uuid && hostViewScales[uuid] ? hostViewScales[uuid] : "fit"
        localCursor = uuid && hostLocalCursors[uuid] === true
        absoluteMouseMode = uuid && hostAbsoluteMouseModes[uuid] === true
    }

    function initializeHostDisplayMode(uuid, nativeWidth, nativeHeight, nativeRefreshRate) {
        initializationHistory = initializationHistory.concat([{
            uuid: uuid,
            width: nativeWidth,
            height: nativeHeight,
            fps: nativeRefreshRate
        }])
        operationHistory = operationHistory.concat(["initialize:" + uuid])
        if (!storedResolutions[uuid]) {
            width = nativeWidth
            height = nativeHeight
            fps = Math.max(10, Math.min(nativeRefreshRate > 0 ? nativeRefreshRate : 60, 480))
            automaticFrameRate = true
            var resolutions = storedResolutions
            resolutions[uuid] = true
            storedResolutions = resolutions
            var widths = hostWidths
            var heights = hostHeights
            widths[uuid] = width
            heights[uuid] = height
            hostWidths = widths
            hostHeights = heights
            return true
        }
        return false
    }

    function setResolution(newWidth, newHeight) {
        if (newWidth <= 0 || newHeight <= 0 ||
                (width === newWidth && height === newHeight)) {
            return false
        }

        width = newWidth
        height = newHeight
        var resolutions = storedResolutions
        resolutions[currentHost] = true
        storedResolutions = resolutions
        var widths = hostWidths
        var heights = hostHeights
        widths[currentHost] = newWidth
        heights[currentHost] = newHeight
        hostWidths = widths
        hostHeights = heights
        if (autoAdjustBitrate) {
            bitrateKbps = getDefaultBitrate(width, height, fps, enableYUV444)
        }
        save()
        return true
    }

    function setFrameRate(newFps) {
        automaticFrameRate = false
        if (newFps <= 0 || fps === newFps) {
            return false
        }

        fps = newFps
        if (autoAdjustBitrate) {
            bitrateKbps = getDefaultBitrate(width, height, fps, enableYUV444)
        }
        save()
        return true
    }

    function setAutomaticFrameRate(nativeRefreshRate) {
        fps = Math.max(10, Math.min(nativeRefreshRate > 0 ? nativeRefreshRate : 60, 480))
        automaticFrameRate = true
        save()
        return true
    }

    function save() {
        operationHistory = operationHistory.concat(["save:" + currentHost])
        if (currentHost) {
            var scales = hostViewScales
            scales[currentHost] = viewScale
            hostViewScales = scales
            var cursors = hostLocalCursors
            var absoluteModes = hostAbsoluteMouseModes
            cursors[currentHost] = localCursor
            absoluteModes[currentHost] = absoluteMouseMode
            hostLocalCursors = cursors
            hostAbsoluteMouseModes = absoluteModes
        }
        saveCalls += 1
    }
}
