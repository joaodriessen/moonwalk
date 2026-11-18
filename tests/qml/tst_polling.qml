import QtQuick 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PollingCharacterization"
    when: windowShown

    // main.qml receives these as QQmlContext properties in production.
    property url initialView: Qt.resolvedUrl("fixtures/EmptyInitialView.qml")
    property bool runConfigChecks: false
    property bool adaptiveShellRequested: false
    property var launcherWindow: null
    property var launcherComponent: null

    function initTestCase() {
        launcherComponent = Qt.createComponent(Qt.resolvedUrl("../../app/gui/main.qml"))
        verify(launcherComponent.status === Component.Ready, launcherComponent.errorString())
    }

    function createLauncher() {
        ComputerManager.reset()
        SdlGamepadKeyNavigation.reset()
        launcherWindow = launcherComponent.createObject(null)
        verify(launcherWindow !== null, launcherComponent.errorString())
        wait(0)
        launcherWindow.requestActivate()
        tryCompare(launcherWindow, "active", true)
    }

    function cleanup() {
        if (launcherWindow) {
            launcherWindow.close()
            launcherWindow.destroy()
            launcherWindow = null
        }
    }

    function findDescendant(object, predicate) {
        if (predicate(object)) {
            return object
        }

        var children = object.children || []
        for (var i = 0; i < children.length; ++i) {
            var childMatch = findDescendant(children[i], predicate)
            if (childMatch) {
                return childMatch
            }
        }

        var data = object.data || []
        for (var j = 0; j < data.length; ++j) {
            var dataObject = data[j]
            if (!TestCompat.containsIdentity(children, dataObject)) {
                var dataMatch = findDescendant(dataObject, predicate)
                if (dataMatch) {
                    return dataMatch
                }
            }
        }

        return null
    }

    function test_visibleActiveWindowStartsPollingAndEnablesFocusedGamepadNavigation() {
        createLauncher()

        verify(launcherWindow.visible)
        verify(launcherWindow.active)
        compare(launcherWindow.pollingActive, true)
        compare(ComputerManager.startPollingCalls, 1)
        compare(SdlGamepadKeyNavigation.enableCalls, 1)
        compare(SdlGamepadKeyNavigation.focusNotifications[
                    SdlGamepadKeyNavigation.focusNotifications.length - 1], true)
    }

    function test_hidingStopsPollingImmediatelyAndShowingRestoresItWhenActive() {
        createLauncher()

        launcherWindow.visible = false
        wait(0)

        compare(launcherWindow.pollingActive, false)
        compare(ComputerManager.stopPollingCalls, 1)
        compare(SdlGamepadKeyNavigation.focusNotifications[
                    SdlGamepadKeyNavigation.focusNotifications.length - 1], false)

        launcherWindow.visible = true
        launcherWindow.requestActivate()
        tryCompare(launcherWindow, "active", true)
        tryCompare(launcherWindow, "pollingActive", true)
        compare(ComputerManager.startPollingCalls, 2)
        tryVerify(function() {
            return SdlGamepadKeyNavigation.focusNotifications[
                        SdlGamepadKeyNavigation.focusNotifications.length - 1]
                    === true
        })
    }

    function test_backendDispatchPrecedesPollingStateCommit() {
        createLauncher()
        ComputerManager.pollingStateSource = launcherWindow

        launcherWindow.visible = false
        wait(0)

        compare(ComputerManager.stopPollingStatesAtDispatch.length, 1)
        compare(ComputerManager.stopPollingStatesAtDispatch[0], true)
        compare(launcherWindow.pollingActive, false)

        launcherWindow.visible = true
        launcherWindow.requestActivate()
        tryCompare(launcherWindow, "active", true)
        tryCompare(launcherWindow, "pollingActive", true)

        compare(ComputerManager.startPollingStatesAtDispatch.length, 2)
        compare(ComputerManager.startPollingStatesAtDispatch[1], false)
        compare(launcherWindow.pollingActive, true)
    }

    function test_losingFocusStartsTheInactivityGraceTimerWithoutStoppingPolling() {
        createLauncher()

        var inactivityTimer = findDescendant(launcherWindow, function(object) {
            return object && object.interval === 5 * 60000 && object.running !== undefined
        })
        verify(inactivityTimer !== null, "The real main.qml inactivity timer was not created")

        var otherWindow = Qt.createQmlObject('import QtQuick.Window 2.15; Window { visible: true }',
                                             testCase,
                                             "focus-stealer")
        verify(otherWindow !== null)
        otherWindow.requestActivate()
        tryCompare(launcherWindow, "active", false)

        compare(inactivityTimer.running, true)
        compare(launcherWindow.pollingActive, true)
        compare(ComputerManager.stopPollingCalls, 0)
        compare(SdlGamepadKeyNavigation.focusNotifications[
                    SdlGamepadKeyNavigation.focusNotifications.length - 1], false)

        otherWindow.close()
        otherWindow.destroy()
    }

    function test_inactivityTimeoutStopsPollingAndFocusRestorationRestartsIt() {
        createLauncher()

        var inactivityTimer = findDescendant(launcherWindow, function(object) {
            return object && object.interval === 5 * 60000 && object.running !== undefined
        })
        verify(inactivityTimer !== null, "The real main.qml inactivity timer was not created")

        var otherWindow = Qt.createQmlObject('import QtQuick.Window 2.15; Window { visible: true }',
                                             testCase,
                                             "focus-stealer")
        verify(otherWindow !== null)
        otherWindow.requestActivate()
        tryCompare(launcherWindow, "active", false)
        compare(inactivityTimer.running, true)

        // Keep the real Timer and onTriggered handler, but shorten this test
        // instance's grace period instead of waiting five minutes.
        inactivityTimer.interval = 1
        inactivityTimer.restart()
        tryCompare(launcherWindow, "pollingActive", false)

        compare(ComputerManager.stopPollingCalls, 1)
        otherWindow.close()
        otherWindow.destroy()

        launcherWindow.requestActivate()
        tryCompare(launcherWindow, "active", true)
        compare(inactivityTimer.running, false)
        compare(launcherWindow.pollingActive, true)
        compare(ComputerManager.startPollingCalls, 2)
        compare(SdlGamepadKeyNavigation.focusNotifications[
                    SdlGamepadKeyNavigation.focusNotifications.length - 1], true)
    }
}
