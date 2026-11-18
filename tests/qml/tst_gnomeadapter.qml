import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtTest 1.3
import Starlight 1.0

import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "GnomeAdapter"
    when: windowShown

    property var testWindow: null

    function initTestCase() {
        testWindow = Qt.createQmlObject(
                    'import QtQuick.Window 2.15; Window {'
                    + ' width: 640; height: 480; visible: true }',
                    testCase)
        verify(testWindow !== null)
        testWindow.requestActivate()
        tryCompare(testWindow, "active", true)
        waitForRendering(testWindow.contentItem)
    }

    function init() {
        Appearance.reset()
        testWindow.contentItem.LayoutMirroring.enabled = false
        testWindow.contentItem.LayoutMirroring.childrenInherit = true
    }

    // Key synthesis needs the TestCase's own window to hold the window
    // manager's focus; under a real WM the sibling harness window can have
    // taken it.
    function activateOwnWindow() {
        var ownWindow = testCase.Window.window
        verify(ownWindow !== null)
        ownWindow.requestActivate()
        tryCompare(ownWindow, "active", true)
    }

    // The mirror: activeFocus on items in the harness window is only true
    // while that window holds the WM focus, which a key-synthesis test may
    // have moved to the TestCase's window.
    function activateHarnessWindow() {
        testWindow.requestActivate()
        tryCompare(testWindow, "active", true)
    }

    function cleanupTestCase() {
        if (testWindow) {
            testWindow.close()
            testWindow.destroy()
            testWindow = null
        }
    }

    function componentFor(fileName) {
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/adaptive/gnome/"
                                   + fileName))
        verify(component.status === Component.Ready,
               fileName + ": " + component.errorString())
        return component
    }

    function createAdapter(fileName, properties, parentItem) {
        var component = componentFor(fileName)
        // Components live in the sibling harness window by default; tests
        // that synthesize key events must parent into the TestCase's own
        // window instead, because keyClick delivers only there once a real
        // window manager owns focus.
        var object = component.createObject(parentItem
                                            || testWindow.contentItem,
                                            properties || {})
        verify(object !== null, fileName + ": " + component.errorString())
        wait(0)
        return object
    }

    function windowControl(controls, action) {
        for (var i = 0; i < controls.children.length; ++i) {
            if (controls.children[i].modelData === action)
                return controls.children[i]
        }
        return null
    }

    function test_platformRouteUsesOnlyImmutableGnomeSelection() {
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/adaptive/PlatformAdapter.qml"))
        verify(component.status === Component.Ready,
               component.errorString())
        var adapter = component.createObject(testCase)
        verify(adapter !== null, component.errorString())
        var expectedHost = "/adaptive/HostPageRoute.qml"
        var expectedSettings = "/adaptive/SettingsPageRoute.qml"
        verify(adapter.hostRoute.toString().endsWith(expectedHost))
        verify(adapter.settingsRoute.toString().endsWith(expectedSettings))

        var routeBefore = adapter.hostRoute
        Appearance.colorScheme = 2
        Appearance.accentColor = "#e01b24"
        Appearance.highContrast = true
        Appearance.fontScale = 2.0
        Appearance.reducedMotion = true
        compare(adapter.hostRoute, routeBefore,
                "live appearance must never change the structural route")
        adapter.destroy()
    }

    function test_buttonTracksLightDarkAccentAndContrastLive() {
        var ordinary = createAdapter("GnomeButton.qml", {
                                         text: "Connect",
                                         accessibleName: "Connect to host"
                                     }, testCase)
        var suggested = createAdapter("GnomeButton.qml", {
                                          text: "Connect",
                                          suggested: true,
                                          hoverEnabled: false
                                      })
        var lightSurface = ordinary.background.color.toString()
        compare(ordinary.Accessible.role, Accessible.Button)
        compare(ordinary.Accessible.name, "Connect to host")
        compare(ordinary.background.radius, 9)

        Appearance.colorScheme = 2
        tryCompare(Appearance, "dark", true)
        tryCompare(ordinary.tokens, "dark", true)
        tryVerify(function() {
            return ordinary.background.color.toString() !== lightSurface
        })
        Appearance.accentColor = "#e01b24"
        tryVerify(function() {
            return Qt.colorEqual(suggested.background.color, "#e01b24")
        })
        Appearance.highContrast = true
        tryCompare(ordinary.background.border, "width", 2)

        var clicks = 0
        ordinary.clicked.connect(function() { clicks += 1 })
        ordinary.Accessible.pressAction()
        ordinary.controllerActivate()
        activateOwnWindow()
        ordinary.forceActiveFocus()
        tryCompare(ordinary, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(clicks, 3)
        ordinary.enabled = false
        ordinary.controllerActivate()
        ordinary.Accessible.pressAction()
        compare(clicks, 3)
        ordinary.destroy()
        suggested.destroy()
    }

    function test_dropdownAndTogglePreserveInputSemanticsAndRtl() {
        var dropdown = createAdapter("GnomeDropdown.qml", {
                                         model: ["Windowed", "Fullscreen"],
                                         currentIndex: 0,
                                         accessibleName: "Display mode",
                                         width: 180
                                     })
        compare(dropdown.Accessible.role, Accessible.ComboBox)
        compare(dropdown.Accessible.name, "Display mode")
        dropdown.incrementCurrentIndex()
        compare(dropdown.currentIndex, 1)
        dropdown.controllerActivate()
        tryCompare(dropdown.popup, "opened", true)
        dropdown.controllerActivate()
        tryCompare(dropdown.popup, "opened", false)
        verify(dropdown.indicator.x > dropdown.width / 2)

        testWindow.contentItem.LayoutMirroring.enabled = true
        tryCompare(dropdown, "mirrored", true)
        verify(dropdown.indicator.x < dropdown.width / 2,
               "RTL must move the disclosure indicator to the leading edge")

        var toggle = createAdapter("GnomeToggle.qml", {
                                       text: "Mute host audio",
                                       checked: false,
                                       accessibleName: "Mute host audio"
                                   })
        compare(toggle.Accessible.role, Accessible.CheckBox)
        toggle.Accessible.toggleAction()
        compare(toggle.checked, true)
        toggle.enabled = false
        toggle.controllerActivate()
        toggle.Accessible.toggleAction()
        compare(toggle.checked, true)
        dropdown.destroy()
        toggle.destroy()
    }

    function test_dialogAndMenuExposeSemanticPopupSurfaces() {
        var dialog = createAdapter("GnomeDialog.qml", {
                                       title: "Forget host",
                                       accessibleName: "Forget host dialog",
                                       standardButtons: Dialog.Ok | Dialog.Cancel
                                   })
        compare(dialog.accessibleItem.Accessible.role, Accessible.Dialog)
        compare(dialog.accessibleItem.Accessible.name, "Forget host dialog")
        compare(dialog.accessibleItem, dialog.contentItem.parent,
                "AT-SPI semantics belong to Qt's actual popup, not a duplicate child dialog")
        dialog.accessibleDescription = "Unsaved progress will be lost"
        compare(dialog.accessibleItem.Accessible.description, "Unsaved progress will be lost")
        verify(dialog.background !== null)
        var accepted = 0
        dialog.accepted.connect(function() { accepted += 1 })
        dialog.open()
        tryCompare(dialog, "opened", true)
        dialog.accept()
        tryCompare(dialog, "opened", false)
        compare(accepted, 1)

        var menu = createAdapter("GnomeMenu.qml", {
                                     title: "Host actions",
                                     accessibleName: "Host actions"
                                 })
        var item = Qt.createQmlObject(
                    'import QtQuick.Controls 2.15;'
                    + ' MenuItem { text: "Wake host" }',
                    menu)
        menu.addItem(item)
        compare(menu.accessibleItem.Accessible.role, Accessible.PopupMenu)
        compare(menu.accessibleItem.Accessible.name, "Host actions")
        verify(menu.background !== null)
        menu.open()
        tryCompare(menu, "opened", true)
        menu.close()
        tryCompare(menu, "opened", false)
        dialog.destroy()
        menu.destroy()
    }

    // These tokens name libadwaita colours one-to-one, so they have to carry
    // libadwaita's values. What follows is what the installed GTK/libadwaita
    // actually resolves for each scheme, read back from rendered swatches
    // rather than derived by hand:
    //
    //   --shade-color            light #000000 6.7%    dark #000006 25.1%
    //   --headerbar-shade-color  light #000000 11.8%   dark #000006 36.1%
    //   --disabled-opacity       50% in both schemes
    //   --card-shade-color       light #000000 6.7%    dark #000006 36.1%
    //   --success-color          light #007c3d         dark #78e9ab
    //   --destructive-color      light #c30000         dark #ff938c
    //
    // The card ring and elevation colours come from the .card box-shadow
    // layers (3% and 7%), which have no light/dark variant at all.
    function test_gnomeTokensMatchAdwaitaPalette() {
        var tokens = componentFor("GnomeTokens.qml").createObject(testCase)
        verify(tokens !== null)

        Appearance.colorScheme = 1
        compare(tokens.dark, false)
        fuzzyCompare(tokens.shade.a, 0.067, 0.005)
        fuzzyCompare(tokens.headerbarShade.a, 0.118, 0.005)
        fuzzyCompare(tokens.cardShade.a, 0.067, 0.005)
        fuzzyCompare(tokens.disabledText.a, 0.5, 0.005)
        compare(String(tokens.success), "#007c3d")
        compare(String(tokens.destructiveText), "#c30000")
        fuzzyCompare(tokens.cardRing.a, 0.03, 0.005)
        fuzzyCompare(tokens.cardShadow.a, 0.07, 0.005)

        Appearance.colorScheme = 2
        compare(tokens.dark, true)
        fuzzyCompare(tokens.shade.a, 0.251, 0.005)
        fuzzyCompare(tokens.headerbarShade.a, 0.361, 0.005)
        fuzzyCompare(tokens.cardShade.a, 0.361, 0.005)
        fuzzyCompare(tokens.disabledText.a, 0.5, 0.005)
        compare(String(tokens.success), "#78e9ab")
        compare(String(tokens.destructiveText), "#ff938c")
        // Unchanged across schemes: these are box-shadow layers.
        fuzzyCompare(tokens.cardRing.a, 0.03, 0.005)
        fuzzyCompare(tokens.cardShadow.a, 0.07, 0.005)

        Appearance.reset()
        tokens.destroy()
    }

    // _scale.scss: the modern Adwaita slider is chunky -- trough min-height
    // 10px (pill radius), 20px round handle filled with
    // color-mix(white 80%, --view-bg-color) and ringed by
    // ---slider-border-color at 10%, over a 0 2px 4px drop. A 4px line is the
    // GTK3-era look. Production also snaps on release, never mid-drag.
    function test_sliderMatchesAdwaitaScale() {
        var slider = createAdapter("GnomeSlider.qml", {
                                       from: 0, to: 100, value: 40, width: 220
                                   })
        var trough = null
        var bg = TestCompat.walkQObjectGraph(slider.background)
        for (var b = 0; b < bg.length; ++b) {
            if (bg[b].objectName === "gnome-slider-trough")
                trough = bg[b]
        }
        verify(trough !== null)
        compare(trough.height, 10)
        compare(slider.snapMode, Slider.SnapOnRelease)

        var handle = slider.handle
        verify(handle !== null)
        compare(handle.width, 20)

        // Dark handle: mix(white 80%, #1d1d20) = #d2d2d3, not pure white.
        Appearance.colorScheme = 2
        tryCompare(slider.tokens, "dark", true)
        var knob = null
        var all = TestCompat.walkQObjectGraph(handle)
        for (var i = 0; i < all.length; ++i) {
            if (all[i].radius !== undefined && all[i].border !== undefined
                    && all[i].border.width > 0)
                knob = all[i]
        }
        verify(knob !== null)
        tryVerify(function() {
            return Math.abs(knob.color.r - 0.823) < 0.01
                    && Math.abs(knob.color.b - 0.825) < 0.012
        }, 5000, "dark knob must be mix(white 80%, view), got " + knob.color)
        tryVerify(function() {
            return Math.abs(knob.border.color.a - 0.10) <= 0.005
        }, 5000, "knob ring is ---slider-border-color at 10%, got "
                 + knob.border.color.a)
        Appearance.reset()
        slider.destroy()
    }

    // GNOME surfaces must pick icon assets from the adapter's Appearance,
    // not from the legacy Theme singleton -- the two resolve independently
    // (Theme follows its own desktop probe), and when they diverge every ink
    // glyph loads the wrong-side asset: dark-ink icons on a dark GNOME UI.
    function test_gnomeIconsFollowAppearanceNotLegacyTheme() {
        var row = createAdapter("GnomeStatusPage.qml", {
                                    title: "Studio Mac",
                                    iconName: "os_macos",
                                    width: 220
                                })
        var icon = null
        var all = TestCompat.walkQObjectGraph(row)
        for (var i = 0; i < all.length; ++i) {
            if (all[i].source !== undefined
                    && String(all[i].source).indexOf("os_macos") >= 0)
                icon = all[i]
        }
        verify(icon !== null)

        Appearance.colorScheme = 2
        tryCompare(row.tokens, "dark", true)
        tryVerify(function() {
            return String(icon.source).indexOf("_inkd") >= 0
        }, 5000, "dark GNOME must load the dark-side asset, got " + icon.source)

        Appearance.colorScheme = 1
        tryCompare(row.tokens, "dark", false)
        tryVerify(function() {
            return String(icon.source).indexOf("_inkl") >= 0
        }, 5000, "light GNOME must load the light-side asset, got " + icon.source)
        Appearance.reset()
        row.destroy()
    }

    // .navigation-sidebar (_sidebars.scss): rows are min-height 36, radius 9,
    // padding 0 8px, with an 8px icon-label gap (Files' own row layout).
    // States are currentColor mixes: hover 7%, active 16%, selected 10%,
    // selected+hover 13% -- selection and hover COMBINE rather than one
    // winning.
    function test_sidebarRowMatchesNavigationSidebar() {
        var row = createAdapter("GnomeSidebarRow.qml", {
                                    name: "Studio Mac",
                                    width: 220
                                })
        compare(row.background.radius, 9)
        compare(row.leftPadding, 8)
        compare(row.rightPadding, 8)
        verify(row.implicitHeight >= 36)

        function alphaNear(value, wanted) {
            return Math.abs(value - wanted) <= 0.006
        }
        // Rest: transparent.
        tryVerify(function() { return alphaNear(row.background.color.a, 0) })
        // Hover: 7%.
        mouseMove(row, row.width / 2, row.height / 2)
        tryVerify(function() { return alphaNear(row.background.color.a, 0.07) },
                  5000, "hover fill was " + row.background.color.a)
        // Press: 16%.
        mousePress(row, row.width / 2, row.height / 2)
        tryVerify(function() { return alphaNear(row.background.color.a, 0.16) },
                  5000, "active fill was " + row.background.color.a)
        mouseRelease(row, row.width / 2, row.height / 2)
        // Selected + still hovered: 13%.
        row.selected = true
        tryVerify(function() { return alphaNear(row.background.color.a, 0.13) },
                  5000, "selected+hover fill was " + row.background.color.a)
        // Selected at rest: 10%.
        mouseMove(testWindow.contentItem, 5, testWindow.height - 5)
        tryVerify(function() { return alphaNear(row.background.color.a, 0.10) },
                  5000, "selected fill was " + row.background.color.a)
        row.destroy()
    }

    // The GLOBAL legacy message dialog (hardware-decoder warning, stream
    // errors) must wear the same Adwaita alert anatomy under the GNOME shell:
    // no icon, centred body, and real filled equal-width responses -- not the
    // style's default flat buttons, which is what it rendered when the
    // DialogButtonBox delegate swap silently failed.
    function test_legacyMessageDialogWearsAlertAnatomyUnderGnome() {
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/NavigableMessageDialog.qml"))
        verify(component.status === Component.Ready, component.errorString())
        var dialog = component.createObject(testWindow.contentItem, {
                                                text: "No functioning decoder",
                                                standardButtons: Dialog.Ok | Dialog.Help
                                            })
        verify(dialog !== null, component.errorString())
        compare(dialog.gnomeChrome, true)
        dialog.open()
        tryCompare(dialog, "opened", true)

        var box = dialog.footer
        verify(box !== null)
        tryVerify(function() { return box.count === 2 })
        var first = box.itemAt(0)
        var second = box.itemAt(1)

        // Real alert responses: 12px radius and a visible fill. The style's
        // default button has neither.
        tryVerify(function() {
            return first.background && first.background.radius === 12
        }, 5000, "first response kept the style's default button")
        verify(second.background.radius === 12)
        verify(first.background.color.a > 0.05
               || second.background.color.a > 0.05,
               "responses must carry a fill")

        // Equal widths with the 12px response spacing.
        tryVerify(function() {
            return Math.abs(first.width - second.width) <= 1
        }, 5000, "responses must be equal width")
        var gap = second.x - (first.x + first.width)
        verify(Math.abs(gap - 12) <= 1, "response spacing " + gap)

        // Alert body: no icon, centred text.
        var icons = TestCompat.walkQObjectGraph(dialog.accessibleItem)
            .filter(function(o) { return o && o.name !== undefined
                                  && o.size !== undefined && o.visible === true })
        compare(icons.length, 0, "alerts have no icon")

        dialog.close()
        tryCompare(dialog, "opened", false)
        dialog.destroy()
    }

    // Measured off a real AdwAlertDialog: its responses are equal-width
    // regular buttons in a 44px-tall response area, with the suggested one
    // accent-filled -- not a flat hairline-separated bar. _message-dialog.scss
    // gives .response-area > button min-height 24, min-width 60, padding
    // 10px 20px, border-radius 12.
    function test_dialogResponsesMatchAdwaitaAlert() {
        var plain = createAdapter("GnomeDialogButton.qml", { text: "Help" })
        var ok = createAdapter("GnomeDialogButton.qml",
                               { text: "OK", suggested: true })
        Appearance.interfaceFontPixelSize = 13
        tryCompare(plain.tokens, "bodySize", 13)

        compare(plain.background.radius, 12)
        compare(ok.background.radius, 12)
        compare(plain.topPadding, 10)
        compare(plain.leftPadding, 20)
        verify(plain.implicitHeight >= 40,
               "response measured " + plain.implicitHeight + ", native is 44")
        verify(plain.implicitWidth >= 100,
               "response measured " + plain.implicitWidth + " wide, native min is 100")

        // An ordinary response is a real button, not a transparent one.
        verify(plain.background.color.a > 0.05,
               "ordinary response must carry the neutral button fill")
        tryVerify(function() {
            return Qt.colorEqual(ok.background.color, ok.tokens.accent)
        })
        Appearance.reset()
        plain.destroy()
        ok.destroy()
    }

    // _scrolling.scss: an overlay-indicator scrollbar shows a 3px slider at
    // rest and widens to the full 8px slider width while hovering, when the
    // trough also picks up a currentColor 10% tint. Slider opacity is
    // currentColor at 20/40/60% for rest/hover/active.
    function test_overlayScrollbarMatchesAdwaita() {
        var bar = createAdapter("GnomeScrollBar.qml",
                                { size: 0.3, orientation: Qt.Vertical })
        // These carry Adwaita's 200ms transition, so settle before reading.
        compare(bar.contentItem.implicitWidth, 3)
        tryVerify(function() {
            return Math.abs(bar.contentItem.color.a - 0.2) <= 0.005
        })
        tryVerify(function() {
            return Math.abs(bar.background.color.a - 0) <= 0.005
        })

        bar.active = true
        tryCompare(bar.contentItem, "implicitWidth", 8)
        tryVerify(function() {
            return Math.abs(bar.contentItem.color.a - 0.4) <= 0.005
        })
        tryVerify(function() {
            return Math.abs(bar.background.color.a - 0.1) <= 0.005
        })

        bar.active = false
        tryCompare(bar.contentItem, "implicitWidth", 3)
        bar.destroy()
    }

    // Qt Quick Controls' Material style insets its backgrounds -- 6px top and
    // bottom on a Button -- so a control paints shorter than it measures. The
    // GNOME adapter draws its own Adwaita backgrounds and must not inherit
    // that: a native pill is 44px tall and ours was painting 29.
    //
    // Material is what production selects before the QML engine exists, so
    // these components are always built on top of it.
    function test_gnomeControlsDoNotInheritStyleInsets() {
        var cases = [
            { file: "GnomeButton.qml", props: { text: "Connect", pill: true } },
            { file: "GnomeButton.qml", props: { text: "Connect" } },
            { file: "GnomeHeaderButton.qml", props: { symbol: "plus" } },
            { file: "GnomeDialogButton.qml", props: { text: "Cancel" } },
            { file: "GnomeTextField.qml", props: { text: "Host" } },
            { file: "GnomeDropdown.qml", props: { model: ["A", "B"] } },
            { file: "GnomeToggle.qml", props: { text: "Mute" } },
            { file: "GnomeMenuItem.qml", props: { text: "Rename" } }
        ]
        for (var i = 0; i < cases.length; ++i) {
            var control = createAdapter(cases[i].file, cases[i].props)
            var label = cases[i].file
                    + (cases[i].props.pill ? " (pill)" : "")
            compare(control.topInset, 0, label + " topInset")
            compare(control.bottomInset, 0, label + " bottomInset")
            compare(control.leftInset, 0, label + " leftInset")
            compare(control.rightInset, 0, label + " rightInset")
            if (control.background) {
                compare(control.background.height, control.height,
                        label + ": background must fill the control")
            }
            control.destroy()
        }
    }

    // A native .suggested-action.pill measures 44px tall and a plain button
    // 34px, both read off real GtkButtons at this desktop's font.
    function test_pillAndButtonHeightsMatchNative() {
        var pill = createAdapter("GnomeButton.qml",
                                 { text: "Connect", suggested: true, pill: true })
        var plain = createAdapter("GnomeButton.qml", { text: "Connect" })
        Appearance.interfaceFontPixelSize = 13
        tryCompare(pill.tokens, "bodySize", 13)
        verify(pill.implicitHeight >= 40,
               "pill measured " + pill.implicitHeight + ", native is 44")
        compare(pill.background.height, pill.height)
        compare(plain.implicitHeight, 34)
        compare(plain.background.height, plain.height)
        Appearance.reset()
        pill.destroy()
        plain.destroy()
    }

    // Pins every mapped token to values extracted from the installed
    // GTK/libadwaita by scripts/design/gnome-tokens.py, so nothing here is a
    // transcription that can quietly drift. Regenerate the JSON and run this
    // to see the complete diff at once, rather than discovering values one at
    // a time.
    function tokenReference() {
        return TestCompat.readJson(Qt.resolvedUrl("../data/adwaita-tokens-v1.json"))
    }

    function compareToken(actual, expected, label) {
        var wanted = Qt.color(expected.hex)
        var delta = Math.max(Math.abs(actual.r - wanted.r),
                             Math.abs(actual.g - wanted.g),
                             Math.abs(actual.b - wanted.b))
        verify(delta <= 2 / 255,
               label + ": expected " + expected.hex + ", got " + String(actual))
        verify(Math.abs(actual.a - expected.alpha) <= 0.006,
               label + ": expected alpha " + expected.alpha
               + ", got " + actual.a)
    }

    function test_tokensMatchExtractedAdwaitaReference() {
        var reference = tokenReference()
        var tokens = componentFor("GnomeTokens.qml").createObject(testCase)
        verify(tokens !== null)

        // Starlight token -> libadwaita variable.
        var mapping = {
            "window": "window-bg-color",
            "view": "view-bg-color",
            "headerbar": "headerbar-bg-color",
            "sidebar": "sidebar-bg-color",
            "card": "card-bg-color",
            "cardShade": "card-shade-color",
            "dialogSurface": "dialog-bg-color",
            "popover": "popover-bg-color",
            "text": "window-fg-color",
            "shade": "shade-color",
            "headerbarShade": "headerbar-shade-color",
            "destructive": "destructive-bg-color",
            "destructiveText": "destructive-color",
            "success": "success-color",
            "accentText": "accent-color",
            "headerbarBackdrop": "headerbar-backdrop-color",
            "sidebarBackdrop": "sidebar-backdrop-color",
            "sidebarBorder": "sidebar-border-color",
            "scrollbarOutline": "scrollbar-outline-color"
        }

        var schemes = [{ value: 1, name: "light" }, { value: 2, name: "dark" }]
        for (var s = 0; s < schemes.length; ++s) {
            Appearance.colorScheme = schemes[s].value
            var colors = reference[schemes[s].name].colors
            for (var token in mapping) {
                compareToken(tokens[token], colors[mapping[token]],
                             schemes[s].name + "." + token)
            }
            var opacities = reference[schemes[s].name].opacities
            verify(Math.abs(tokens.border.a - opacities["border-opacity"]) <= 0.006,
                   schemes[s].name + ".border opacity")
            verify(Math.abs(tokens.disabledText.a
                            - opacities["disabled-opacity"]) <= 0.006,
                   schemes[s].name + ".disabled opacity")
            verify(Math.abs(tokens.secondaryText.a
                            - opacities["dim-opacity"]) <= 0.006,
                   schemes[s].name + ".dim opacity")
        }

        var metrics = reference.metrics
        compare(tokens.headerbarHeight, metrics.headerbar_height)
        compare(tokens.rowMinHeight, metrics.row_min_height)
        compare(tokens.contentClamp, metrics.content_clamp)
        compare(tokens.windowControlHitSize, metrics.window_control_button)
        compare(tokens.windowControlSpacing,
                metrics.window_control_pitch - metrics.window_control_button)

        Appearance.reset()
        tokens.destroy()
    }

    // Measured off a real libadwaita widget tree (an AdwPreferencesGroup of
    // AdwActionRow/AdwSwitchRow/AdwComboRow inside an AdwPreferencesPage,
    // laid out by GTK), then corroborated in the stylesheet so a misread
    // allocation cannot drive a change on its own:
    //
    //   boxed-list row   50px   _lists.scss:133 min-height on row > box.header
    //   preferences clamp 600px AdwClamp's maximum-size on a preferences page
    //   header bar        46px  already matched
    //   card radius       12px  already matched
    function test_gnomeMetricsMatchMeasuredNativeReference() {
        var tokens = componentFor("GnomeTokens.qml").createObject(testCase)
        verify(tokens !== null)
        compare(tokens.headerbarHeight, 46)
        compare(tokens.rowMinHeight, 50)
        compare(tokens.contentClamp, 600)
        compare(tokens.radiusCard, 12)
        // Window controls measured 34 overall (min-width 24 + 5px padding each
        // side), a 24px circular image (16px icon + 4px padding) and
        // border-spacing 3. Those already match; pin them so they stay.
        compare(tokens.windowControlHitSize, 34)
        compare(tokens.windowControlImageSize, 24)
        compare(tokens.windowControlIconSize, 16)
        compare(tokens.windowControlSpacing, 3)
        tokens.destroy()
    }

    // libadwaita sizes every label as a percentage of the desktop's interface
    // font; it hardcodes no pixel sizes (_labels.scss):
    //
    //   .title-1 181%/800   .title-2 136%/800   .title-3 136%/700
    //   .title-4 118%/700   .heading 100%/700   .body 100%/400
    //   .caption 82%/400
    //
    // A row subtitle uses `font-size: smaller` (83.33%), which is why the
    // native reference measured 11.11px against a 13.33px body.
    //
    // So the scale has to follow the interface font rather than assume 14px.
    // Measured natively on this desktop: 10pt SF Pro Text = 13.33px body.
    function test_typographyFollowsInterfaceFont() {
        var tokens = componentFor("GnomeTokens.qml").createObject(testCase)
        verify(tokens !== null)

        // Nothing to follow: keep the established fallback so the legacy
        // shell and every non-GNOME route are untouched.
        Appearance.interfaceFontPixelSize = 0
        compare(tokens.bodySize, 14)

        // GNOME reporting this desktop's font.
        Appearance.interfaceFontPixelSize = 13
        compare(tokens.bodySize, 13)
        compare(tokens.captionSize, Math.round(13 * 0.82))
        compare(tokens.titleSize, Math.round(13 * 1.18))
        compare(tokens.largeTitleSize, Math.round(13 * 1.36))
        compare(tokens.heroTitleSize, Math.round(13 * 1.81))
        compare(tokens.subtitleSize, Math.round(13 * 0.8333))

        // A larger desktop font must scale the whole ramp, not just the body.
        Appearance.interfaceFontPixelSize = 20
        compare(tokens.bodySize, 20)
        compare(tokens.captionSize, Math.round(20 * 0.82))
        compare(tokens.heroTitleSize, Math.round(20 * 1.81))

        // Accessibility text scaling still multiplies on top.
        Appearance.fontScale = 1.5
        compare(tokens.bodySize, 30)
        Appearance.reset()
        tokens.destroy()
    }

    function compareColorChannels(actual, expectedHex, tolerance) {
        var expected = Qt.color(expectedHex)
        var delta = Math.max(Math.abs(actual.r - expected.r),
                             Math.abs(actual.g - expected.g),
                             Math.abs(actual.b - expected.b))
        verify(delta <= tolerance,
               "expected " + expectedHex + ", got " + String(actual)
               + " (delta " + (delta * 255).toFixed(1) + "/255)")
    }

    // --accent-color is the standalone accent -- the one used for text, icons
    // and focus, as opposed to --accent-bg-color which fills buttons. It is
    // not a lightened or darkened accent; libadwaita computes it in oklab
    // (_colors.scss):
    //
    //   light: oklab(from @accent_bg_color min(l, 0.5) a b)
    //   dark:  oklab(from @accent_bg_color max(l, 0.85) a b)
    //
    // The accent arrives from the portal and can be any colour, so this has to
    // be computed rather than tabulated. Expected values below are what the
    // installed GTK resolves for those exact expressions.
    function test_standaloneAccentUsesAdwaitaOklabRule() {
        var tokens = componentFor("GnomeTokens.qml").createObject(testCase)
        verify(tokens !== null)

        Appearance.accentColor = "#3584e4"
        Appearance.colorScheme = 1
        compareColorChannels(tokens.accentText, "#0461be", 2 / 255)
        Appearance.colorScheme = 2
        compareColorChannels(tokens.accentText, "#81d1ff", 2 / 255)

        // A second accent proves the rule is applied, not tabulated.
        Appearance.accentColor = "#e62d42"
        Appearance.colorScheme = 1
        compareColorChannels(tokens.accentText, "#c10023", 2 / 255)
        Appearance.colorScheme = 2
        compareColorChannels(tokens.accentText, "#ff888d", 2 / 255)

        // $focus_border_color mixes the STANDALONE accent at 50%, not the
        // background accent.
        compareColorChannels(tokens.focus, "#ff888d", 2 / 255)
        fuzzyCompare(tokens.focus.a, 0.5, 0.005)

        Appearance.reset()
        tokens.destroy()
    }

    // popover.background > contents, from libadwaita 1.9.2
    // src/stylesheet/widgets/_popovers.scss:
    //
    //   border: 1px solid RGB(0 0 0 / 14%);
    //   box-shadow: 0 1px 5px 1px RGB(0 0 0 / 9%),
    //               0 2px 14px 3px RGB(0 0 0 / 5%);
    //   border-radius: $popover_radius;   /* $menu_radius + 6 = 15px */
    //
    // The border is one scheme-independent colour, and both shadow layers
    // carry a downward offset, so the shadow reaches further below the popover
    // than above it. The asset and these margins come from the same native GTK
    // render as the window shadow (scripts/design/gnome-shadows.py).
    function test_gnomeMenuChromeMatchesAdwaitaSource() {
        var menu = createAdapter("GnomeMenu.qml", {
                                     accessibleName: "Host actions"
                                 })
        var background = menu.background
        verify(background !== null)
        compare(background.radius, 15)
        compare(background.border.width, 1)
        compare(background.border.color.r, 0)
        compare(background.border.color.g, 0)
        compare(background.border.color.b, 0)
        fuzzyCompare(background.border.color.a, 0.14, 0.005)

        var shadow = null
        for (var i = 0; i < background.children.length; ++i) {
            if (background.children[i].objectName === "popover.shadow")
                shadow = background.children[i]
        }
        verify(shadow !== null)
        compare(shadow.opacity, 1.0)
        verify(String(shadow.source).indexOf("popover_shadow.png") >= 0)
        compare(shadow.anchors.leftMargin, -14)
        compare(shadow.anchors.rightMargin, -14)
        compare(shadow.anchors.topMargin, -12)
        compare(shadow.anchors.bottomMargin, -16)
        compare(shadow.border.left, 36)
        compare(shadow.border.right, 36)
        compare(shadow.border.top, 36)
        compare(shadow.border.bottom, 36)
        tryVerify(function() { return shadow.status === Image.Ready })
        menu.destroy()
    }

    function test_menuRowsFitLargeText() {
        Appearance.fontScale = 2.0
        var item = createAdapter("GnomeMenuItem.qml", {text: "Keyboard shortcuts"})
        verify(item.height >= item.contentItem.implicitHeight + item.topPadding + item.bottomPadding,
               "Large text must fit inside the menu row without clipping")
        item.destroy()
    }

    function test_reducedMotionMenusOpenAndCloseWithoutAnimationDelay() {
        Appearance.reducedMotion = true
        var menu = createAdapter("GnomeMenu.qml")
        var item = createAdapter("GnomeMenuItem.qml", {text: "Preferences"})
        menu.addItem(item)
        menu.open()
        tryCompare(menu, "opened", true, 50)
        menu.close()
        tryCompare(menu, "visible", false, 50)
        menu.destroy()
    }

    function test_largeTextUpdatesTypographyWithoutScalingGtkGeometry() {
        var field = createAdapter("GnomeTextField.qml", {
                                      placeholderText: "Host address",
                                      accessibleName: "Host address"
                                  })
        var button = createAdapter("GnomeButton.qml", { text: "Connect" })
        var baselineSize = field.font.pixelSize
        var baselineControlHeight = button.tokens.controlHeight
        var baselineFieldHeight = field.tokens.fieldHeight

        Appearance.fontScale = 2.0
        tryCompare(field.font, "pixelSize", baselineSize * 2)
        compare(button.tokens.controlHeight, baselineControlHeight)
        compare(field.tokens.fieldHeight, baselineFieldHeight)
        field.text = "192.0.2.10"
        compare(field.text, "192.0.2.10")
        activateHarnessWindow()
        field.Accessible.pressAction()
        tryCompare(field, "activeFocus", true)
        field.readOnly = true
        compare(field.Accessible.readOnly, true)
        field.destroy()
        button.destroy()
    }

    function test_windowControlsUseRecordedMetricsAndExposeRestoreGlyph() {
        Appearance.windowButtonLayout = ":close,minimize,maximize"
        var controls = createAdapter("GnomeWindowControls.qml", {
                                         side: "right"
                                     })
        // The test runner characterizes this reusable primitive without
        // activating the whole GNOME route, so make its repeater visible.
        controls.visible = true
        tryVerify(function() { return controls.children.length >= 3 })

        var closeControl = windowControl(controls, "close")
        var minimizeControl = windowControl(controls, "minimize")
        var maximizeControl = windowControl(controls, "maximize")
        verify(closeControl !== null)
        verify(minimizeControl !== null)
        verify(maximizeControl !== null)

        compare(controls.tokens.windowControlSize, 34)
        compare(controls.tokens.windowControlSpacing, 3)
        compare(controls.tokens.windowControlImageSize, 24)
        compare(controls.tokens.windowControlIconSize, 16)
        compare(controls.spacing, controls.tokens.windowControlSpacing)
        compare(closeControl.width, controls.tokens.windowControlSize)
        compare(closeControl.height, controls.tokens.windowControlSize)
        compare(closeControl.iconName, "window-close-symbolic")
        compare(minimizeControl.iconName, "window-minimize-symbolic")
        compare(maximizeControl.iconName, "window-maximize-symbolic")

        testWindow.showMaximized()
        tryCompare(testWindow, "visibility", Window.Maximized)
        tryCompare(maximizeControl, "iconName", "window-restore-symbolic")
        testWindow.showNormal()
        tryCompare(testWindow, "visibility", Window.Windowed)

        controls.destroy()
    }

    function test_sliderAndScrollBarKeepAccessibleStepActions() {
        var slider = createAdapter("GnomeSlider.qml", {
                                       from: 0,
                                       to: 10,
                                       stepSize: 2,
                                       value: 4,
                                       accessibleName: "Bitrate"
                                   })
        compare(slider.Accessible.role, Accessible.Slider)
        slider.Accessible.increaseAction()
        compare(slider.value, 6)
        slider.Accessible.decreaseAction()
        compare(slider.value, 4)
        slider.enabled = false
        slider.Accessible.increaseAction()
        compare(slider.value, 4)

        var scrollBar = createAdapter("GnomeScrollBar.qml", {
                                          orientation: Qt.Vertical,
                                          size: 0.25,
                                          position: 0.25,
                                          stepSize: 0.1,
                                          accessibleName: "Host list"
        })
        compare(scrollBar.Accessible.role, Accessible.ScrollBar)
        compare(scrollBar.stepSize, 0.1,
                "the accessible action must receive the instance step size")
        scrollBar.Accessible.increaseAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position - 0.35) < 0.000001
        })
        scrollBar.Accessible.decreaseAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position - 0.25) < 0.000001
        })
        scrollBar.Accessible.scrollDownAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position - 0.35) < 0.000001
        })
        scrollBar.Accessible.scrollUpAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position - 0.25) < 0.000001
        })

        scrollBar.position = 0.7
        scrollBar.Accessible.scrollDownAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position - 0.75) < 0.000001
        }, 1000, "downward actions must clamp at the valid maximum")
        scrollBar.position = 0.05
        scrollBar.Accessible.scrollUpAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position) < 0.000001
        }, 1000, "upward actions must clamp at zero")
        scrollBar.enabled = false
        scrollBar.Accessible.increaseAction()
        tryVerify(function() {
            return Math.abs(scrollBar.position) < 0.000001
        })
        slider.destroy()
        scrollBar.destroy()
    }

    function test_reducedMotionStopsBusyAndTransitionAnimationsLive() {
        var busy = createAdapter("GnomeBusyIndicator.qml", {
                                     running: true,
                                     accessibleName: "Connecting"
                                 })
        var toggle = createAdapter("GnomeToggle.qml", {
                                       checked: false
                                   })
        compare(busy.Accessible.role, Accessible.Animation)
        compare(busy.animationRunning, true)
        verify(toggle.motionDuration > 0)

        Appearance.reducedMotion = true
        tryCompare(busy, "animationRunning", false)
        tryCompare(toggle, "motionDuration", 0)
        compare(busy.Accessible.description, "In progress")
        busy.destroy()
        toggle.destroy()
    }

    function test_navigationUnifiesAssistiveControllerAndKeyboardRoutes() {
        var navigation = createAdapter("GnomeNavigationButton.qml", {
                                           text: "Back",
                                           accessibleName: "Back to hosts"
                                       }, testCase)
        compare(navigation.Accessible.role, Accessible.Button)
        compare(navigation.Accessible.name, "Back to hosts")
        var requests = 0
        navigation.navigationRequested.connect(function() { requests += 1 })
        navigation.Accessible.pressAction()
        navigation.controllerActivate()
        activateOwnWindow()
        navigation.forceActiveFocus()
        tryCompare(navigation, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(requests, 3)
        navigation.enabled = false
        navigation.Accessible.pressAction()
        navigation.controllerActivate()
        compare(requests, 3)
        navigation.destroy()
    }

    function test_narrowControlsClipContentWithoutOverflow() {
        var button = createAdapter("GnomeButton.qml", {
                                       width: 120,
                                       text: "Connect to a very long host name"
                                   })
        var dropdown = createAdapter("GnomeDropdown.qml", {
                                         width: 140,
                                         model: ["A very long display mode label"],
                                         currentIndex: 0
                                     })
        verify(button.contentItem.width <= button.availableWidth)
        compare(dropdown.contentItem.elide, Text.ElideRight)
        verify(dropdown.indicator.x >= 0)
        verify(dropdown.indicator.x + dropdown.indicator.width
               <= dropdown.width)
        button.destroy()
        dropdown.destroy()
    }
}
