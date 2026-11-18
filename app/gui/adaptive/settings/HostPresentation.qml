import QtQuick 2.15

// What the selected host means to the person using Starlight: its state in
// words, the one thing to do next, and every per-host setting. Non-painted and
// reached only through the page facade, so GNOME, Plasma and the generic
// surface cannot drift apart.
QtObject {
    id: root
    required property var page

    readonly property var host: page.selectedHostSnapshot || ({})
    readonly property var prefs: page.hostPreferenceSnapshot || ({})
    readonly property bool hasHost: !!host.uuid

    // ---- State in words ---------------------------------------------------
    readonly property bool checking: hasHost && host.statusUnknown === true
    readonly property bool online: host.online === true
    readonly property bool paired: host.paired === true
    readonly property bool supported: host.serverSupported !== false
    readonly property bool resumeDesktop: host.runningAppId > 0 && host.runningAppId === host.desktopAppId
    readonly property bool switchToDesktop: host.runningAppId > 0 && host.desktopAppId >= 0 && !resumeDesktop
    readonly property bool sessionActionsAvailable: online && paired && !checking && host.runningAppId > 0

    function statusFor(isOnline, isPaired, unknown, isSupported, busy) {
        if (unknown) return qsTranslate("PcView", "Checking…")
        if (!isOnline) return qsTranslate("PcView", "Offline")
        if (!isPaired) return qsTranslate("PcView", "Needs pairing")
        if (!isSupported) return qsTranslate("PcView", "Update needed")
        return busy ? qsTranslate("PcView", "In use") : qsTranslate("PcView", "Ready")
    }
    // ready, active, attention, offline or checking: each desktop picks its own colour.
    function toneFor(isOnline, isPaired, unknown, isSupported, busy) {
        if (unknown) return "checking"
        if (!isOnline) return "offline"
        if (!isPaired || !isSupported) return "attention"
        return busy ? "active" : "ready"
    }
    readonly property string statusText: !hasHost ? ""
        : (resumeDesktop && online && paired && supported
           ? qsTranslate("PcView", "Ready to resume")
           : statusFor(online, paired, checking, supported, host.busy === true))
    readonly property string statusTone: toneFor(online, paired, checking, supported, host.busy === true)

    function osName(os) {
        switch (os) {
        case "macos": return qsTranslate("PcView", "macOS")
        case "windows": return qsTranslate("PcView", "Windows")
        case "linux": return qsTranslate("PcView", "Linux")
        default: return ""
        }
    }
    function osIconName(os) {
        switch (os) {
        case "macos": return "os_macos"
        case "windows": return "os_windows"
        case "linux": return "os_linux"
        default: return "monitor"
        }
    }
    readonly property string iconName: osIconName(host.hostOs || "")
    // In the words someone would use, rather than a timestamp.
    function connectedSince(seconds, nowMs) {
        if (!(seconds > 0)) return ""
        var then = new Date(seconds * 1000)
        var today = new Date(nowMs)
        var days = Math.round((new Date(today.getFullYear(), today.getMonth(), today.getDate())
                               - new Date(then.getFullYear(), then.getMonth(), then.getDate())) / 86400000)
        if (days <= 0) return qsTranslate("PcView", "Connected today")
        if (days === 1) return qsTranslate("PcView", "Connected yesterday")
        if (days < 30) return qsTranslate("PcView", "Connected %1 days ago").arg(days)
        return qsTranslate("PcView", "Connected %1").arg(then.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
    }
    // "Ready · macOS · Moonwalk Host (0.4.0-alpha.15) · Connected yesterday"; the protocol still says "Skylight"
    readonly property string statusLine: {
        var parts = [statusText]
        if (osName(host.hostOs)) parts.push(osName(host.hostOs))
        if (host.serverSoftware) parts.push(host.serverSoftware === "Skylight" ? "Moonwalk Host" : host.serverSoftware)
        parts.push(connectedSince(prefs.lastConnectedAt, Date.now()))
        // Wraps between facts, never in the middle of one.
        return parts.filter(function(part) { return part.length > 0 })
                    .map(function(part) { return part.replace(/ /g, "\u00A0").replace(/-/g, "-\u2060") }).join(" · ")
    }

    // Only Moonwalk Host hides its own pointer for a local one (clientSideCursor
    // at launch). The Sunshine family all keep drawing theirs unless toggled by
    // hand: the toggle is a process-wide flip with no readable state, so the
    // client can't safely press it for you.
    readonly property bool hostTogglesCursorByHand:
        ["Sunshine", "Apollo", "Vibeshine", "Vibepollo"].indexOf(host.serverSoftware || "") >= 0
    // What the host does with its own pointer while ours is drawn, if that needs saying.
    readonly property string hostCursorNote: hostTogglesCursorByHand
        ? qsTranslate("PcView", "If you see two pointers, press Ctrl+Alt+Shift+N to toggle the host cursor.")
        : host.serverSoftware && host.serverSoftware !== "Skylight"
          ? qsTranslate("PcView", "The host may draw a second cursor.") : ""
    readonly property string localCursorDescription: {
        var base = qsTranslate("PcView", "Draws the pointer here so it can move without waiting for video.")
        if (host.serverSoftware === "Skylight")
            return base + " " + qsTranslate("PcView", "Moonwalk Host can hide the remote cursor while this one is visible.")
        return hostCursorNote ? base + " " + hostCursorNote : base
    }

    // ---- The one thing to do next -----------------------------------------
    readonly property bool connectVisible: hasHost && online && paired && !checking
    readonly property bool connectEnabled: connectVisible && supported
    readonly property string connectLabel: resumeDesktop ? qsTranslate("PcView", "Resume Desktop")
                                           : switchToDesktop ? qsTranslate("PcView", "Switch to Desktop")
                                           : qsTranslate("PcView", "Connect")
    readonly property bool pairVisible: hasHost && online && !paired && !checking
    readonly property bool wakeVisible: hasHost && !online && !checking && host.wakeable === true
    readonly property string wakeLabel: page.waitingForWake === true
                                        ? qsTranslate("PcView", "Cancel Wake and Connect")
                                        : qsTranslate("PcView", "Wake and Connect")
    readonly property string runningNote: switchToDesktop
        ? qsTranslate("PcView", "%1 is running. Moonwalk will ask before closing it to open Desktop.")
          .arg(host.runningAppName || qsTranslate("PcView", "Another app")) : ""
    readonly property string resumeRunningLabel: switchToDesktop
        ? qsTranslate("PcView", "Resume %1").arg(host.runningAppName || qsTranslate("PcView", "App")) : ""
    readonly property bool waking: page.waitingForWake === true && hasHost && !online
    readonly property string wakingNote: waking
        ? qsTranslate("PcView", "Waiting for %1 to wake. Moonwalk will connect when the host is ready.").arg(host.name) : ""
    readonly property string unsupportedNote: hasHost && online && paired && !supported
        ? qsTranslate("PcView", "This version of Moonwalk cannot connect to the host. Check for a newer build.") : ""

    // A host with nothing to configure yet gets a full status page rather than
    // an empty settings form.
    readonly property bool settingsVisible: hasHost && paired && !checking
    readonly property string statusPageTitle: {
        if (!hasHost || settingsVisible) return ""
        if (checking) return host.name || ""
        if (!online) return qsTranslate("PcView", "%1 is offline").arg(host.name)
        return qsTranslate("PcView", "Pair with %1").arg(host.name)
    }
    readonly property string statusPageBody: {
        if (!hasHost || settingsVisible) return ""
        if (checking) return qsTranslate("PcView", "Checking whether this host is available…")
        if (!online) return host.wakeable === true
            ? qsTranslate("PcView", "Wake this host from here or check that it is on and connected to this network.")
            : qsTranslate("PcView", "Check that the host is on, connected to this network, and running its streaming software.")
        return qsTranslate("PcView", "Pair once by entering the PIN on the host.")
    }

    // ---- First launch and an empty list ----------------------------------
    readonly property bool discoveryOff: prefs.enableMdns === false
    readonly property bool stillSearching: !discoveryOff && page.initialDiscoveryElapsed !== true
    readonly property string emptyTitle: discoveryOff ? qsTranslate("PcView", "Automatic Host Discovery Is Off")
                                         : stillSearching ? qsTranslate("PcView", "Looking for Hosts")
                                         : qsTranslate("PcView", "No Hosts Found Yet")
    readonly property string emptyBody: discoveryOff
        ? qsTranslate("PcView", "Add a host by its address, or turn discovery back on in Preferences.")
        : stillSearching
          ? qsTranslate("PcView", "Looking for compatible hosts on your local network.")
          : qsTranslate("PcView", "Start the host software with both computers on the same network. If the host doesn't appear, add it by address.")

    // ---- What the next stream will be ------------------------------------
    // The stream plan chooses for Automatic and prices every Manual choice;
    // this page only puts its words beside the controls.
    readonly property var plan: page.streamPlan || ({})
    readonly property bool manual: prefs.manualStream === true
    readonly property string qualityMode: prefs.autoAdjustBitrate === false ? "manual"
        : prefs.adaptiveBitrate === true && host.adaptiveBitrateSupported === true ? "adaptive" : "recommended"
    readonly property string bitrateMode: qualityMode === "adaptive" ? qsTranslate("PcView", "Automatic")
        : qualityMode === "manual" ? qsTranslate("PcView", "Limited") : qsTranslate("PcView", "Recommended")
    function sizeText(width, height) { return qsTranslate("PcView", "%1×%2").arg(width).arg(height) }
    readonly property string bitrateText: qsTranslate("PcView", "%1 Mbps").arg(formatMbps(plan.startBitrateKbps > 0 ? plan.startBitrateKbps : prefs.bitrateKbps || 0))
    function formatMbps(kbps) {
        var mbps = kbps / 1000
        return mbps >= 100 || mbps === Math.round(mbps) ? String(Math.round(mbps)) : mbps.toFixed(1)
    }
    function sentence(text) { return text.length > 0 && ".!?…".indexOf(text[text.length - 1]) < 0 ? text + "." : text }
    // What Automatic chose, in plain words: "2880×1800 at 60 fps", or the
    // host's own limit when it can't keep up with this display.
    readonly property string automaticChoice: plan.width > 0
        ? qsTranslate("PcView", "%1 at %2 fps").arg(sizeText(plan.width, plan.height)).arg(plan.fps) : ""
    readonly property string streamModeDescription: manual
        ? qsTranslate("PcView", "Uses your saved stream settings. Check the individual controls for limits.")
        : plan.limitedByHost === true && plan.limitReason
          ? sentence(plan.limitReason)
          : automaticChoice.length > 0
            ? qsTranslate("PcView", "Plans %1 for this display.").arg(automaticChoice)
            : qsTranslate("PcView", "Chooses a starting picture and bitrate for this display and host.")
    readonly property var modeOptions: [
        { text: qsTranslate("PcView", "Automatic"), value: "automatic" },
        { text: qsTranslate("PcView", "Manual"), value: "manual" }
    ]
    // Display and Input are Automatic or Manual too. Automatic says in one line
    // what it does; Manual shows the choices.
    readonly property bool manualDisplay: prefs.manualDisplay === true
    readonly property bool manualInput: prefs.manualInput === true
    readonly property string ownChoices: qsTranslate("PcView", "Uses your saved settings.")
    readonly property bool ownDisplayForDesktop: host.virtualDisplaySupported === true && host.virtualDisplayReady === true
    readonly property string displayModeDescription: manualDisplay ? ownChoices
        : (host.desktopScaleSupported === true ? qsTranslate("PcView", "Fits the picture to the window and requests the closest supported text size.")
                                                : qsTranslate("PcView", "The whole picture fits the window. The host controls text size."))
          + (ownDisplayForDesktop ? " " + qsTranslate("PcView", "Requests a virtual display when the host allows it.") : "")
    readonly property string inputModeDescription: manualInput ? ownChoices
        : qsTranslate("PcView", "Uses desktop or game pointer movement and draws the cursor locally.")
          + (hostCursorNote ? " " + hostCursorNote : "")
    // What the next connection will be, at a glance.
    readonly property var streamFacts: plan.width > 0 ? [
        { objectName: "resolution", label: qsTranslate("PcView", "Resolution"),
          value: sizeText(plan.width, plan.height),
          note: manual && prefs.automaticResolution !== true ? qsTranslate("PcView", "Manual")
                : plan.limitedByHost === true ? qsTranslate("PcView", "Limited by host")
                : qsTranslate("PcView", "Matches this display") },
        { objectName: "frame-rate", label: qsTranslate("PcView", "Frame rate"),
          value: qsTranslate("PcView", "%1 FPS").arg(plan.fps),
          note: manual && prefs.automaticFrameRate !== true ? qsTranslate("PcView", "Manual")
                : plan.fps !== page.displayRefreshRate ? qsTranslate("PcView", "Adjusted for this stream")
                : qsTranslate("PcView", "Matches this display") },
        { objectName: "bitrate", label: qsTranslate("PcView", "Bitrate"),
          value: bitrateText,
          note: qualityMode === "manual" ? bitrateMode : qsTranslate("PcView", "Planned bitrate budget") }
    ] : []
    // "2880×1800 · 60 FPS · up to 53 Mbps", for screen readers and small spaces.
    readonly property string streamSummary: plan.width > 0
        ? [sizeText(plan.width, plan.height),
           qsTranslate("PcView", "%1 FPS").arg(plan.fps),
           qsTranslate("PcView", "%1 planned bitrate budget").arg(bitrateText)].join(" · ")
        : ""

    readonly property var codecOptions: [
        { text: qsTranslate("PcView", "Automatic"), value: 0 },
        { text: qsTranslate("PcView", "H.264"), value: 1 },
        { text: qsTranslate("PcView", "HEVC (H.265)"), value: 2 },
        { text: qsTranslate("PcView", "AV1"), value: 4 }
    ]
    readonly property var desktopScaleOptions: [
        { text: qsTranslate("PcView", "Automatic"), value: "auto" },
        { text: qsTranslate("PcView", "Small (1×)"), value: "1" },
        { text: qsTranslate("PcView", "Large (2×)"), value: "2" }
    ]
    // Never below what the planned picture needs, never above the decoder's reach.
    readonly property int bandwidthFloor: Math.max(500, plan.floorKbps || 0)
    readonly property var pointerModes: [
        qsTranslate("PcView", "Automatic"), qsTranslate("PcView", "Desktop"), qsTranslate("PcView", "Game")
    ]
    readonly property int pointerMode: prefs.automaticPointer === true ? 0 : prefs.absoluteMouseMode === true ? 1 : 2
    readonly property string pointerDescription: pointerMode === 0
        ? qsTranslate("PcView", "Follows your pointer on desktops, and uses relative movement in games.")
        : pointerMode === 1 ? qsTranslate("PcView", "Sends the pointer position on the picture.")
        : qsTranslate("PcView", "Sends the distance and direction the mouse moves.")

    function indexForValue(options, value) {
        for (var index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index
        }
        return 0
    }
    function indexForResolution() {
        var options = page.resolutionOptions || []
        if (prefs.automaticResolution) return 0
        for (var index = 0; index < options.length - 1; ++index) {
            if (options[index].automatic) continue
            if (prefs.width === options[index].w && prefs.height === options[index].h)
                return index
        }
        return Math.max(0, options.length - 1)
    }
    function indexForFrameRate() {
        var options = page.frameRateOptions || []
        if (prefs.automaticFrameRate) return 0
        for (var index = 0; index < options.length - 1; ++index) {
            if (prefs.fps === options[index].value)
                return index
        }
        return Math.max(0, options.length - 1)
    }
    function withWarning(description, warning) {
        return warning ? (description ? description + " " : "") + warning : description
    }


    // ---- Every per-host setting ------------------------------------------
    // Everything here applies the next time this host connects: Starlight is
    // only on screen between streams, so rows do not each repeat it.
    readonly property list<SettingGroup> groups: [
        SettingGroup {
            groupId: "stream"
            title: qsTranslate("PcView", "Stream")
            SettingRow {
                controlId: "starlight.hosts.stream-mode"
                kind: "choice"
                title: qsTranslate("PcView", "Picture and bandwidth")
                subtitle: root.streamModeDescription
                keywords: qsTranslate("PcView", "automatic manual customise resolution frame rate bitrate bandwidth codec")
                options: root.modeOptions
                textRole: "text"
                currentIndex: root.manual ? 1 : 0
                displayText: root.modeOptions[currentIndex].text
                onActivated: function(index) { root.page.setStreamMode(root.modeOptions[index].value) }
            }
            SettingRow {
                controlId: "starlight.hosts.resolution"
                kind: "combo"
                shown: root.manual
                title: qsTranslate("PcView", "Resolution")
                warning: root.plan.resolutionWarning || ""
                accessibleDescription: root.withWarning(displayText, warning)
                options: root.page.resolutionOptions || []
                textRole: "text"
                currentIndex: root.indexForResolution()
                displayText: currentIndex === 0 && root.prefs.automaticResolution === true
                             ? qsTranslate("PcView", "Automatic (%1)").arg(root.sizeText(root.plan.width, root.plan.height))
                             : currentIndex === options.length - 1
                               ? root.sizeText(root.prefs.width, root.prefs.height)
                               : (options[currentIndex] ? options[currentIndex].text : "")
                onActivated: function(index) {
                    var option = root.page.resolutionOptions[index]
                    if (option.w === 0) root.page.requestCustomResolution(root.page.selectedHostUuid)
                    else if (option.automatic) root.page.setAutomaticResolution()
                    else root.page.setResolution(option.w, option.h)
                }
            }
            SettingRow {
                controlId: "starlight.hosts.frame-rate"
                kind: "combo"
                shown: root.manual
                title: qsTranslate("PcView", "Frame rate")
                warning: root.plan.fpsWarning || ""
                accessibleDescription: root.withWarning(displayText, warning)
                options: root.page.frameRateOptions || []
                textRole: "text"
                currentIndex: root.indexForFrameRate()
                displayText: currentIndex === options.length - 1
                             ? qsTranslate("PcView", "%1 FPS").arg(root.prefs.fps)
                             : (options[currentIndex] ? options[currentIndex].text : "")
                onActivated: function(index) {
                    var option = root.page.frameRateOptions[index]
                    if (option.value < 0) root.page.requestCustomFrameRate(root.page.selectedHostUuid)
                    else if (option.automatic) root.page.setAutomaticFrameRate()
                    else root.page.setFrameRate(option.value)
                }
            }
            SettingRow {
                controlId: "starlight.hosts.video-codec"
                kind: "combo"
                shown: root.manual
                title: qsTranslate("PcView", "Video codec")
                warning: root.plan.codecWarning || ""
                accessibleDescription: root.withWarning(displayText, warning)
                options: root.codecOptions
                textRole: "text"
                currentIndex: root.indexForValue(root.codecOptions, root.prefs.videoCodecConfig)
                displayText: root.codecOptions[currentIndex].text
                onActivated: function(index) { root.page.setVideoCodec(root.codecOptions[index].value) }
            }
            SettingRow {
                controlId: "starlight.hosts.limit-bandwidth"
                shown: root.manual
                title: qsTranslate("PcView", "Limit bandwidth")
                subtitle: root.qualityMode === "manual"
                          ? qsTranslate("PcView", "Uses the bitrate target you set below.")
                          : qsTranslate("PcView", "Chooses a bitrate based on the stream and host.")
                keywords: qsTranslate("PcView", "bitrate data cap")
                checked: root.qualityMode === "manual"
                onToggled: function(wanted) { root.page.setBandwidthLimit(wanted) }
            }
            SettingRow {
                controlId: "starlight.hosts.maximum-bitrate"
                kind: "slider"
                shown: root.manual && root.qualityMode === "manual"
                title: qsTranslate("PcView", "Bandwidth limit")
                warning: root.plan.bitrateWarning || ""
                from: root.bandwidthFloor
                to: 150000
                stepSize: 500
                value: Math.max(root.bandwidthFloor, root.prefs.bitrateKbps || 0)
                valueText: qsTranslate("PcView", "%1 Mbps").arg((value / 1000).toFixed(1))
                accessibleDescription: root.withWarning(qsTranslate("PcView", "Target %1 Mbps").arg((value / 1000).toFixed(1)), warning)
                onMoved: function(wanted) { root.page.setBitrateKbps(wanted) }
            }
        },
        SettingGroup {
            groupId: "display"
            title: qsTranslate("PcView", "Display")
            SettingRow {
                controlId: "starlight.hosts.display-mode"
                kind: "choice"
                title: qsTranslate("PcView", "Size and fit")
                subtitle: root.displayModeDescription
                keywords: qsTranslate("PcView", "automatic manual remote text size desktop scale view fit actual host virtual display")
                options: root.modeOptions
                textRole: "text"
                currentIndex: root.manualDisplay ? 1 : 0
                displayText: root.modeOptions[currentIndex].text
                onActivated: function(index) { root.page.setDisplayMode(root.modeOptions[index].value) }
            }
            SettingRow {
                controlId: "starlight.hosts.desktop-scale"
                kind: "choice"
                shown: root.manualDisplay
                title: qsTranslate("PcView", "Remote text size")
                subtitle: root.host.desktopScaleSupported === true
                    ? qsTranslate("PcView", "Text and interface size on the host.")
                    : qsTranslate("PcView", "This host controls its own text size.")
                sensitive: root.host.desktopScaleSupported === true
                options: root.desktopScaleOptions
                textRole: "text"
                currentIndex: root.indexForValue(root.desktopScaleOptions, root.prefs.desktopScale || "auto")
                displayText: root.desktopScaleOptions[currentIndex].text
                onActivated: function(index) { root.page.setDesktopScale(root.desktopScaleOptions[index].value) }
            }
            SettingRow {
                controlId: "starlight.hosts.view-scale"
                kind: "choice"
                shown: root.manualDisplay
                title: qsTranslate("PcView", "View scale")
                subtitle: root.prefs.viewScale === "actual"
                          ? qsTranslate("PcView", "One remote pixel for each local pixel.")
                          : qsTranslate("PcView", "The whole remote picture fits the window.")
                options: [qsTranslate("PcView", "Fit in Window"), qsTranslate("PcView", "Actual Size")]
                currentIndex: root.prefs.viewScale === "actual" ? 1 : 0
                displayText: options[currentIndex]
                onActivated: function(index) { root.page.setViewScale(index === 1 ? "actual" : "fit") }
            }
            SettingRow {
                controlId: "starlight.hosts.display-target"
                kind: "combo"
                shown: root.manualDisplay
                       && (root.host.virtualDisplaySupported === true || (root.prefs.displayTarget || "host") !== "host")
                title: qsTranslate("PcView", "Host display")
                subtitle: root.host.virtualDisplaySupported !== true
                    ? qsTranslate("PcView", "This host has not offered a choice of displays.")
                    : root.host.virtualDisplayReady === true
                      ? qsTranslate("PcView", "Automatic asks the host for a virtual display for Desktop.")
                      : qsTranslate("PcView", "The virtual display driver is unavailable. Automatic follows the host setting.")
                options: [qsTranslate("PcView", "Automatic"), qsTranslate("PcView", "Use host setting"),
                          qsTranslate("PcView", "Dedicated virtual display"), qsTranslate("PcView", "Physical display")]
                currentIndex: Math.max(0, ["auto", "host", "virtual", "physical"].indexOf(root.prefs.displayTarget || "host"))
                displayText: options[currentIndex]
                onActivated: function(index) { root.page.setDisplayTarget(["auto", "host", "virtual", "physical"][index]) }
            }
        },
        SettingGroup {
            groupId: "input"
            title: qsTranslate("PcView", "Keyboard and Mouse")
            SettingRow {
                controlId: "starlight.hosts.input-mode"
                kind: "choice"
                title: qsTranslate("PcView", "Pointer and cursor")
                subtitle: root.inputModeDescription
                keywords: qsTranslate("PcView", "automatic manual mouse pointer cursor absolute relative local")
                options: root.modeOptions
                textRole: "text"
                currentIndex: root.manualInput ? 1 : 0
                displayText: root.modeOptions[currentIndex].text
                onActivated: function(index) { root.page.setInputMode(root.modeOptions[index].value) }
            }
            SettingRow {
                controlId: "starlight.hosts.pointer-mode"
                kind: "choice"
                shown: root.manualInput
                title: qsTranslate("PcView", "Pointer")
                subtitle: root.pointerDescription
                keywords: qsTranslate("PcView", "mouse absolute relative")
                options: root.pointerModes
                currentIndex: root.pointerMode
                displayText: options[currentIndex]
                onActivated: function(index) {
                    if (index === 0) root.page.setAutomaticPointer(true)
                    else root.page.setAbsoluteMouseMode(index === 1)
                }
            }
            SettingRow {
                controlId: "starlight.hosts.local-cursor"
                shown: root.manualInput
                title: qsTranslate("PcView", "Local cursor")
                subtitle: root.localCursorDescription
                checked: root.prefs.localCursor === true
                onToggled: function(wanted) { root.page.setLocalCursorEnabled(wanted) }
            }
            // Whether to type with this computer's layout depends on the person
            // and the host, not on anything Moonwalk can see, so it stays out of
            // Automatic and in view.
            SettingRow {
                controlId: "starlight.hosts.local-text-input"
                title: qsTranslate("PcView", "Type with this computer’s keyboard layout")
                subtitle: qsTranslate("PcView", "Sends accents and input method text. Turn off for games that need physical keys.")
                checked: root.prefs.localTextInput === true
                onToggled: function(wanted) { root.page.setLocalTextInput(wanted) }
            }
        },
        SettingGroup {
            groupId: "advanced"
            title: qsTranslate("PcView", "Advanced")
            SettingRow {
                controlId: "starlight.hosts.saved-settings"
                kind: "action"
                title: qsTranslate("PcView", "Saved Settings")
                subtitle: root.prefs.activePresetName && root.prefs.activePresetName !== qsTranslate("StreamingPreferences", "Custom")
                          ? root.prefs.activePresetName
                          : qsTranslate("PcView", "Save a set of choices to use again on this host.")
                onTriggered: root.page.requestPresets(root.page.selectedHostUuid)
            }
            SettingRow {
                controlId: "starlight.hosts.relearn-performance"
                kind: "action"
                title: qsTranslate("PcView", "Re-evaluate This Host")
                subtitle: qsTranslate("PcView", "Clear what Moonwalk learned about performance for this host.")
                onTriggered: root.page.resetLearnedPerformance()
            }
            SettingRow {
                controlId: "starlight.hosts.restore-last-working"
                kind: "action"
                title: qsTranslate("PcView", "Restore Last Working Settings")
                subtitle: qsTranslate("PcView", "Use the settings from the last healthy stream.")
                onTriggered: root.page.restoreSuccessfulSettings()
            }
        }
    ]
}
