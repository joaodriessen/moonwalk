import QtQuick 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import "../../app/gui/starlight/ControlCompat.js" as ControlCompat
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "AccessibilityContract"
    when: windowShown

    property var testWindow: null

    // PcView and SettingsView receive these context properties from main.qml
    // in production. Keeping the adapters here lets the test load the real
    // page QML rather than a semantic-only fixture.
    property var stackView: stackViewAdapter
    property var window: windowAdapter
    property var addPcDialog: addPcDialogAdapter

    QtObject {
        id: stackViewAdapter
        property int focusRequests: 0

        function reset() {
            focusRequests = 0
        }

        function forceActiveFocus() {
            focusRequests += 1
        }

        function push() {
        }
    }

    QtObject {
        id: windowAdapter
        property url settingsRoute: Qt.resolvedUrl(
                                        "../../app/gui/adaptive/SettingsPageRoute.qml")
        property bool pollingActive: true
        property bool clearOnBack: false
        property int goBackCalls: 0
        property int navigateToCalls: 0

        function showToast() {
        }

        function navigateTo() {
            navigateToCalls += 1
        }

        function goBack() {
            goBackCalls += 1
        }

        function reset() {
            goBackCalls = 0
            navigateToCalls = 0
        }
    }

    QtObject {
        id: addPcDialogAdapter
        property int openCalls: 0

        function reset() {
            openCalls = 0
        }

        function open() {
            openCalls += 1
        }
    }

    function realComponent(relativePath) {
        var component = Qt.createComponent(Qt.resolvedUrl(relativePath))
        verify(component.status === Component.Ready, component.errorString())
        return component
    }

    function createReal(relativePath, stableObjectName, properties, parentObject) {
        var component = realComponent(relativePath)
        var initialProperties = properties || {}
        if (relativePath.indexOf("HostPageRoute.qml") !== -1 &&
                initialProperties.hostSource === undefined) {
            initialProperties.hostSource = Qt.createQmlObject(
                        'import Starlight.Test 1.0; TestHostSource {}', testCase,
                        "accessibility-host-source")
        }
        initialProperties.objectName = stableObjectName
        var object = component.createObject(
                    parentObject || testWindow.contentItem,
                    initialProperties)
        verify(object !== null, component.errorString())
        compare(object.objectName, stableObjectName,
                "The caller supplied objectName must remain the stable automation identifier")
        wait(0)
        return object
    }

    function initTestCase() {
        testWindow = Qt.createQmlObject(
                    'import QtQuick.Window 2.15; Window {'
                    + ' width: 1000; height: 800; visible: true }',
                    testCase,
                    "accessibility-contract-window")
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

    function destroyObject(object) {
        if (object) {
            object.destroy()
            // QML destruction is deferred. Let the object leave the focus
            // chain before the next test creates controls in the same window.
            wait(0)
        }
    }

    function hasPublicProperty(object, propertyName) {
        var available = object[propertyName] !== undefined
        verify(available, object.objectName + " must expose public " + propertyName)
        return available
    }

    function verifyStableAccessibleId(object, expectedStableId) {
        var stableId = expectedStableId || object.objectName
        verify(stableId.length > 0,
               "The semantic object must retain a stable objectName")
        if ("id" in object.Accessible) {
            compare(object.Accessible["id"], stableId)
        }
    }

    function verifyLabelRelationship(control, expectedName) {
        compare(control.Accessible.name, expectedName)
        if (!("labelledBy" in control.Accessible)) {
            return
        }

        var label = control.Accessible["labelledBy"]
        verify(label !== null,
               "A Qt version with accessibility relations must expose the visible label")
        compare(label.Accessible.name, expectedName)
        if ("labelFor" in label.Accessible) {
            compare(label.Accessible["labelFor"], control)
        }
    }

    function findByObjectName(object, stableObjectName) {
        if (!object) {
            return null
        }

        if (object.objectName === stableObjectName) {
            return object
        }

        var children = object.children || []
        for (var i = 0; i < children.length; ++i) {
            var childMatch = findByObjectName(children[i], stableObjectName)
            if (childMatch) {
                return childMatch
            }
        }

        var data = object.data || []
        for (var j = 0; j < data.length; ++j) {
            var dataObject = data[j]
            if (!TestCompat.containsIdentity(children, dataObject)) {
                var dataMatch = findByObjectName(dataObject, stableObjectName)
                if (dataMatch) {
                    return dataMatch
                }
            }
        }

        return null
    }

    function test_buttonPublishesCallerSuppliedSemanticsAndPressAction() {
        var button = createReal("../../app/gui/starlight/SLButton.qml",
                                "starlight.test.connect-button",
                                { text: "Connect" })
        if (!hasPublicProperty(button, "accessibleName")
                || !hasPublicProperty(button, "accessibleDescription")) {
            destroyObject(button)
            return
        }

        button.accessibleName = "Connect to test host"
        button.accessibleDescription = "Starts a stream to Test host"
        var clicks = 0
        button.clicked.connect(function() { clicks += 1 })

        compare(button.Accessible.role, Accessible.Button)
        verifyStableAccessibleId(button)
        compare(button.Accessible.name, button.accessibleName)
        compare(button.Accessible.description, button.accessibleDescription)
        verify(button.Accessible.focusable,
               "Button focusable state: enabled=" + button.enabled
               + ", visible=" + button.visible
               + ", focusPolicy=" + button.focusPolicy
               + ", activeFocus=" + button.activeFocus)

        button.forceActiveFocus()
        tryCompare(button, "activeFocus", true)
        compare(button.Accessible.focused, true)

        button.Accessible.pressAction()
        compare(clicks, 1, "Accessible press must follow the same click route as keyboard and pointer activation")
        button.enabled = false
        button.Accessible.pressAction()
        compare(clicks, 1,
                "A disabled button must reject the same assistive action that pointer and keyboard activation reject")
        destroyObject(button)
    }

    function test_togglePublishesCheckedStateAndToggleAction() {
        var toggle = createReal("../../app/gui/starlight/SLToggle.qml",
                                "starlight.test.mute-toggle",
                                { text: "Mute host speakers", checked: false })
        if (!hasPublicProperty(toggle, "accessibleName")
                || !hasPublicProperty(toggle, "accessibleDescription")) {
            destroyObject(toggle)
            return
        }

        toggle.accessibleName = "Mute host speakers"
        toggle.accessibleDescription = "Silences the host while streaming"
        var wanted = []
        toggle.switched.connect(function(value) { wanted.push(value) })

        compare(toggle.Accessible.role, Accessible.CheckBox)
        verifyStableAccessibleId(toggle)
        compare(toggle.Accessible.name, toggle.accessibleName)
        compare(toggle.Accessible.description, toggle.accessibleDescription)
        compare(toggle.Accessible.checkable, true)
        compare(toggle.Accessible.checked, false)

        toggle.Accessible.toggleAction()
        compare(JSON.stringify(wanted), JSON.stringify([true]))
        toggle.enabled = false
        toggle.Accessible.pressAction()
        toggle.Accessible.toggleAction()
        compare(JSON.stringify(wanted), JSON.stringify([true]),
                "A disabled toggle must not emit a preference-changing assistive action")
        destroyObject(toggle)
    }

    function test_dropdownPublishesSelectionAndPressAction() {
        var dropdown = createReal("../../app/gui/starlight/SLDropdown.qml",
                                  "starlight.test.display-mode",
                                  { model: [ { text: "Windowed", value: 1 },
                                             { text: "Fullscreen", value: 2 } ],
                                    currentIndex: 0 })
        if (!hasPublicProperty(dropdown, "accessibleName")
                || !hasPublicProperty(dropdown, "accessibleDescription")
                || !hasPublicProperty(dropdown, "popupOpen")) {
            destroyObject(dropdown)
            return
        }

        dropdown.accessibleName = "Streaming display mode"
        dropdown.accessibleDescription = "Changes how the streaming window opens"

        compare(dropdown.Accessible.role, Accessible.ComboBox)
        verifyStableAccessibleId(dropdown)
        compare(dropdown.Accessible.name, dropdown.accessibleName)
        compare(dropdown.Accessible.description, dropdown.accessibleDescription)
        compare(dropdown.popupOpen, false)

        dropdown.Accessible.pressAction()
        tryCompare(dropdown, "popupOpen", true)
        dropdown.Accessible.pressAction()
        tryCompare(dropdown, "popupOpen", false)
        dropdown.enabled = false
        dropdown.Accessible.pressAction()
        compare(dropdown.popupOpen, false,
                "A disabled combo box must not open its popup through an assistive action")
        destroyObject(dropdown)
    }

    function test_sliderPublishesValueStateAndIncrementActions() {
        var slider = createReal("../../app/gui/starlight/SLSlider.qml",
                                "starlight.test.bitrate",
                                { from: 0, to: 10, value: 5, stepSize: 1 })
        if (!hasPublicProperty(slider, "accessibleName")
                || !hasPublicProperty(slider, "accessibleDescription")) {
            destroyObject(slider)
            return
        }

        slider.accessibleName = "Video bitrate"
        slider.accessibleDescription = "Sets the streaming video bitrate"

        compare(slider.Accessible.role, Accessible.Slider)
        verifyStableAccessibleId(slider)
        compare(slider.Accessible.name, slider.accessibleName)
        compare(slider.Accessible.description, slider.accessibleDescription)
        // Qt's Slider accessible bridge reads these standard Slider properties;
        // they are deliberately not duplicate properties on Accessible.
        compare(slider.value, 5)
        compare(slider.from, 0)
        compare(slider.to, 10)
        compare(slider.stepSize, 1)

        slider.Accessible.increaseAction()
        compare(slider.value, 6)
        slider.Accessible.decreaseAction()
        compare(slider.value, 5)
        slider.enabled = false
        slider.Accessible.increaseAction()
        slider.Accessible.decreaseAction()
        compare(slider.value, 5,
                "A disabled slider must not change value through assistive increment/decrement actions")
        destroyObject(slider)
    }

    function test_textFieldPublishesEditableSemanticsAndFocusTarget() {
        var field = createReal("../../app/gui/starlight/SLTextField.qml",
                               "starlight.test.host-address",
                               { placeholderText: "Host address" })
        if (!hasPublicProperty(field, "accessibleName")
                || !hasPublicProperty(field, "accessibleDescription")
                || !hasPublicProperty(field, "accessibleItem")) {
            destroyObject(field)
            return
        }

        field.accessibleName = "Host address"
        field.accessibleDescription = "Enter the hostname or IP address"
        verify(field.accessibleItem !== null)

        compare(field.accessibleItem.Accessible.role, Accessible.EditableText)
        verifyStableAccessibleId(field.accessibleItem, field.objectName)
        compare(field.accessibleItem.Accessible.name, field.accessibleName)
        compare(field.accessibleItem.Accessible.description, field.accessibleDescription)
        compare(field.accessibleItem.Accessible.editable, true)
        compare(field.accessibleItem.Accessible.readOnly, false)
        compare(field.accessibleItem.Accessible.focusable, true)

        field.forceActiveFocus()
        tryCompare(field.accessibleItem, "activeFocus", true)
        compare(field.accessibleItem.Accessible.focused, true)
        destroyObject(field)
    }

    function test_rowLabelsTextFieldSemanticFocusTarget() {
        var row = createReal("../../app/gui/starlight/SLRow.qml",
                             "starlight.test.host-address-row",
                             { label: "Host address",
                               sublabel: "Enter the hostname or IP address" })
        var field = createReal("../../app/gui/starlight/SLTextField.qml",
                               "starlight.test.row-host-address",
                               { placeholderText: "Host address" })
        if (!hasPublicProperty(field, "accessibleItem")) {
            destroyObject(field)
            destroyObject(row)
            return
        }

        row.accessibleControl = field
        wait(0)

        verifyLabelRelationship(field.accessibleItem, "Host address")
        compare(field.accessibleItem.Accessible.name, "Host address")
        compare(field.accessibleItem.Accessible.description,
                "Enter the hostname or IP address")
        destroyObject(field)
        destroyObject(row)
    }

    function test_rowDefaultTrailingContentLabelsCompositeSemanticFocusTarget() {
        var row = Qt.createQmlObject(
                    'import QtQuick 2.15\n'
                    + 'import "../../app/gui/starlight" as Starlight\n'
                    + 'Starlight.SLRow {\n'
                    + '    objectName: "starlight.test.default-row"\n'
                    + '    label: "Host address"\n'
                    + '    sublabel: "Enter the hostname or IP address"\n'
                    + '    property Item testField: field\n'
                    + '    Starlight.SLTextField {\n'
                    + '        id: field\n'
                    + '        objectName: "starlight.test.default-row-host-address"\n'
                    + '        placeholderText: "Host address"\n'
                    + '    }\n'
                    + '}',
                    testCase,
                    "accessibility-default-row.qml")
        verify(row !== null)
        var field = row.testField
        verify(field !== null)
        wait(0)

        verifyLabelRelationship(field.accessibleItem, "Host address")
        compare(field.accessibleItem.Accessible.name, "Host address")
        compare(field.accessibleItem.Accessible.description,
                "Enter the hostname or IP address")
        destroyObject(row)
    }

    function test_disclosurePublishesHeaderActionWithoutChangingFocusContract() {
        var disclosure = createReal("../../app/gui/starlight/SLDisclosure.qml",
                                    "starlight.test.advanced",
                                    { title: "Advanced", hint: "Optional", expanded: false })
        if (!hasPublicProperty(disclosure, "accessibleName")
                || !hasPublicProperty(disclosure, "accessibleDescription")
                || !hasPublicProperty(disclosure, "accessibleItem")) {
            destroyObject(disclosure)
            return
        }

        disclosure.accessibleName = "Advanced settings"
        disclosure.accessibleDescription = "Shows optional settings"
        verify(disclosure.accessibleItem !== null)

        compare(disclosure.accessibleItem.Accessible.role, Accessible.Button)
        verifyStableAccessibleId(disclosure.accessibleItem,
                                 disclosure.objectName)
        compare(disclosure.accessibleItem.Accessible.name, disclosure.accessibleName)
        compare(disclosure.accessibleItem.Accessible.description, disclosure.accessibleDescription)
        compare(disclosure.accessibleItem.Accessible.checkable, true)
        compare(disclosure.accessibleItem.Accessible.checked, false)
        compare(disclosure.expanded, false)

        disclosure.accessibleItem.Accessible.pressAction()
        compare(disclosure.expanded, true)
        compare(disclosure.accessibleItem.Accessible.checked, true)
        disclosure.accessibleItem.Accessible.pressAction()
        compare(disclosure.expanded, false)
        disclosure.enabled = false
        disclosure.accessibleItem.Accessible.pressAction()
        disclosure.accessibleItem.Accessible.toggleAction()
        compare(disclosure.expanded, false,
                "A disabled disclosure must not expose an action that changes its expanded state")
        destroyObject(disclosure)
    }

    function test_scrollBarPublishesScrollableSemantics() {
        var scrollBar = createReal("../../app/gui/starlight/SLScrollBar.qml",
                                   "starlight.test.hosts-scrollbar",
                                   { size: 0.25, position: 0.25 })
        if (!hasPublicProperty(scrollBar, "accessibleName")
                || !hasPublicProperty(scrollBar, "accessibleDescription")) {
            destroyObject(scrollBar)
            return
        }

        scrollBar.accessibleName = "Hosts list scroll bar"
        scrollBar.accessibleDescription = "Scrolls through available hosts"

        compare(scrollBar.Accessible.role, Accessible.ScrollBar)
        verifyStableAccessibleId(scrollBar)
        compare(scrollBar.Accessible.name, scrollBar.accessibleName)
        compare(scrollBar.Accessible.description, scrollBar.accessibleDescription)
        compare(scrollBar.Accessible.focusable,
                scrollBar.visible && scrollBar.enabled && scrollBar.interactive)
        var initialPosition = scrollBar.position
        scrollBar.Accessible.increaseAction()
        verify(scrollBar.position > initialPosition,
               "Accessible increase must move the same scroll bar value as pointer scrolling")
        scrollBar.Accessible.decreaseAction()
        compare(scrollBar.position, initialPosition)
        scrollBar.enabled = false
        scrollBar.Accessible.increaseAction()
        scrollBar.Accessible.decreaseAction()
        compare(scrollBar.position, initialPosition,
                "A disabled scroll bar must not move through assistive scroll actions")
        destroyObject(scrollBar)
    }

    function test_pagesKeepDisabledAssistiveActionsUnavailable() {
        var pageWindow = Qt.createQmlObject('import QtQuick.Window 2.15; Window { width: 1000; height: 800; visible: true }',
                                            testCase,
                                            "accessibility-disabled-actions-window")
        verify(pageWindow !== null)
        pageWindow.requestActivate()
        tryCompare(pageWindow, "active", true)

        var pcView = createReal("../../app/gui/adaptive/HostPageRoute.qml", "starlight.hosts.page",
                                { width: pageWindow.width, height: pageWindow.height }, pageWindow.contentItem)
        var settingsView = createReal("../../app/gui/adaptive/SettingsPageRoute.qml", "starlight.settings.page",
                                      { width: pageWindow.width, height: pageWindow.height }, pageWindow.contentItem)
        var backButton = findByObjectName(settingsView, "starlight.settings.back")
        var settingsButton = findByObjectName(pcView, "starlight.hosts.open-settings")
        var adjustButton = findByObjectName(pcView, "starlight.hosts.adjust-settings")
        var hostDisclosure = findByObjectName(pcView, "starlight.hosts.host-settings")

        verify(backButton !== null)
        verify(settingsButton !== null)
        verify(adjustButton !== null)
        verify(hostDisclosure !== null)

        windowAdapter.reset()
        backButton.enabled = false
        backButton.Accessible.pressAction()
        compare(windowAdapter.goBackCalls, 0,
                "A disabled Back button must not navigate through its assistive action")

        settingsButton.enabled = false
        settingsButton.Accessible.pressAction()
        compare(windowAdapter.navigateToCalls, 0,
                "A disabled Settings button must not navigate through its assistive action")

        hostDisclosure.expanded = false
        adjustButton.enabled = false
        adjustButton.Accessible.pressAction()
        adjustButton.Accessible.toggleAction()
        compare(hostDisclosure.expanded, false,
                "A disabled Adjust button must not change the disclosure through assistive actions")

        destroyObject(settingsView)
        destroyObject(pcView)
        pageWindow.close()
        pageWindow.destroy()
    }

    function test_hostPairingAndErrorExposeAccessibleRecovery() {
        var pageWindow = Qt.createQmlObject('import QtQuick.Window 2.15; Window { width: 1000; height: 800; visible: true }',
                                            testCase,
                                            "accessibility-host-recovery-window")
        verify(pageWindow !== null)
        pageWindow.requestActivate()
        tryCompare(pageWindow, "active", true)

        var pcView = createReal("../../app/gui/adaptive/HostPageRoute.qml", "starlight.hosts.page",
                                { width: pageWindow.width, height: pageWindow.height }, pageWindow.contentItem)
        var pairButton = findByObjectName(pcView, "starlight.hosts.add-host")
        var errorDialog = findByObjectName(pcView, "starlight.hosts.error-dialog")

        verify(pairButton !== null)
        verify(errorDialog !== null)
        compare(pairButton.Accessible.role, Accessible.Button)
        compare(pairButton.Accessible.focusable, true,
                "The visible pairing entry must be reachable by the same focus model as its assistive action")

        addPcDialogAdapter.reset()
        pairButton.forceActiveFocus()
        tryCompare(pairButton, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(addPcDialogAdapter.openCalls, 1,
                "Return on the focused pairing entry must use the ordinary pairing route")
        pairButton.Accessible.pressAction()
        compare(addPcDialogAdapter.openCalls, 2,
                "The pairing assistive action must use that same ordinary route")

        if (!hasPublicProperty(errorDialog, "accessibleItem")) {
            destroyObject(pcView)
            pageWindow.close()
            pageWindow.destroy()
            return
        }

        const errorMessage = "Couldn't reach the host at that address."
        pcView.context.presentError(errorMessage, "")
        tryCompare(errorDialog, "visible", true)
        verify(errorDialog.accessibleItem !== null)
        compare(errorDialog.accessibleItem.Accessible.role, Accessible.Dialog)
        verifyStableAccessibleId(errorDialog.accessibleItem,
                                 errorDialog.objectName)
        compare(errorDialog.accessibleItem.Accessible.name, "Error")
        verify(errorDialog.accessibleItem.Accessible.description.indexOf(errorMessage) !== -1,
               "The dynamic error message must be available as the dialog description")

        stackViewAdapter.reset()
        errorDialog.close()
        tryCompare(errorDialog, "visible", false)
        tryCompare(stackViewAdapter, "focusRequests", 1)

        destroyObject(pcView)
        pageWindow.close()
        pageWindow.destroy()
    }

    function test_pagesExposeNamedLandmarksAndSettingsLabelRelationships() {
        // keyClick targets the TestCase window, not the last window opened
        // by another test. Keep the real route and its keyboard events together.
        var harnessWindow = testCase.Window.window
        harnessWindow.requestActivate()
        tryCompare(harnessWindow, "active", true)
        var settingsStack = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15;'
                    + ' StackView { anchors.fill: parent }',
                    harnessWindow.contentItem, "accessibility-settings-stack")
        var hostComponent = realComponent("../../app/gui/adaptive/HostPageRoute.qml")
        var pcView = settingsStack.push(hostComponent, {
                                             objectName: "starlight.hosts.page",
                                             hostSource: Qt.createQmlObject('import Starlight.Test 1.0; TestHostSource {}', testCase,
                                                                            "accessibility-landmark-host-source") })
        verify(pcView !== null, hostComponent.errorString())
        var hostSurface = pcView.legacySurface
        verify(hostSurface !== null)
        var settingsComponent = realComponent("../../app/gui/adaptive/SettingsPageRoute.qml")
        var hostsList = findByObjectName(pcView, "starlight.hosts.list")

        compare(pcView.objectName, "starlight.hosts.page")
        compare(hostSurface.Accessible.role, Accessible.Pane)
        compare(hostSurface.Accessible.name, "Hosts")
        verify(hostsList !== null, "PcView must give its primary host list a stable objectName")
        compare(hostsList.Accessible.role, Accessible.List)
        compare(hostsList.Accessible.name, "Hosts")

        var settingsView = settingsStack.push(settingsComponent,
                                              { objectName: "starlight.settings.page" })
        verify(settingsView !== null, settingsComponent.errorString())
        tryCompare(settingsStack, "busy", false)
        var settingsSurface = settingsView.legacySurface
        verify(settingsSurface !== null)
        var backButton = findByObjectName(settingsView, "starlight.settings.back")
        var windowMode = findByObjectName(settingsView, "starlight.settings.window-mode")
        var language = findByObjectName(settingsView, "starlight.settings.language")
        var advanced = findByObjectName(settingsView, "starlight.settings.advanced")
        var keyboardCapture = findByObjectName(settingsView, "starlight.settings.capture-system-keys")

        compare(settingsView.objectName, "starlight.settings.page")
        compare(settingsSurface.Accessible.role, Accessible.Pane)
        compare(settingsSurface.Accessible.name, "Settings")
        verify(backButton !== null)
        verify(windowMode !== null)
        verify(language !== null)
        verify(advanced !== null)
        verify(keyboardCapture !== null)
        compare(advanced.expanded, false)
        compare(keyboardCapture.visible, language.visible,
                "Keyboard capture should be as discoverable as Language")

        verifyLabelRelationship(windowMode, "Streaming display mode")
        verifyLabelRelationship(language, "Language")

        backButton.forceActiveFocus()
        tryCompare(backButton, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(windowMode, "activeFocus", true)

        verify(advanced.accessibleItem !== null)
        compare(advanced.accessibleItem.Accessible.name, "Advanced")
        settingsStack.destroy()
        wait(0)
        testWindow.requestActivate()
        tryCompare(testWindow, "active", true)
    }

    function test_controlActivationSupportsPreQt68AbstractButtons() {
        var modern = Qt.createQmlObject(
                    'import QtQml 2.15\n'
                    + 'QtObject {\n'
                    + '    property bool enabled: true\n'
                    + '    property bool checkable: false\n'
                    + '    property int clickCalls: 0\n'
                    + '    property int clickedCalls: 0\n'
                    + '    function click() { clickCalls += 1 }\n'
                    + '    function clicked() { clickedCalls += 1 }\n'
                    + '}',
                    testCase,
                    "modern-abstract-button.qml")
        verify(modern !== null)
        ControlCompat.activate(modern)
        compare(modern.clickCalls, 1)
        compare(modern.clickedCalls, 0)

        var legacy = Qt.createQmlObject(
                    'import QtQml 2.15\n'
                    + 'QtObject {\n'
                    + '    property bool enabled: true\n'
                    + '    property bool checkable: true\n'
                    + '    property int toggleCalls: 0\n'
                    + '    property int clickedCalls: 0\n'
                    + '    function toggle() { toggleCalls += 1 }\n'
                    + '    function clicked() { clickedCalls += 1 }\n'
                    + '}',
                    testCase,
                    "legacy-abstract-button.qml")
        verify(legacy !== null)
        ControlCompat.activate(legacy)
        compare(legacy.toggleCalls, 1)
        compare(legacy.clickedCalls, 1)

        legacy.enabled = false
        ControlCompat.activate(legacy)
        compare(legacy.toggleCalls, 1)
        compare(legacy.clickedCalls, 1)

        destroyObject(legacy)
        destroyObject(modern)
    }
}
