pragma Singleton
import QtQuick 2.15

QtObject {
    signal displayConfigurationChanged()
    property bool decoderProbeReady: false
    property string decoderProbeError: ""
    property bool decoderProbeRunning: false
    signal decoderProbeStateChanged()
    onDecoderProbeErrorChanged: decoderProbeStateChanged()
    onDecoderProbeRunningChanged: decoderProbeStateChanged()
    property bool hasDesktopEnvironment: true
    property bool isWow64: false
    property bool hasHardwareAcceleration: true
    property bool hasDiscordIntegration: false
    property bool isRunningXWayland: false
    property string unmappedGamepads: ""
    property string friendlyNativeArchName: "x86_64"
    property bool hasHwHevcOrAv1Decode: true
    property int currentDisplay: 0
    property var displays: []
    function displayIndexForWindow(window) { return currentDisplay }
    property int nativeWidth: 1920
    property int nativeHeight: 1080
    property int nativeRefreshRate: 60
    property int refreshDisplayCalls: 0
    property bool supportsHdr: false
    property string starlightVersionString: "test"

    function refreshDisplays() {
        refreshDisplayCalls += 1
    }

    function getNativeResolution(index) {
        return displays.length ? Qt.size(displays[index].width, displays[index].height) : Qt.size(nativeWidth, nativeHeight)
    }

    function getRefreshRate(index) {
        return displays.length ? displays[index].rate : nativeRefreshRate
    }

    function startAsyncLoad() {
    }

    function waitForAsyncLoad() {
    }
}
