import QtQuick 2.15
import QtTest 1.3
import StreamingPreferences 1.0
import Starlight.Test 1.0
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PairingCharacterization"
    when: windowShown

    property var pcView: null
    property var stackView: stackViewAdapter
    property var window: windowAdapter
    property var testWindow: null
    property var routeStack: null
    property var hostSource: null
    property int pairingDispatchBefore: 0

    QtObject {
        id: stackViewAdapter
        function forceActiveFocus() {
        }
    }

    QtObject {
        id: windowAdapter
        property int showToastCalls: 0
        property string lastToast: ""

        function reset() {
            showToastCalls = 0
            lastToast = ""
        }

        function showToast(message) {
            showToastCalls += 1
            lastToast = message
        }
    }

    function initTestCase() {
        testWindow = Qt.createQmlObject(
                    'import QtQuick.Window 2.15; Window {'
                    + ' width: 1000; height: 800; visible: true }',
                    testCase,
                    "pairing-characterization-window")
        verify(testWindow !== null)
        testWindow.requestActivate()
        tryCompare(testWindow, "active", true)
        waitForRendering(testWindow.contentItem)

        var component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/adaptive/HostPageRoute.qml"))
        verify(component.status === Component.Ready, component.errorString())
        routeStack = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15;'
                    + ' StackView { width: 1000; height: 800 }',
                    testWindow.contentItem, "pairing-route-stack")
        hostSource = Qt.createQmlObject('import Starlight.Test 1.0; TestHostSource {}',
                                        testCase, "pairing-host-source")
        pcView = routeStack.push(component, { hostSource: hostSource })
        verify(pcView !== null, component.errorString())
        wait(0)
    }

    function cleanupTestCase() {
        if (pcView) {
            routeStack.destroy()
            pcView = null
        }
        if (testWindow) {
            testWindow.close()
            testWindow.destroy()
            testWindow = null
        }
    }

    function findDescendant(object, predicate) {
        if (predicate(object)) {
            return object
        }

        var children = object.children || []
        for (var i = 0; i < children.length; ++i) {
            var match = findDescendant(children[i], predicate)
            if (match) {
                return match
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

    function preparePairingAttempt() {
        pairingDispatchBefore = hostSource.pairDispatchCount
        windowAdapter.reset()
        hostSource.replaceHosts([{
            name: "Test host",
            uuid: "test-host",
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
        wait(0)

        var sidebarList = findDescendant(pcView, function(object) {
            return object && object.currentIndex !== undefined && object.model === pcView.page.hosts
        })
        verify(sidebarList !== null, "The real PcView sidebar ListView was not created")
        compare(hostSource.rowCount(), 1)
        sidebarList.currentIndex = 0
        wait(0)
        compare(sidebarList.currentIndex, 0)
        pcView.page.selectHostByUuid("test-host")
    }

    function test_staleResultIsConsumedWithoutAffectingNewerAttempt() {
        var pairSheet = findDescendant(pcView, function(object) {
            return object && object.pin !== undefined && object.hostName !== undefined
        })
        verify(pairSheet !== null, "The real PcView pairing sheet was not created")
        preparePairingAttempt()

        // Exercise the real beginPairing() path and its ComputerModel signal
        // connection. The adapter only records the C++ boundary; PcView owns
        // the attempt IDs, queue, cancellation, and stale-result behavior.
        pcView.beginPairing()
        compare(hostSource.pairDispatchCount, pairingDispatchBefore + 1)
        compare(windowAdapter.showToastCalls, 0)
        verify(pcView.testPairAttemptId >= 0)
        tryCompare(pairSheet, "visible", true)

        pairSheet.reject()
        tryCompare(pairSheet, "visible", false)

        pcView.beginPairing()
        compare(hostSource.pairDispatchCount, pairingDispatchBefore + 2)
        verify(pcView.testPairAttemptId >= 0)
        tryCompare(pairSheet, "visible", true)

        hostSource.completePairing("late failure")

        wait(150)
        compare(pairSheet.visible, true)

        hostSource.completePairing()
        tryCompare(pairSheet, "visible", false)
    }

    function test_cancelMarksOnlyTheCurrentAttemptForSuppression() {
        var pairSheet = findDescendant(pcView, function(object) {
            return object && object.pin !== undefined && object.hostName !== undefined
        })
        var errorDialog = findDescendant(pcView, function(object) {
            return object && object.helpText !== undefined && object.iconName !== undefined
        })
        verify(pairSheet !== null, "The real PcView pairing sheet was not created")
        verify(errorDialog !== null, "The real PcView error dialog was not created")

        preparePairingAttempt()
        pcView.beginPairing()
        compare(hostSource.pairDispatchCount, pairingDispatchBefore + 1)
        tryCompare(pairSheet, "visible", true)
        pairSheet.reject()

        tryCompare(pairSheet, "visible", false)

        hostSource.completePairing("late failure")

        compare(errorDialog.visible, false)
    }

    function legacyItem(name) {
        var matches = TestCompat.walkQObjectGraph(pcView).filter(function(object) { return object.objectName === name })
        compare(matches.length, 1, name)
        return matches[0]
    }

    function test_legacyQualityModeReplacesSeparateAutomaticControl() {
        preparePairingAttempt()
        StreamingPreferences.reset()
        legacyItem("starlight.hosts.host-settings").expanded = true
        var quality = legacyItem("starlight.hosts.quality-mode")
        var bitrate = legacyItem("starlight.hosts.maximum-bitrate")
        compare(quality.model[quality.currentIndex].value, "recommended")
        compare(bitrate.visible, false)
        verify(!findChild(pcView, "starlight.hosts.automatic-bitrate"))
        StreamingPreferences.saveCalls = 0
        quality.activated(quality.model.length - 1)
        compare(StreamingPreferences.autoAdjustBitrate, false)
        compare(StreamingPreferences.adaptiveBitrate, false)
        compare(StreamingPreferences.saveCalls, 1)
        tryCompare(bitrate, "visible", true)
        quality.activated(0)
        compare(StreamingPreferences.autoAdjustBitrate, true)
        compare(StreamingPreferences.adaptiveBitrate, false)
        compare(StreamingPreferences.saveCalls, 2)
        tryCompare(bitrate, "visible", false)
    }

    function test_legacyDetailsAddressOnlyWritesOnExplicitSave() {
        preparePairingAttempt()
        StreamingPreferences.reset()
        pcView.page.requestDetails("test-host")
        var dialog = legacyItem("starlight.host-details-dialog")
        tryCompare(dialog, "visible", true)
        var field = legacyItem("starlight.hosts.connection-address")
        field.text = "draft.example:48000"
        compare(StreamingPreferences.connectionAddress, "")
        compare(StreamingPreferences.saveCalls, 0)
        legacyItem("legacy-host-details-save-address").clicked()
        compare(StreamingPreferences.connectionAddress, "draft.example:48000")
        compare(StreamingPreferences.saveCalls, 1)
        compare(dialog.visible, true)
        field.text = "discard.example"
        dialog.close()
        tryCompare(dialog, "visible", false)
        compare(StreamingPreferences.connectionAddress, "draft.example:48000")
        compare(StreamingPreferences.saveCalls, 1)
        pcView.page.requestDetails("test-host")
        tryCompare(dialog, "visible", true)
        compare(field.text, "draft.example:48000")
        legacyItem("legacy-host-details-automatic-address").clicked()
        compare(StreamingPreferences.connectionAddress, "")
        compare(StreamingPreferences.saveCalls, 2)
        dialog.close()
    }

    function test_resolutionChangeUsesTheAutomaticBitratePathAndPersists() {
        preparePairingAttempt()
        StreamingPreferences.reset()
        StreamingPreferences.enableYUV444 = true
        StreamingPreferences.stubDefaultBitrate = 23456

        // A host without a stored display mode has its frame rate seeded
        // from the native refresh rate (60 here) on selection, so that is
        // the rate the automatic bitrate path must receive.
        compare(StreamingPreferences.fps, 60)

        pcView.page.setResolution(1600, 900)

        compare(StreamingPreferences.width, 1600)
        compare(StreamingPreferences.height, 900)
        compare(StreamingPreferences.defaultBitrateCalls, 1)
        compare(StreamingPreferences.lastBitrateWidth, 1600)
        compare(StreamingPreferences.lastBitrateHeight, 900)
        compare(StreamingPreferences.lastBitrateFps, 60)
        compare(StreamingPreferences.lastBitrateYuv444, true)
        compare(StreamingPreferences.bitrateKbps, 23456)
        compare(StreamingPreferences.saveCalls, 1)
    }
}
