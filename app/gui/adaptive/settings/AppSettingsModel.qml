import QtQuick 2.15
import "../KeyboardShortcuts.js" as KeyboardShortcuts

// Every application-wide preference, declared once for every desktop.
QtObject {
    id: root
    required property var page

    function indexForValue(options, value) {
        for (var index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index
        }
        return 0
    }

    // Search: every word has to appear in the row, its group or its other names.
    property string searchText: ""
    property bool searchHasResults: true
    onSearchTextChanged: applySearch()
    function applySearch() {
        var words = searchText.toLowerCase().split(/\s+/).filter(function(word) { return word.length > 0 })
        var anyResult = false
        for (var g = 0; g < groups.length; ++g) {
            var group = groups[g]
            var anyInGroup = false
            for (var r = 0; r < group.rows.length; ++r) {
                var row = group.rows[r]
                var haystack = (group.title + " " + row.title + " " + row.subtitle + " " + row.keywords).toLowerCase()
                var matches = words.every(function(word) { return haystack.indexOf(word) >= 0 })
                row.filteredOut = !matches
                anyInGroup = anyInGroup || (matches && row.shown)
            }
            group.filteredOut = !anyInGroup
            anyResult = anyResult || anyInGroup
        }
        searchHasResults = anyResult
    }

    readonly property list<SettingGroup> groups: [
        SettingGroup {
            groupId: "streaming"
            title: qsTranslate("SettingsView", "Streaming")
            SettingRow {
                controlId: "starlight.settings.window-mode"
                keywords: qsTranslate("SettingsView", "fullscreen windowed borderless")
                kind: "combo"
                shown: root.page.windowModeVisible
                sensitive: root.page.windowModeSensitive
                title: qsTranslate("SettingsView", "Streaming display mode")
                subtitle: qsTranslate("SettingsView", "Fullscreen is usually fastest. Borderless windowed works better with Alt+Tab, screenshots and overlays.")
                options: root.page.windowModeOptions
                textRole: "text"
                currentIndex: root.indexForValue(options, root.page.windowModeValue)
                onActivated: function(index) { root.page.setWindowMode(options[index].value) }
            }
            SettingRow {
                controlId: "starlight.settings.keep-awake"
                title: qsTranslate("SettingsView", "Keep the display awake while streaming")
                checked: root.page.keepAwakeValue
                sensitive: root.page.keepAwakeSensitive
                onToggled: function(wanted) { root.page.setKeepAwakeEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.quit-app-after"
                title: qsTranslate("SettingsView", "End games when you disconnect")
                subtitle: qsTranslate("SettingsView", "Also ends the remote game or app; unsaved work may be lost. Desktop cleanup runs in the background.")
                checked: root.page.quitAppAfterValue
                sensitive: root.page.quitAppAfterSensitive
                onToggled: function(wanted) { root.page.setQuitAppAfterEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.performance-overlay"
                title: qsTranslate("SettingsView", "Show performance stats while streaming")
                subtitle: qsTranslate("SettingsView", "Toggle it at any time with Ctrl+Alt+Shift+S or Select+L1+R1+X.")
                checked: root.page.performanceOverlayValue
                sensitive: root.page.performanceOverlaySensitive
                onToggled: function(wanted) { root.page.setPerformanceOverlayEnabled(wanted) }
            }
        },
        SettingGroup {
            groupId: "audio"
            title: qsTranslate("SettingsView", "Audio")
            SettingRow {
                controlId: "starlight.settings.mute-host-speakers"
                keywords: qsTranslate("SettingsView", "sound volume")
                title: qsTranslate("SettingsView", "Mute the host's speakers while streaming")
                subtitle: qsTranslate("SettingsView", "Takes effect the next time an app starts on the host.")
                accessibleDescription: qsTranslate("SettingsView", "Mutes the host speakers while streaming")
                checked: root.page.muteHostSpeakersValue
                sensitive: root.page.muteHostSpeakersSensitive
                onToggled: function(wanted) { root.page.setMuteHostSpeakersEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.mute-on-focus-loss"
                shown: root.page.muteOnFocusLossVisible
                sensitive: root.page.muteOnFocusLossSensitive
                title: qsTranslate("SettingsView", "Mute audio when Moonwalk is not the active window")
                checked: root.page.muteOnFocusLossValue
                onToggled: function(wanted) { root.page.setMuteOnFocusLossEnabled(wanted) }
            }
        },
        SettingGroup {
            groupId: "keyboard"
            title: qsTranslate("SettingsView", "Keyboard")
            SettingRow {
                controlId: "starlight.settings.capture-system-keys"
                title: qsTranslate("SettingsView", "Capture system keyboard shortcuts")
                subtitle: qsTranslate("SettingsView", "Sends shortcuts like Alt+Tab to the host instead of this computer.") 
                accessibleDescription: qsTranslate("SettingsView", "Enables capture of system keyboard shortcuts while streaming") + " " + KeyboardShortcuts.releaseHint()
                checked: root.page.captureSystemKeysValue
                sensitive: root.page.captureSystemKeysSensitive
                onToggled: function(wanted) { root.page.setCaptureSystemKeysEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.capture-system-keys-mode"
                kind: "combo"
                title: qsTranslate("SettingsView", "Capture mode")
                accessibleName: qsTranslate("SettingsView", "System keyboard shortcut capture mode")
                accessibleDescription: qsTranslate("SettingsView", "Chooses when Moonwalk captures system keyboard shortcuts")
                sensitive: root.page.captureSystemKeysModeSensitive && root.page.captureSystemKeysValue
                options: root.page.captureSystemKeysModeOptions
                textRole: "text"
                currentIndex: root.indexForValue(options, root.page.captureSystemKeysModeValue)
                onActivated: function(index) { root.page.setCaptureSystemKeysMode(options[index].value) }
            }
        },
        SettingGroup {
            groupId: "pointer"
            title: qsTranslate("SettingsView", "Mouse and Touch")
            SettingRow {
                controlId: "starlight.settings.virtual-trackpad"
                title: qsTranslate("SettingsView", "Use touchscreen as a virtual trackpad")
                subtitle: qsTranslate("SettingsView", "When off, touching the screen moves the pointer directly.")
                checked: root.page.virtualTrackpadValue
                sensitive: root.page.virtualTrackpadSensitive
                onToggled: function(wanted) { root.page.setVirtualTrackpadEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.swap-mouse-buttons"
                title: qsTranslate("SettingsView", "Swap left and right mouse buttons")
                checked: root.page.swapMouseButtonsValue
                sensitive: root.page.swapMouseButtonsSensitive
                onToggled: function(wanted) { root.page.setSwapMouseButtonsEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.reverse-scroll"
                title: qsTranslate("SettingsView", "Reverse mouse scrolling direction")
                checked: root.page.reverseScrollValue
                sensitive: root.page.reverseScrollSensitive
                onToggled: function(wanted) { root.page.setReverseScrollEnabled(wanted) }
            }
        },
        SettingGroup {
            groupId: "gamepad"
            title: qsTranslate("SettingsView", "Gamepad")
            SettingRow {
                controlId: "starlight.settings.swap-face-buttons"
                title: qsTranslate("SettingsView", "Swap A/B and X/Y gamepad buttons")
                subtitle: qsTranslate("SettingsView", "A Nintendo-style button layout.")
                checked: root.page.swapFaceButtonsValue
                sensitive: root.page.swapFaceButtonsSensitive
                onToggled: function(wanted) { root.page.setSwapFaceButtonsEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.single-controller"
                title: qsTranslate("SettingsView", "Force gamepad #1 always connected")
                subtitle: qsTranslate("SettingsView", "The host always sees one gamepad, even when none is connected here.")
                checked: root.page.singleControllerValue
                sensitive: root.page.singleControllerSensitive
                onToggled: function(wanted) { root.page.setSingleControllerEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.gamepad-mouse"
                title: qsTranslate("SettingsView", "Enable mouse control by holding Start")
                checked: root.page.gamepadMouseValue
                sensitive: root.page.gamepadMouseSensitive
                onToggled: function(wanted) { root.page.setGamepadMouseEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.background-gamepad"
                shown: root.page.backgroundGamepadVisible
                sensitive: root.page.backgroundGamepadSensitive
                title: qsTranslate("SettingsView", "Process gamepad input when Moonwalk is in the background")
                checked: root.page.backgroundGamepadValue
                onToggled: function(wanted) { root.page.setBackgroundGamepadEnabled(wanted) }
            }
        },
        SettingGroup {
            groupId: "network"
            title: qsTranslate("SettingsView", "Network")
            SettingRow {
                controlId: "starlight.settings.mdns"
                keywords: qsTranslate("SettingsView", "discovery mdns")
                title: qsTranslate("SettingsView", "Find hosts on the local network automatically")
                checked: root.page.mdnsValue
                sensitive: root.page.mdnsSensitive
                onToggled: function(wanted) { root.page.setMdnsEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.detect-network-blocking"
                title: qsTranslate("SettingsView", "Detect blocked connections automatically")
                checked: root.page.detectNetworkBlockingValue
                sensitive: root.page.detectNetworkBlockingSensitive
                onToggled: function(wanted) { root.page.setDetectNetworkBlockingEnabled(wanted) }
            }
        },
        SettingGroup {
            groupId: "general"
            title: qsTranslate("SettingsView", "General")
            SettingRow {
                controlId: "starlight.settings.ui-display-mode"
                kind: "combo"
                shown: root.page.uiDisplayModeVisible
                sensitive: root.page.uiDisplayModeSensitive
                title: qsTranslate("SettingsView", "Launcher window mode")
                options: root.page.uiDisplayModeOptions
                textRole: "text"
                currentIndex: root.indexForValue(options, root.page.uiDisplayModeValue)
                onActivated: function(index) { root.page.setUiDisplayMode(options[index].value) }
            }
            SettingRow {
                controlId: "starlight.settings.language"
                kind: "combo"
                title: qsTranslate("SettingsView", "Language")
                accessibleDescription: qsTranslate("SettingsView", "Changes Moonwalk's display language")
                sensitive: root.page.languageSensitive
                options: root.page.languageOptions
                textRole: "text"
                currentIndex: root.indexForValue(options, root.page.languageValue)
                onActivated: function(index) { root.page.setLanguage(options[index].value) }
            }
            SettingRow {
                controlId: "starlight.settings.connection-warnings"
                title: qsTranslate("SettingsView", "Show connection quality warnings")
                checked: root.page.connectionWarningsValue
                sensitive: root.page.connectionWarningsSensitive
                onToggled: function(wanted) { root.page.setConnectionWarningsEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.configuration-warnings"
                title: qsTranslate("SettingsView", "Show configuration warnings")
                checked: root.page.configurationWarningsValue
                sensitive: root.page.configurationWarningsSensitive
                onToggled: function(wanted) { root.page.setConfigurationWarningsEnabled(wanted) }
            }
            SettingRow {
                controlId: "starlight.settings.rich-presence"
                shown: root.page.richPresenceVisible
                sensitive: root.page.richPresenceSensitive
                title: qsTranslate("SettingsView", "Discord Rich Presence integration")
                subtitle: qsTranslate("SettingsView", "Shows what you are streaming in your Discord status.")
                checked: root.page.richPresenceValue
                onToggled: function(wanted) { root.page.setRichPresenceEnabled(wanted) }
            }
        }
    ]
}
