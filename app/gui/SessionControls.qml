import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Starlight 1.0
import "starlight"
import "adaptive/gnome" as AdaptiveGnome
import "adaptive/settings" as Settings

// The in-stream panel. Written once in plain Controls: the Adwaita style or
// Breeze paints it, so every desktop gets the same capabilities natively.
ApplicationWindow {
    id: panel
    property var session
    // The launcher's tile for this host; a command-line launch only has its name.
    property string hostSeed: ""
    property string hostIcon: ""
    readonly property var state: session ? session.controlState : ({})
    title: qsTr("Stream Controls")
    transientParent: null // The launcher is hidden while SDL owns the stream window.
    // Older Qt auto-parenting runs after declarative property assignments.
    Component.onCompleted: transientParent = null
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    width: Math.min(520, Screen.desktopAvailableWidth * 0.9)
    height: Math.min(580, Screen.desktopAvailableHeight * 0.85)
    minimumWidth: Math.min(380, Screen.desktopAvailableWidth)
    minimumHeight: 320
    font.family: Qt.application.font.family
    font.pixelSize: Math.max(14, Qt.application.font.pixelSize > 0 ? Qt.application.font.pixelSize
                             : Qt.application.font.pointSize * 96 / 72)
    color: palette.window
    visible: !!session && session.controlsOpen
    onActiveChanged: if (active && visible && session) session.prepareControlsClipboard()
    readonly property bool nativeStyle: typeof plasmaDesktopAdapterActive !== "undefined" && plasmaDesktopAdapterActive
    readonly property AdaptiveGnome.GnomeTokens tokens: AdaptiveGnome.GnomeTokens {}
    readonly property color goodColor: tokens.success
    readonly property color warningColor: tokens.warning
    SystemPalette { id: nativePalette; colorGroup: SystemPalette.Active }
    palette.window: nativeStyle ? nativePalette.window : tokens.window
    palette.windowText: nativeStyle ? nativePalette.windowText : tokens.text
    palette.text: nativeStyle ? nativePalette.text : tokens.text
    palette.base: nativeStyle ? nativePalette.base : tokens.view
    palette.button: nativeStyle ? nativePalette.button : tokens.card
    palette.buttonText: nativeStyle ? nativePalette.buttonText : tokens.text
    palette.highlight: nativeStyle ? nativePalette.highlight : tokens.accent
    palette.highlightedText: nativeStyle ? nativePalette.highlightedText : tokens.accentInk
    background: Rectangle { color: panel.palette.window }
    Loader {
        active: typeof materialStyleActive !== "undefined" && materialStyleActive
        source: "starlight/MaterialPaletteBridge.qml"
        onLoaded: { item.target = panel }
    }
    onVisibleChanged: if (visible) {
        x = Screen.virtualX + Math.max(0, (Screen.width - width) / 2)
        y = Screen.virtualY + Math.max(0, (Screen.height - height) / 3)
        requestActivate()
        returnButton.forceActiveFocus()
    }
    onClosing: function(close) {
        if (session) session.closeControls()
    }
    Shortcut { sequence: "Escape"; enabled: panel.visible; onActivated: if (session) session.closeControls() }
    function act(action, value) { if (session) session.controlAction(action, value) }
    function hostName() { return panel.state.host || qsTr("the host") }

    // How the stream is doing, read once a second while this panel is showing.
    property var liveHealth: ({ rttMs: -1, fps: -1, networkPoor: false })
    property double lastFrames: -1
    property double lastFramesAt: 0
    function refreshLiveHealth() {
        if (!session || !session.liveStats) return
        var stats = session.liveStats()
        var now = Date.now()
        var fps = -1
        if (lastFrames >= 0 && now > lastFramesAt && stats.presentedFrames >= lastFrames)
            fps = Math.round((stats.presentedFrames - lastFrames) * 1000 / (now - lastFramesAt))
        lastFrames = stats.presentedFrames
        lastFramesAt = now
        liveHealth = { rttMs: stats.rttMs, fps: fps, networkPoor: stats.networkPoor === true }
    }
    Timer {
        interval: 1000
        repeat: true
        running: panel.visible
        triggeredOnStart: true
        onTriggered: panel.refreshLiveHealth()
    }
    readonly property string healthText: {
        var parts = []
        if (liveHealth.rttMs >= 0) parts.push(qsTr("%1 ms").arg(liveHealth.rttMs))
        if (liveHealth.fps >= 0) parts.push(qsTr("%1 FPS").arg(liveHealth.fps))
        return parts.join(" · ")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Settings.HostAvatar {
                objectName: "starlight.controls.avatar"
                size: 44
                seed: panel.hostSeed.length > 0 ? panel.hostSeed : (panel.state.host || "")
                iconName: panel.hostIcon.length > 0 ? panel.hostIcon : "monitor"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: (panel.state.host || "") + " · " + (panel.state.app || "")
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    text: (panel.state.format || "") + (panel.state.mediaProtection ? " · " + panel.state.mediaProtection : "")
                    visible: text !== ""
                    opacity: 0.7
                    wrapMode: Text.Wrap
                }
            }
            // Live round trip and frame rate, with a plain word when the network struggles.
            ColumnLayout {
                objectName: "starlight.controls.health"
                visible: panel.healthText.length > 0
                spacing: 2
                Accessible.role: Accessible.StaticText
                Accessible.name: (panel.liveHealth.networkPoor ? qsTr("Slow connection") : qsTr("Good connection")) + ", " + panel.healthText
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: panel.liveHealth.networkPoor ? qsTr("Slow connection") : qsTr("Good connection")
                    font.bold: true
                    color: panel.liveHealth.networkPoor ? panel.warningColor : panel.goodColor
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: panel.healthText
                    opacity: 0.7
                }
            }
        }
        RowLayout {
            visible: panel.state.optimizationAvailable === true
            Layout.fillWidth: true
            Label {
                objectName: "starlight.controls.optimization-notice"
                Layout.fillWidth: true
                text: panel.state.optimizationNotice || ""
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "starlight.controls.apply-optimization"
                text: qsTr("Apply Now")
                onClicked: panel.act("applyOptimization")
            }
        }
        TabBar {
            id: tabs
            objectName: "starlight.controls.tabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("Session") }
            TabButton { text: qsTr("Input") }
            TabButton { text: qsTr("Clipboard") }
            TabButton { text: qsTr("Files") }
        }
        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    SessionRow {
                        title: qsTr("Mute audio")
                        Switch { objectName: "starlight.controls.mute"; checked: !!panel.state.muted; Accessible.name: qsTr("Mute audio"); onClicked: panel.act("mute", checked) }
                    }
                    SessionRow {
                        title: qsTr("Fullscreen")
                        Switch { objectName: "starlight.controls.fullscreen"; checked: !!panel.state.fullscreen; Accessible.name: qsTr("Fullscreen"); onClicked: panel.act("fullscreen", checked) }
                    }
                    SessionRow {
                        title: qsTr("View scale")
                        subtitle: panel.state.viewScale === "actual"
                                  ? qsTr("Release input, then scroll to move around the picture. Click the picture to control the host again.") : ""
                        ComboBox {
                            objectName: "starlight.controls.view-scale"
                            model: [qsTr("Fit in Window"), qsTr("Actual Size")]
                            currentIndex: panel.state.viewScale === "actual" ? 1 : 0
                            Accessible.name: qsTr("View scale")
                            onActivated: panel.act("viewScale", currentIndex === 1 ? "actual" : "fit")
                        }
                    }
                    SessionRow {
                        visible: !!panel.state.automaticBitrate
                        title: qsTr("Video quality")
                        subtitle: panel.state.automaticBitrateDeferred
                            ? qsTr("Automatic bitrate changes will apply the next time you connect.")
                            : qsTr("Moonwalk can adjust this bitrate target during the stream.")
                        Label {
                            objectName: "starlight.controls.automatic-bitrate"
                            text: panel.state.automaticBitrateKbps > 0 ? qsTr("%1 Mbps").arg((panel.state.automaticBitrateKbps / 1000).toFixed(1)) : qsTr("Measuring…")
                            font.bold: true
                        }
                    }
                    SessionRow {
                        visible: !!panel.state.liveBitrateSupported
                        title: qsTr("Bitrate")
                        subtitle: panel.state.liveBitrateNotice
                                  || (panel.state.liveBitrateRebuilds
                                      ? qsTr("The host applies the bitrate when you release the slider. Video may pause briefly.")
                                      : qsTr("Changes the bitrate during the stream."))
                        Label {
                            objectName: "starlight.controls.bitrate-value"
                            text: qsTr("%1 Mbps").arg((bitrateValue.kbps / 1000).toFixed(1))
                            font.bold: true
                        }
                    }
                    // In the unit people think in, sent as soon as the slider settles.
                    Slider {
                        id: bitrateValue
                        objectName: "starlight.controls.bitrate"
                        visible: !!panel.state.liveBitrateSupported
                        Layout.fillWidth: true
                        from: 500; to: Math.max(150000, panel.state.liveBitrateKbps || 0)
                        // Breeze draws a tick for every step, so it gets no steps to draw.
                        stepSize: panel.nativeStyle ? 0 : 500
                        readonly property int kbps: Math.round(value / 500) * 500
                        // The request the host has not answered yet: the slider stays on it
                        // rather than falling back while the host thinks.
                        property int pendingKbps: -1
                        function follow() {
                            if (pressed || bitrateCommit.running) return
                            var live = panel.state.liveBitrateKbps || 10000
                            if (pendingKbps >= 0 && live !== pendingKbps && !panel.state.liveBitrateNotice && bitrateAnswer.running) return
                            pendingKbps = -1
                            bitrateAnswer.stop()
                            value = live
                        }
                        function stepBy(steps) {
                            value = Math.max(from, Math.min(to, kbps + steps * 1000))
                            bitrateCommit.restart()
                        }
                        Component.onCompleted: follow()
                        Connections {
                            target: panel
                            function onStateChanged() { bitrateValue.follow() }
                        }
                        Accessible.name: qsTr("Bitrate in kilobits per second")
                        Keys.onLeftPressed: stepBy(mirrored ? 1 : -1)
                        Keys.onRightPressed: stepBy(mirrored ? -1 : 1)
                        Keys.onUpPressed: stepBy(1)
                        Keys.onDownPressed: stepBy(-1)
                        WheelHandler { onWheel: function(event) { bitrateValue.stepBy(event.angleDelta.y > 0 ? 1 : -1) } }
                        // A host that rebuilds its encoder freezes for every value it is sent,
                        // so it hears one per release, and a run of key steps settles first.
                        readonly property bool rebuilds: !!panel.state.liveBitrateRebuilds
                        onMoved: if (!pressed || !rebuilds) bitrateCommit.restart()
                        onPressedChanged: {
                            if (!pressed) bitrateCommit.restart()
                            else if (rebuilds) bitrateCommit.stop()
                        }
                        Timer {
                            id: bitrateCommit
                            interval: bitrateValue.rebuilds ? 1200 : 350
                            onTriggered: {
                                if (bitrateValue.pressed && bitrateValue.rebuilds) return
                                if (bitrateValue.kbps === panel.state.liveBitrateKbps) {
                                    bitrateValue.pendingKbps = -1
                                    return
                                }
                                bitrateValue.pendingKbps = bitrateValue.kbps
                                bitrateAnswer.restart()
                                panel.act("bitrate", bitrateValue.kbps)
                            }
                        }
                        // A host that never answers does not hold the slider for ever.
                        Timer {
                            id: bitrateAnswer
                            interval: 8000
                            onTriggered: bitrateValue.follow()
                        }
                    }
                    SessionRow {
                        visible: !!panel.state.safeTrial
                        title: qsTr("Safe settings")
                        subtitle: qsTr("Save this working configuration for your next connection.")
                        Button {
                            objectName: "starlight.controls.keep-safe"
                            enabled: !!panel.state.hasVideo && !panel.state.safeKept
                            text: panel.state.safeKept ? qsTr("Saved") : qsTr("Keep for This Host")
                            onClicked: panel.act("keepSafeSettings", true)
                        }
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    SessionRow {
                        title: qsTr("Capture input on return")
                        Switch { objectName: "starlight.controls.capture"; checked: !!panel.state.capture; Accessible.name: qsTr("Capture input on return"); onClicked: panel.act("capture", checked) }
                    }
                    SessionRow {
                        title: qsTr("Desktop mouse mode")
                        subtitle: qsTr("Match the host pointer to your local pointer. Turn off for relative mouse input in games.")
                        Switch { checked: !!panel.state.absolute; Accessible.name: qsTr("Desktop mouse mode"); onClicked: panel.act("absolute", checked) }
                    }
                    SessionRow {
                        title: qsTr("Send system shortcuts to %1").arg(panel.hostName())
                        Switch { checked: !!panel.state.systemKeys; Accessible.name: qsTr("Send system shortcuts to the host"); onClicked: panel.act("systemKeys", checked) }
                    }
                    SessionRow {
                        title: qsTr("Use local keyboard for typing")
                        subtitle: qsTr("Use this computer’s keyboard layout and input methods for accented characters and languages. Turn off for games. Shortcuts still use physical keys.")
                        Switch { checked: !!panel.state.localText; Accessible.name: qsTr("Use local keyboard for typing"); onClicked: panel.act("localText", checked) }
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    SessionRow {
                        title: qsTr("Copy and paste")
                        subtitle: panel.state.imageClipboard ? qsTr("Copy text and PNG images between computers. Images can be up to 4 MiB and 4096 pixels per side.") : qsTr("Text only: image clipboard is unavailable for this connection.")
                        ComboBox {
                            objectName: "starlight.controls.clipboard-direction"
                            model: [qsTr("Paused"), qsTr("Both directions"), qsTr("This computer → host"), qsTr("Host → this computer")]
                            currentIndex: panel.state.clipboardDirection === undefined ? 1 : panel.state.clipboardDirection
                            onActivated: panel.act("clipboardDirection", currentIndex)
                            Accessible.name: qsTr("Clipboard direction")
                        }
                    }
                    Label { visible: !!panel.state.clipboardNotice; text: panel.state.clipboardNotice || ""; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Label {
                        visible: panel.state.clipboardReadAllowed === false || panel.state.clipboardWriteAllowed === false
                        text: panel.state.clipboardReadAllowed === false && panel.state.clipboardWriteAllowed === false
                            ? qsTr("Clipboard access is disabled for this device in the host settings.")
                            : panel.state.clipboardReadAllowed === false
                              ? qsTr("The host allows sending text, but reading its clipboard is disabled.")
                              : qsTr("The host allows receiving text, but writing its clipboard is disabled.")
                        wrapMode: Text.Wrap; Layout.fillWidth: true
                    }
                }
            }
            ScrollView {
                clip: true
                contentWidth: availableWidth
                FileTransferPane {
                    id: files
                    objectName: "starlight.controls.files"
                    width: parent.width
                    transfer: panel.session ? panel.session.fileTransfer : null
                    active: !!panel.session && panel.session.controlsOpen && tabs.currentIndex === 3
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Remote input is paused. Esc returns to the stream.")
            wrapMode: Text.Wrap
            opacity: 0.7
        }
        // Leaving is always one click away; Disconnect leaves what is running on
        // the host intact, Return is the default.
        Flow {
            Layout.fillWidth: true
            spacing: 8
            layoutDirection: Qt.application.layoutDirection
            Button {
                objectName: "starlight.controls.disconnect"
                text: qsTr("Disconnect")
                // Lucide's glyph as a theme icon, so each style colours it like its own.
                icon.name: panel.nativeStyle ? "lucide-unplug" : ""
                // Only the Adwaita style has a destructive variant to ask for.
                Component.onCompleted: if ("destructive" in this) this.destructive = true
                onClicked: panel.act("disconnect", true)
            }
            Button {
                objectName: "starlight.controls.release"
                text: qsTr("Release Input")
                onClicked: { panel.act("capture", false); if (session) session.closeControls() }
            }
            Button {
                id: returnButton
                objectName: "starlight.controls.return"
                text: qsTr("Return to Stream")
                highlighted: true
                onClicked: session.closeControls()
            }
        }
    }
    Connections {
        target: panel.session
        ignoreUnknownSignals: true
        function onFileDropRequested(file) { tabs.currentIndex = 3; files.useSource(file.toString()) }
        function onFilesDropRequested(locations) {
            files.sendDroppedFiles(locations)
            tabs.currentIndex = 3
        }
    }
}
