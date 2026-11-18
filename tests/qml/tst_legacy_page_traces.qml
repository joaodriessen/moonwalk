import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3
import StreamingPreferences 1.0
import Starlight.Test 1.0

TestCase {
    id: testCase
    name: "LegacyPageTraces"
    when: windowShown

    property var testWindow: null
    property var page: null
    property var hostSource: null
    property var hostRouteStack: null
    property var stackView: stackViewAdapter

    QtObject {
        id: stackViewAdapter
        function forceActiveFocus() {
        }
    }

    Component {
        id: replacementPage
        Item {
        }
    }

    function initTestCase() {
        testWindow = Qt.createQmlObject(
                    'import QtQuick.Window 2.15; Window { width: 1000; height: 800; visible: true }',
                    testCase,
                    "legacy-page-trace-window")
        verify(testWindow !== null)
        testWindow.requestActivate()
        tryCompare(testWindow, "active", true)

        const component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/adaptive/HostPageRoute.qml"))
        verify(component.status === Component.Ready, component.errorString())
        hostSource = Qt.createQmlObject(
                    'import Starlight.Test 1.0; TestHostSource {}',
                    testCase, "legacy-trace-host-source")
        hostRouteStack = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15;'
                    + ' StackView { width: 1000; height: 800 }',
                    testWindow.contentItem, "legacy-trace-host-route-stack")
        page = hostRouteStack.push(component, { hostSource: hostSource })
        verify(page !== null)
    }

    function cleanupTestCase() {
        if (testWindow) {
            hostRouteStack.destroy()
            hostRouteStack = null
            testWindow.close()
            testWindow.destroy()
            testWindow = null
        }
    }

    function preparePairingHost() {
        hostSource.replaceHosts([{
            name: "Trace host",
            uuid: "trace-host",
            online: true,
            paired: false,
            busy: false,
            wakeable: false,
            statusUnknown: false,
            serverSupported: true,
            address: "192.0.2.1",
            details: "",
            serverSoftware: ""
        }])
        page.testSelectHostIndex(0)
        wait(0)
    }

    function test_pair_cancel_late_complete_repair_trace() {
        preparePairingHost()
        page.beginPairing()
        const firstAttempt = page.testPairAttemptId
        verify(firstAttempt >= 0)
        compare(page.testOverlayKind, "pairing")

        page.cancelPairing(firstAttempt)
        page.testCompletePair(firstAttempt, true)
        compare(page.testOverlayKind, "none")

        page.beginPairing()
        verify(page.testPairAttemptId !== firstAttempt)
        compare(page.testOverlayKind, "pairing")
    }

    function test_settings_saves_on_deactivation_and_destruction() {
        StreamingPreferences.reset()
        const stack = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15;'
                    + ' StackView { width: 1000; height: 800 }',
                    testWindow.contentItem,
                    "legacy-settings-lifecycle-stack")
        verify(stack !== null)

        const component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/adaptive/SettingsPageRoute.qml"))
        verify(component.status === Component.Ready, component.errorString())
        const settings = stack.push(component)
        verify(settings !== null, component.errorString())
        stack.push(replacementPage)
        tryCompare(StreamingPreferences, "saveCalls", 1)

        stack.clear()
        tryCompare(StreamingPreferences, "saveCalls", 2)
        stack.destroy()
    }
}
