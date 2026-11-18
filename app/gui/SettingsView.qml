import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2


import Starlight 1.0
import "starlight"
import "starlight/ControlCompat.js" as ControlCompat

// Starlight's HIG restyle of the settings escape hatch (reached via the
// sidebar gear in PcView.qml). Same grouped-form language as PcView's
// per-host page -- SLGroup/SLRow/SLToggle/SLDropdown/SLSlider on Theme
// tokens.
//
// Per docs/design/2026-07-14-per-host-settings.md: this view holds ONLY
// GLOBAL app-behavior settings now. Every connection setting (resolution,
// frame rate, video bitrate, VSync, frame pacing, HDR, video decoder/codec,
// YUV 4:4:4, unlock-bitrate, audio configuration, optimize-mouse-for-remote-
// desktop) moved to PcView.qml's per-host page -- StreamingPreferences is
// host-scoped via setCurrentHost() there, so those settings are genuinely
// per-host and are deliberately NOT duplicated here. Display mode
// (windowMode) stays here: it's local client window behavior, not a
// connection parameter.
Item {
    id: settingsPage

    // See HostPageRoute: route construction injects the facade while the
    // legacy direct fixture is migrated in this extraction slice.
    required property var page
    // Stable object names are the public automation convention for page-level
    // landmarks and the handful of controls used by the manual AT-SPI gate.
    objectName: "starlight.settings.page"
    property string shellRouteId: "legacy-settings"
    readonly property bool shellRouteActive:
        StackView.status === StackView.Active

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Settings")
    Accessible.description: qsTr("Global Moonwalk settings")

    AccessibleIdentity {
        target: settingsPage
        stableId: settingsPage.objectName
    }

    signal languageChanged()

    // ── Helpers shared by more than one row ──

    // Generic "find the option whose value matches" used by every dropdown
    // below.
    function indexForValue(list, value) {
        for (var i = 0; i < list.length; i++) {
            if (list[i].value === value) {
                return i
            }
        }
        return -1
    }

    // NOTE: resolution/frame rate/bitrate/VSync/frame pacing/HDR/YUV444/
    // decoder/codec/unlock-bitrate/audio-config/optimize-mouse-for-RDP are
    // PER-HOST connection settings now (see docs/design/2026-07-14-per-host-
    // settings.md §1) and live on PcView.qml's host page instead -- along
    // with their option-list builders, setResolution()/setFps(), and the
    // custom resolution/frame-rate dialogs, which moved there too. This
    // view only keeps the option lists and helpers backing the GLOBAL
    // settings still below.

    // ── Display mode: base list + "(Recommended)" tag/reorder, exactly as
    // the pre-restyle AutoResizingComboBox model did. ──

    readonly property var windowModeOptions: page.windowModeOptions

    // ── Static option lists for the remaining dropdowns ──

    readonly property var uiDisplayModeOptions: page.uiDisplayModeOptions

    readonly property var captureSysKeysOptions: page.captureSystemKeysModeOptions

    // Remembers the last non-off capture mode chosen, so re-enabling the
    // toggle restores it instead of always resetting to "in fullscreen"
    // (mirrors the pre-restyle combo box, which kept its own currentIndex
    // independent of the live pref while the row was disabled).
    //
    // NOTE: this must be a plain property set only imperatively (initial
    // load + dropdown selection + the toggle's own off-snapshot below), NOT
    // a live binding on StreamingPreferences.captureSysKeysMode -- a live
    // binding re-evaluates every time that pref changes, including when the
    // toggle itself writes CSK_OFF, which would immediately snap this back
    // to CSK_FULLSCREEN and silently discard a chosen "always".
    property int lastCaptureSysKeysMode: 1

    Component.onCompleted: {
        if (page.captureSystemKeysModeValue !== 0) {
            lastCaptureSysKeysMode = page.captureSystemKeysModeValue
        }
    }

    readonly property var languageOptions: page.languageOptions

    // ── Autoscroll-to-focused-control + StackView lifecycle, unchanged in
    // behavior from the pre-restyle Flickable root, just retargeted at the
    // nested contentFlick (the ground Rectangle below needs to sit outside
    // the scrolling content, so the Flickable can no longer be the root
    // item the way it was before). ──

    function isChildOfFlickable(item) {
        while (item) {
            if (item.parent === contentFlick.contentItem) {
                return true
            }
            item = item.parent
        }
        return false
    }

    NumberAnimation {
        id: autoScrollAnimation
        target: contentFlick
        property: "contentY"
        duration: 100
    }

    Window.onActiveFocusItemChanged: {
        var item = Window.activeFocusItem
        if (item) {
            // Ignore non-child elements like the toolbar buttons
            if (!isChildOfFlickable(item)) {
                return
            }

            // Map the focus item's position into the flickable's content coordinate space
            var pos = item.mapToItem(contentFlick.contentItem, 0, 0)

            // Ensure some extra space is visible around the element we're scrolling to
            var scrollMargin = contentFlick.height > 100 ? 50 : 0

            if (pos.y - scrollMargin < contentFlick.contentY) {
                autoScrollAnimation.from = contentFlick.contentY
                autoScrollAnimation.to = Math.max(pos.y - scrollMargin, 0)
                autoScrollAnimation.start()
            }
            else if (pos.y + item.height + scrollMargin > contentFlick.contentY + contentFlick.height) {
                autoScrollAnimation.from = contentFlick.contentY
                autoScrollAnimation.to = Math.min(pos.y + item.height + scrollMargin - contentFlick.height, contentFlick.contentHeight - contentFlick.height)
                autoScrollAnimation.start()
            }
        }
    }

    function routeActivated(focusFirstControl) {
        if (focusFirstControl === true) {
            if (displayModeRow.visible)
                windowModeDropdown.forceActiveFocus(Qt.TabFocus)
            else
                languageDropdown.forceActiveFocus(Qt.TabFocus)
        }
    }

    // Full-bleed page ground: the app's ApplicationWindow keeps moonlight's
    // Material-dark background, so without this the Theme's light/dark
    // tokens float on whatever that theme paints (same fix as PcView.qml).
    Rectangle {
        anchors.fill: parent
        color: Theme.isDark ? "#202024" : "#F6F6F8"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── HIG header: replaces the legacy blue ToolBar (removed from
        // main.qml entirely now -- it never showed on this page in the
        // first place). Left-aligned back control + title, per PcView's
        // chrome patterns. ──
        Item {
            id: settingsHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 52

            RowLayout {
                anchors.fill: parent
                // Matches the content area's Theme.pageInsetX below (was a
                // bare 14px literal here vs. the content Column's 26px --
                // the back-button/title row didn't line up with the group
                // edges beneath it, audit §2.3/§4).
                anchors.leftMargin: Theme.pageInsetX
                anchors.rightMargin: Theme.pageInsetX
                spacing: 14

                // macOS-toolbar-style back control: chevron glyph + label,
                // hover pill background, comfortable hit area -- replaces
                // the old plain-text "‹ Hosts" SLButton per item 9.
                AbstractButton {
                    id: settingsBackButton
                    objectName: "starlight.settings.back"
                    Layout.alignment: Qt.AlignVCenter
                    hoverEnabled: true
                    focusPolicy: Qt.TabFocus

                    leftPadding: 8
                    rightPadding: 12
                    topPadding: 5
                    bottomPadding: 5

                    implicitWidth: backButtonRow.implicitWidth + leftPadding + rightPadding
                    implicitHeight: Math.max(28, backButtonRow.implicitHeight + topPadding + bottomPadding)

                    // Same back path Escape already uses (Keys.onEscapePressed
                    // in main.qml's StackView), so this control and the
                    // keyboard shortcut stay in sync.
                    onClicked: page.requestBack(false)

                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Back to hosts")
                    Accessible.description: qsTr("Returns to the hosts page")
                    Accessible.focusable: settingsBackButton.enabled && settingsBackButton.visible
                    Accessible.focused: settingsBackButton.activeFocus
                    Accessible.pressed: settingsBackButton.pressed
                    Accessible.onPressAction: {
                        if (settingsBackButton.visible) {
                            ControlCompat.activate(settingsBackButton)
                        }
                    }

                    AccessibleIdentity {
                        target: settingsBackButton
                        stableId: settingsBackButton.objectName
                    }

                    background: Rectangle {
                        radius: 8
                        color: (settingsBackButton.hovered || settingsBackButton.visualFocus) ? Theme.segBg : "transparent"
                    }

                    contentItem: RowLayout {
                        id: backButtonRow
                        spacing: 3

                        SLIcon {
                            name: "chevron-left"
                            size: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: qsTr("Hosts")
                            color: Theme.label
                            font.pixelSize: Theme.sizeBody
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                Text {
                    text: qsTr("Settings")
                    color: Theme.label
                    font.pixelSize: Theme.sizeHeadline
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.hairline
            }
        }

        Flickable {
            id: contentFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight + 44
            boundsBehavior: Flickable.OvershootBounds
            clip: true

            ScrollBar.vertical: SLScrollBar {
                objectName: "starlight.settings.scrollbar"
                accessibleName: qsTr("Settings scroll bar")
                accessibleDescription: qsTr("Scrolls through global Moonwalk settings")
            }

            Column {
                id: contentColumn
                x: Theme.pageInsetX
                y: Theme.pageInsetTop
                width: parent.width - 2 * Theme.pageInsetX
                spacing: 0

            // ── Essentials ──
            //
            // Per docs/design (Essentials + Advanced reveal, adaptive settings
            // UI, 2026-07-18): the default view shows only genuinely-chosen or
            // host/client-relevant settings. Everything else lives one click
            // away in the "Advanced" SLDisclosure below -- fully reachable,
            // never deleted (except gameOptimizations, an apps/gaming-only
            // concept the owner asked to remove outright).

            // ── Basic (essential: streaming display mode) ──

            // Header/spacer/group hide TOGETHER when the section's only row is
            // hidden (no desktop environment) — otherwise "Basic" renders as an
            // orphaned header floating above the next section.
            SLSectionHeader { text: qsTr("Basic"); visible: displayModeRow.visible }
            Item { width: 1; height: 7; visible: displayModeRow.visible }
            SLGroup {
                width: parent.width
                visible: displayModeRow.visible

                // Resolution/frame rate/video bitrate/VSync/frame pacing/HDR
                // are PER-HOST connection settings now -- moved to PcView.qml's
                // host page (see docs/design/2026-07-14-per-host-settings.md
                // §1). Display mode (the local client window mode) is GLOBAL
                // and stays here.
                SLRow {
                    id: displayModeRow
                    topBorder: false
                    visible: page.windowModeVisible
                    label: qsTr("Streaming display mode")
                    sublabel: qsTr("Fullscreen generally provides the best performance, but borderless windowed may work better with features like macOS Spaces, Alt+Tab, screenshot tools, and on-screen overlays.")

                    SLDropdown {
                        id: windowModeDropdown
                        objectName: "starlight.settings.window-mode"
                        accessibleName: displayModeRow.label
                        accessibleDescription: displayModeRow.sublabel
                        model: page.windowModeOptions
                        popupWidth: 220
                        enabled: page.windowModeSensitive
                        currentIndex: settingsPage.indexForValue(model, page.windowModeValue)
                        onActivated: page.setWindowMode(model[index].value)
                    }
                }
            }

            Item { width: 1; height: Theme.spacingL }

            // ── Audio (essential: mute host speakers) ──

            SLSectionHeader { text: qsTr("Audio") }
            Item { width: 1; height: 7 }
            SLGroup {
                width: parent.width

                // Audio configuration (channel layout) is a PER-HOST setting
                // now -- moved to PcView.qml's host page.
                SLRow {
                    topBorder: false
                    label: qsTr("Mute the host's speakers while streaming")
                    sublabel: qsTr("You must restart any app currently in progress for this setting to take effect.")

                    SLToggle {
                        objectName: "starlight.settings.mute-host-speakers"
                        accessibleName: qsTr("Mute the host's speakers while streaming")
                        accessibleDescription: qsTr("Mutes the host speakers while streaming")
                        checked: page.muteHostSpeakersValue
                        onSwitched: (wanted) => page.setMuteHostSpeakersEnabled(wanted)
                    }
                }
            }

            Item { width: 1; height: Theme.spacingL }

            // ── UI (essential: Language -- must never be buried behind an
            // English-language "Advanced" label) ──

            SLSectionHeader { text: qsTr("UI") }
            Item { width: 1; height: 7 }
            SLGroup {
                width: parent.width

                SLRow {
                    topBorder: false
                    label: qsTr("Language")

                    SLDropdown {
                        id: languageDropdown
                        objectName: "starlight.settings.language"
                        accessibleName: qsTr("Language")
                        accessibleDescription: qsTr("Changes Moonwalk's display language")
                        model: page.languageOptions
                        popupWidth: 220
                        currentIndex: settingsPage.indexForValue(model, page.languageValue)
                        onActivated: {
                            var newLanguage = model[index].value
                            page.setLanguage(newLanguage)
                            // Route-owned language effects update navigation and
                            // restart messaging; the painted dropdown has no
                            // backend decision object.
                            settingsPage.languageChanged()
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.spacingL }

            // Keyboard shortcuts are a routine remote-desktop choice.
            SLSectionHeader { text: qsTr("Keyboard") }
            Item { width: 1; height: 7 }
            SLGroup {
                width: parent.width

                SLRow {
                    topBorder: false
                    label: qsTr("Capture system keyboard shortcuts")
                    sublabel: qsTr("This enables the capture of system-wide keyboard shortcuts like Alt+Tab that would normally be handled by the client OS while streaming.") + " " +
                              qsTr("Certain keyboard shortcuts like Ctrl+Alt+Del on Windows cannot be intercepted by any application, including Moonwalk.")
                    accessibleControl: captureSysKeysToggle

                    RowLayout {
                        spacing: 10

                        SLDropdown {
                            objectName: "starlight.settings.capture-system-keys-mode"
                            accessibleName: qsTr("System keyboard shortcut capture mode")
                            accessibleDescription: qsTr("Chooses when Moonwalk captures system keyboard shortcuts")
                            model: page.captureSystemKeysModeOptions
                            enabled: captureSysKeysToggle.checked && captureSysKeysToggle.enabled
                            currentIndex: Math.max(0, settingsPage.indexForValue(model, page.captureSystemKeysModeValue))
                            onActivated: {
                                settingsPage.lastCaptureSysKeysMode = model[index].value
                                page.setCaptureSystemKeysMode(model[index].value)
                            }
                        }

                        SLToggle {
                            id: captureSysKeysToggle
                            objectName: "starlight.settings.capture-system-keys"
                            accessibleName: qsTr("Capture system keyboard shortcuts")
                            accessibleDescription: qsTr("Enables capture of system keyboard shortcuts while streaming")
                            enabled: page.captureSystemKeysSensitive
                            checked: page.captureSystemKeysValue
                            onSwitched: (wanted) => {
                                if (!page.captureSystemKeysSensitive)
                                    return
                                if (!wanted) {
                                    if (page.captureSystemKeysModeValue !== 0)
                                        settingsPage.lastCaptureSysKeysMode = page.captureSystemKeysModeValue
                                    page.setCaptureSystemKeysEnabled(false)
                                } else {
                                    page.setCaptureSystemKeysEnabled(true)
                                }
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.spacingL }

            // ── Advanced (collapsed by default; everything else, still fully
            // reachable, sub-grouped internally so the page doesn't dump 18
            // controls into one undifferentiated wall once opened) ──

            SLDisclosure {
                objectName: "starlight.settings.advanced"
                width: parent.width
                title: qsTr("Advanced")

                // Behaviour
                SLSectionHeader { text: qsTr("Behaviour") }
                Item { width: 1; height: 7 }
                SLGroup {
                    width: parent.width

                    SLRow {
                        topBorder: false
                    visible: page.muteOnFocusLossVisible
                        label: qsTr("Mute audio stream when Moonwalk is not the active window")
                        sublabel: qsTr("Mutes Moonwalk's audio when you Alt+Tab out of the stream or click on a different window.")

                        SLToggle {
                            checked: page.muteOnFocusLossValue
                            onSwitched: (wanted) => page.setMuteOnFocusLossEnabled(wanted)
                        }
                    }

                    // Reworded away from "app" (owner: not a gaming/app tool) --
                    // binding is unchanged, still StreamingPreferences.quitAppAfter.
                    SLRow {
                        label: qsTr("End games when you disconnect")
                        sublabel: qsTr("Closes what's running on the host when you end your session. Any unsaved work will be lost.")

                        SLToggle {
                            checked: page.quitAppAfterValue
                            onSwitched: (wanted) => page.setQuitAppAfterEnabled(wanted)
                        }
                    }

                    SLRow {
                    visible: page.uiDisplayModeVisible
                        label: qsTr("Launcher window mode")

                        SLDropdown {
                            model: page.uiDisplayModeOptions
                            currentIndex: settingsPage.indexForValue(model, page.uiDisplayModeValue)
                            onActivated: page.setUiDisplayMode(model[index].value)
                        }
                    }

                    SLRow {
                        label: qsTr("Show connection quality warnings")

                        SLToggle {
                            checked: page.connectionWarningsValue
                            onSwitched: (wanted) => page.setConnectionWarningsEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Show configuration warnings")

                        SLToggle {
                            checked: page.configurationWarningsValue
                            onSwitched: (wanted) => page.setConfigurationWarningsEnabled(wanted)
                        }
                    }

                    SLRow {
                    visible: page.richPresenceVisible
                        label: qsTr("Discord Rich Presence integration")
                        sublabel: qsTr("Updates your Discord status to display the name of the app you're streaming.")

                        SLToggle {
                            checked: page.richPresenceValue
                            onSwitched: (wanted) => page.setRichPresenceEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Keep the display awake while streaming")
                        sublabel: qsTr("Prevents the screensaver from starting or the display from going to sleep while streaming.")

                        SLToggle {
                            checked: page.keepAwakeValue
                            onSwitched: (wanted) => page.setKeepAwakeEnabled(wanted)
                        }
                    }
                }

                Item { width: 1; height: Theme.spacingL }

                // Mouse and touch options retain the existing Advanced group.
                SLSectionHeader { text: qsTr("Input & accessibility") }
                Item { width: 1; height: 7 }
                SLGroup {
                    width: parent.width

                    // "Optimize mouse for remote desktop" (absoluteMouseMode) is a
                    // PER-HOST setting now -- moved to PcView.qml's host page.
                    SLRow {
                        topBorder: false
                        label: qsTr("Use touchscreen as a virtual trackpad")
                        sublabel: qsTr("When checked, the touchscreen acts like a trackpad. When unchecked, the touchscreen will directly control the mouse pointer.")

                        SLToggle {
                            checked: page.virtualTrackpadValue
                            onSwitched: (wanted) => page.setVirtualTrackpadEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Swap left and right mouse buttons")

                        SLToggle {
                            checked: page.swapMouseButtonsValue
                            onSwitched: (wanted) => page.setSwapMouseButtonsEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Reverse mouse scrolling direction")

                        SLToggle {
                            checked: page.reverseScrollValue
                            onSwitched: (wanted) => page.setReverseScrollEnabled(wanted)
                        }
                    }
                }

                Item { width: 1; height: Theme.spacingL }

                // Gamepad
                SLSectionHeader { text: qsTr("Gamepad") }
                Item { width: 1; height: 7 }
                SLGroup {
                    width: parent.width

                    SLRow {
                        topBorder: false
                        label: qsTr("Swap A/B and X/Y gamepad buttons")
                        sublabel: qsTr("This switches gamepads into a Nintendo-style button layout.")

                        SLToggle {
                            checked: page.swapFaceButtonsValue
                            onSwitched: (wanted) => page.setSwapFaceButtonsEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Force gamepad #1 always connected")
                        sublabel: qsTr("Forces a single gamepad to always stay connected to the host, even if no gamepads are actually connected to this computer.") + " " +
                                  qsTr("Only enable this option when streaming an app that doesn't support gamepads being connected after startup.")

                        SLToggle {
                            checked: page.singleControllerValue
                            onSwitched: (wanted) => page.setSingleControllerEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Enable mouse control with gamepads by holding the 'Start' button")

                        SLToggle {
                            checked: page.gamepadMouseValue
                            onSwitched: (wanted) => page.setGamepadMouseEnabled(wanted)
                        }
                    }

                    SLRow {
                        visible: page.backgroundGamepadVisible
                        label: qsTr("Process gamepad input when Moonwalk is in the background")
                        sublabel: qsTr("Allows Moonwalk to capture gamepad inputs even if it's not the current window in focus.")

                        SLToggle {
                            checked: page.backgroundGamepadValue
                            onSwitched: (wanted) => page.setBackgroundGamepadEnabled(wanted)
                        }
                    }
                }

                Item { width: 1; height: Theme.spacingL }

                // Network & discovery
                SLSectionHeader { text: qsTr("Network & discovery") }
                Item { width: 1; height: 7 }
                SLGroup {
                    width: parent.width

                    SLRow {
                        topBorder: false
                        label: qsTr("Automatically find hosts on the local network (Recommended)")

                        SLToggle {
                            checked: page.mdnsValue
                            onSwitched: (wanted) => page.setMdnsEnabled(wanted)
                        }
                    }

                    SLRow {
                        label: qsTr("Automatically detect blocked connections (Recommended)")

                        SLToggle {
                            checked: page.detectNetworkBlockingValue
                            onSwitched: (wanted) => page.setDetectNetworkBlockingEnabled(wanted)
                        }
                    }
                }

                Item { width: 1; height: Theme.spacingL }

                // Diagnostics
                SLSectionHeader { text: qsTr("Diagnostics") }
                Item { width: 1; height: 7 }
                SLGroup {
                    width: parent.width

                    SLRow {
                        topBorder: false
                        label: qsTr("Show performance stats while streaming")
                        sublabel: qsTr("Display real-time stream performance information while streaming.") + " " +
                                  qsTr("You can toggle it at any time while streaming using Ctrl+Alt+Shift+S or Select+L1+R1+X.") + " " +
                                  qsTr("The performance overlay is not supported on Steam Link or Raspberry Pi.")

                        SLToggle {
                            checked: page.performanceOverlayValue
                            onSwitched: (wanted) => page.setPerformanceOverlayEnabled(wanted)
                        }
                    }
                }
            }

            Item { width: 1; height: 24 }

            // ── About footer (replaces the legacy toolbar's versionLabel,
            // which only ever showed on this page and is gone now that the
            // toolbar itself is hidden here -- see main.qml). ──
            Text {
                width: parent.width
                text: qsTr("Moonwalk %1 · moonlight-qt engine %2")
                      .arg(Qt.application.starlightVersion || Qt.application.version)
                      .arg(Qt.application.version)
                color: Theme.label3
                font.pixelSize: Theme.sizeFootnote
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Item { width: 1; height: 8 }
        }
    }
    }
}
