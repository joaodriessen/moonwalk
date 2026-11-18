import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import Starlight 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PlasmaAdapter"
    when: windowShown

    property var testWindow: null
    property url initialView:
        Qt.resolvedUrl("fixtures/EmptyInitialView.qml")
    property bool runConfigChecks: false
    property bool adaptiveShellRequested: false

    Component {
        id: nativeButton
        Button {}
    }
    Component {
        id: nativeComboBox
        ComboBox {}
    }
    Component {
        id: nativeSwitch
        CheckBox {}
    }
    Component {
        id: nativeTextField
        TextField {}
    }
    Component {
        id: nativeSlider
        Slider {}
    }
    Component {
        id: nativeScrollBar
        ScrollBar {}
    }
    Component {
        id: nativeBusy
        BusyIndicator {}
    }
    Component {
        id: nativeToolButton
        ToolButton {}
    }

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

    function cleanupTestCase() {
        if (testWindow) {
            testWindow.close()
            testWindow.destroy()
            testWindow = null
        }
    }

    function componentFor(fileName) {
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/adaptive/plasma/"
                                   + fileName))
        verify(component.status === Component.Ready,
               fileName + ": " + component.errorString())
        return component
    }

    function createAdapter(fileName, properties) {
        var component = componentFor(fileName)
        var initialProperties = properties || {}
        var object = component.createObject(testWindow.contentItem,
                                            initialProperties)
        verify(object !== null, fileName + ": " + component.errorString())
        wait(0)
        return object
    }

    function createNative(component, properties) {
        var object = component.createObject(testWindow.contentItem,
                                            properties || {})
        verify(object !== null)
        wait(0)
        return object
    }

    function destroyPair(adapter, nativeControl) {
        if (adapter) {
            adapter.destroy()
        }
        if (nativeControl) {
            nativeControl.destroy()
        }
    }

    function canonicalWidgetStyle(styleName) {
        return styleName.toString().split(":")[0].trim().toLowerCase()
    }

    function compareSelectedStyleGeometry(adapter, nativeControl, family) {
        compare(adapter.leftPadding, nativeControl.leftPadding,
                family + " must retain the selected style's left padding")
        compare(adapter.rightPadding, nativeControl.rightPadding,
                family + " must retain the selected style's right padding")
        compare(adapter.topPadding, nativeControl.topPadding,
                family + " must retain the selected style's top padding")
        compare(adapter.bottomPadding, nativeControl.bottomPadding,
                family + " must retain the selected style's bottom padding")
        compare(adapter.background === null,
                nativeControl.background === null,
                family + " must preserve whether the selected style supplies a background")
        if (adapter.background !== null) {
            compare(adapter.background.implicitWidth,
                    nativeControl.background.implicitWidth,
                    family + " must not substitute an app-painted background")
            compare(adapter.background.implicitHeight,
                    nativeControl.background.implicitHeight,
                    family + " must not substitute an app-painted background")
        }
    }

    function test_platformAdapterSelectsOnlyTheConfirmedRoute() {
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/adaptive/PlatformAdapter.qml"))
        verify(component.status === Component.Ready,
               component.errorString())
        var adapter = component.createObject(testCase)
        verify(adapter !== null, component.errorString())
        var expectedHostRoute = "/adaptive/HostPageRoute.qml"
        var expectedSettingsRoute = "/adaptive/SettingsPageRoute.qml"
        verify(adapter.hostRoute.toString().endsWith(expectedHostRoute),
               "host route must match the preflight-confirmed adapter")
        verify(adapter.settingsRoute.toString().endsWith(expectedSettingsRoute),
               "settings route must match the preflight-confirmed adapter")
        adapter.destroy()
    }

    function test_selectedStyleDelegatesAreCapabilityChecked() {
        failOnWarning(/Property 'color' does not exist on QQuickItem/)
        if (plasmaWidgetStyleExpected !== "") {
            var reference = createNative(nativeButton)
            verify(reference.background !== null,
                   "the KDE button must expose its native style delegate")
            verify(typeof reference.background.styleName !== "undefined",
                   "the KDE delegate must report its effective widget style")
            compare(canonicalWidgetStyle(reference.background.styleName),
                    canonicalWidgetStyle(plasmaWidgetStyleExpected),
                    "the focused matrix must exercise its labelled widget style")
            reference.destroy()
        }
        var component = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/starlight/SLBusy.qml"))
        verify(component.status === Component.Ready,
               component.errorString())
        var busy = component.createObject(testWindow.contentItem,
                                          { running: true })
        verify(busy !== null, component.errorString())
        wait(0)
        busy.destroy()
    }

    function test_realSharedTooltipCapabilityChecksTheKdeDelegate() {
        failOnWarning(/Property 'color' does not exist on QQuickItem/)
        failOnWarning(/QML ToolTip: cannot find any window to open popup in/)
        var launcherComponent = Qt.createComponent(
                    Qt.resolvedUrl("../../app/gui/main.qml"))
        verify(launcherComponent.status === Component.Ready,
               launcherComponent.errorString())
        var launcher = launcherComponent.createObject(null)
        verify(launcher !== null, launcherComponent.errorString())
        launcher.show()
        waitForRendering(launcher.contentItem)
        var toolTip = launcher.sharedToolTip
        verify(toolTip !== null,
               "the real shared ToolTip must remain available")
        toolTip.parent = launcher.contentItem
        compare(toolTip.parent, launcher.contentItem,
                "the direct Popup probe must retain its launcher window owner")
        toolTip.text = "Native tooltip capability probe"
        toolTip.visible = true
        wait(0)
        verify(toolTip.contentItem !== null)
        if ("color" in toolTip.contentItem) {
            verify(toolTip.contentItem.color !== undefined)
        }
        toolTip.visible = false
        launcher.close()
        launcher.destroy()
    }

    function test_buttonUsesNativeStyleAndOneActivationRoute() {
        var button = createAdapter("PlasmaButton.qml", {
                                       objectName: "starlight.plasma.connect",
                                       text: "Connect",
                                       accessibleName: "Connect to host"
                                   })
        var reference = createNative(nativeButton, { text: "Connect" })
        compareSelectedStyleGeometry(button, reference, "button")
        compare(button.Accessible.role, Accessible.Button)
        if ("id" in button.Accessible) {
            compare(button.Accessible["id"], button.objectName)
        }
        compare(button.Accessible.name, "Connect to host")

        var clicks = 0
        button.clicked.connect(function() { clicks += 1 })
        button.Accessible.pressAction()
        button.controllerActivate()
        button.forceActiveFocus()
        tryCompare(button, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(clicks, 3,
                "assistive, controller, and keyboard routes must each activate once")
        button.enabled = false
        button.Accessible.pressAction()
        button.controllerActivate()
        compare(clicks, 3, "disabled activation must fail closed")
        destroyPair(button, reference)
    }

    function test_dropdownUsesNativePopupAndSelectionBehavior() {
        var choices = ["Windowed", "Fullscreen", "Borderless"]
        var dropdown = createAdapter("PlasmaDropdown.qml", {
                                         objectName: "starlight.plasma.mode",
                                         model: choices,
                                         currentIndex: 0,
                                         accessibleName: "Display mode"
                                     })
        var reference = createNative(nativeComboBox, {
                                         model: choices,
                                         currentIndex: 0
                                     })
        compareSelectedStyleGeometry(dropdown, reference, "dropdown")
        compare(dropdown.Accessible.role, Accessible.ComboBox)
        compare(dropdown.Accessible.name, "Display mode")
        dropdown.incrementCurrentIndex()
        compare(dropdown.currentIndex, 1)
        dropdown.controllerActivate()
        tryCompare(dropdown.popup, "opened", true)
        dropdown.controllerActivate()
        tryCompare(dropdown.popup, "opened", false)
        destroyPair(dropdown, reference)
    }

    function test_toggleUsesNativeIndicatorAndHonorsDisabledState() {
        var toggle = createAdapter("PlasmaToggle.qml", {
                                       objectName: "starlight.plasma.audio",
                                       text: "Mute host audio",
                                       checked: false,
                                       accessibleName: "Mute host audio"
                                   })
        var reference = createNative(nativeSwitch, {
                                         text: "Mute host audio",
                                         checked: false
                                     })
        compareSelectedStyleGeometry(toggle, reference, "toggle")
        verify(toggle.indicator !== null,
               "toggle must retain the selected style's indicator")
        compare(toggle.indicator.implicitWidth,
                reference.indicator.implicitWidth)
        compare(toggle.Accessible.role, Accessible.CheckBox)
        compare(toggle.Accessible.checked, false)
        toggle.Accessible.toggleAction()
        compare(toggle.checked, true)
        toggle.enabled = false
        toggle.controllerActivate()
        toggle.Accessible.toggleAction()
        compare(toggle.checked, true)
        destroyPair(toggle, reference)
    }

    function test_dialogKeepsNativeChromeAndAcceptRejectBehavior() {
        var dialog = createAdapter("PlasmaDialog.qml", {
                                       objectName: "starlight.plasma.confirm",
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
        verify(dialog.background !== null,
               "dialog must retain the selected style's background")
        verify(dialog.footer !== null,
               "standard buttons must retain the selected style's footer")
        var accepted = 0
        dialog.accepted.connect(function() { accepted += 1 })
        dialog.open()
        tryCompare(dialog, "opened", true)
        dialog.accept()
        tryCompare(dialog, "opened", false)
        compare(accepted, 1)
        dialog.destroy()
    }

    function test_menuKeepsNativeDelegatesAndTriggersOneAction() {
        var menu = createAdapter("PlasmaMenu.qml", {
                                     objectName: "starlight.plasma.host-menu",
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
        verify(menu.background !== null,
               "menu must retain the selected style's popup background")
        var triggers = 0
        item.triggered.connect(function() { triggers += 1 })
        menu.open()
        tryCompare(menu, "opened", true)
        item.triggered()
        compare(triggers, 1)
        menu.close()
        tryCompare(menu, "opened", false)
        menu.destroy()
    }

    function test_fieldKeepsNativeFrameAndEditableSemantics() {
        var field = createAdapter("PlasmaTextField.qml", {
                                      objectName: "starlight.plasma.host-field",
                                      placeholderText: "Host address",
                                      accessibleName: "Host address"
                                  })
        var reference = createNative(nativeTextField, {
                                         placeholderText: "Host address"
                                     })
        compareSelectedStyleGeometry(field, reference, "field")
        compare(field.Accessible.role, Accessible.EditableText)
        compare(field.Accessible.name, "Host address")
        field.text = "192.0.2.10"
        compare(field.text, "192.0.2.10")
        field.Accessible.pressAction()
        tryCompare(field, "activeFocus", true)
        field.readOnly = true
        compare(field.Accessible.readOnly, true)
        destroyPair(field, reference)
    }

    function test_sliderUsesNativeHandleAndAccessibleStepActions() {
        var slider = createAdapter("PlasmaSlider.qml", {
                                       objectName: "starlight.plasma.bitrate",
                                       from: 0,
                                       to: 10,
                                       stepSize: 2,
                                       value: 4,
                                       accessibleName: "Bitrate"
                                   })
        var reference = createNative(nativeSlider, {
                                         from: 0,
                                         to: 10,
                                         stepSize: 2,
                                         value: 4
                                     })
        compareSelectedStyleGeometry(slider, reference, "slider")
        verify(slider.handle !== null,
               "slider must retain the selected style's handle")
        compare(slider.handle.implicitWidth, reference.handle.implicitWidth)
        compare(slider.Accessible.role, Accessible.Slider)
        slider.Accessible.increaseAction()
        compare(slider.value, 6)
        slider.Accessible.decreaseAction()
        compare(slider.value, 4)
        slider.enabled = false
        slider.Accessible.increaseAction()
        compare(slider.value, 4)
        destroyPair(slider, reference)
    }

    function test_scrollbarUsesNativeThumbAndScrollActions() {
        var scrollBar = createAdapter("PlasmaScrollBar.qml", {
                                          objectName: "starlight.plasma.scroll",
                                          orientation: Qt.Vertical,
                                          size: 0.25,
                                          position: 0.25,
                                          stepSize: 0.1,
                                          accessibleName: "Host list scroll bar"
                                      })
        var reference = createNative(nativeScrollBar, {
                                         orientation: Qt.Vertical,
                                         size: 0.25,
                                         position: 0.25,
                                         stepSize: 0.1
                                     })
        compareSelectedStyleGeometry(scrollBar, reference, "scrollbar")
        verify(scrollBar.contentItem !== null,
               "scrollbar must retain the selected style's thumb")
        compare(scrollBar.contentItem.implicitWidth,
                reference.contentItem.implicitWidth)
        compare(scrollBar.Accessible.role, Accessible.ScrollBar)
        scrollBar.Accessible.increaseAction()
        compare(scrollBar.position, 0.35)
        scrollBar.Accessible.decreaseAction()
        compare(scrollBar.position, 0.25)
        scrollBar.enabled = false
        scrollBar.Accessible.increaseAction()
        compare(scrollBar.position, 0.25)
        destroyPair(scrollBar, reference)
    }

    function test_busyUsesSelectedStyleAndRunningSemantics() {
        var busy = createAdapter("PlasmaBusyIndicator.qml", {
                                     objectName: "starlight.plasma.busy",
                                     running: false,
                                     accessibleName: "Connecting"
                                 })
        var reference = createNative(nativeBusy, { running: false })
        compareSelectedStyleGeometry(busy, reference, "busy")
        compare(busy.contentItem.implicitWidth,
                reference.contentItem.implicitWidth,
                "busy indicator must retain selected-style animation geometry")
        compare(busy.Accessible.role, Accessible.Animation)
        compare(busy.Accessible.name, "Connecting")
        compare(busy.busyState, false)
        compare(busy.Accessible.description, "Idle")
        busy.running = true
        compare(busy.busyState, true)
        compare(busy.Accessible.description, "In progress")
        if (plasmaReducedMotionExpected) {
            verify(typeof busy.contentItem.animationShouldBeRunning
                   !== "undefined",
                   "the KDE style must expose its reduced-motion animation gate")
            compare(busy.contentItem.animationShouldBeRunning, false,
                    "the active KDE style must suppress its animation when"
                    + " desktop animation duration is disabled")
        }
        destroyPair(busy, reference)
    }

    function test_navigationUsesNativeToolButtonAndSharedInputRoute() {
        var navigation = createAdapter("PlasmaNavigationButton.qml", {
                                           objectName: "starlight.plasma.back",
                                           text: "Back",
                                           accessibleName: "Back to hosts"
                                       })
        var reference = createNative(nativeToolButton, { text: "Back" })
        compareSelectedStyleGeometry(navigation, reference, "navigation")
        compare(navigation.Accessible.role, Accessible.Button)
        compare(navigation.Accessible.name, "Back to hosts")
        var requests = 0
        navigation.navigationRequested.connect(function() { requests += 1 })
        navigation.Accessible.pressAction()
        navigation.controllerActivate()
        navigation.forceActiveFocus()
        tryCompare(navigation, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(requests, 3,
                "assistive, controller, and keyboard routes must each navigate once")
        navigation.enabled = false
        navigation.Accessible.pressAction()
        navigation.controllerActivate()
        compare(requests, 3)
        destroyPair(navigation, reference)
    }

    function test_destructiveAndDisabledRowsUsePaletteRoles() {
        var active = ({
            window: "#202124", windowText: "#e8eaed", base: "#17181a",
            alternateBase: "#292a2d", text: "#e8eaed",
            placeholderText: "#8a8d91", button: "#303134",
            buttonText: "#f1f3f4", highlight: "#6f42c1",
            highlightedText: "#ffffff", toolTipBase: "#202124",
            toolTipText: "#e8eaed", light: "#4b4d51",
            midlight: "#3c3e42", mid: "#303236", dark: "#151619",
            shadow: "#050506"
        })
        var disabled = Object.assign({}, active)
        disabled.text = "#83758f"
        PlasmaAppearance.active = active
        PlasmaAppearance.inactive = active
        PlasmaAppearance.disabled = disabled
        PlasmaAppearance.ready = true
        PlasmaAppearance.revision += 1

        var row = createAdapter("PlasmaButtonRow.qml", {
                                    title: "Remove host",
                                    subtitle: "Cannot be undone",
                                    destructive: true,
                                    width: 420
                                })
        wait(0)

        var title = TestCompat.walkQObjectGraph(row).filter(function(object) {
            return object.objectName === "plasmaButtonRowTitle"
        })[0]
        verify(title !== undefined)
        compare(title.color, "#6f42c1")

        row.enabled = false
        wait(0)
        compare(title.color, "#83758f")
        row.destroy()
        PlasmaAppearance.reset()
    }
}
