import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtTest 1.3

import Starlight 1.0
import "../../app/gui/starlight" as AppStyle
import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "StyleBootstrap"
    when: windowShown

    // main.qml receives these context properties from main.cpp.
    property url initialView: Qt.resolvedUrl("fixtures/EmptyInitialView.qml")
    property bool runConfigChecks: false
    property bool adaptiveShellRequested: false

    // The segue roots receive these context properties from main.qml.
    property var stackView: stackViewAdapter
    property var window: windowAdapter

    property var launcherComponent: null
    property var streamComponent: null
    property var quitComponent: null
    property var messageComponent: null
    property var busyComponent: null

    SystemPalette { id: nativePalette; colorGroup: SystemPalette.Active }

    QtObject {
        id: stackViewAdapter
        property int depth: 1
        property var currentItem: null

        function forceActiveFocus() {
        }

        function pop() {
        }

        function replace() {
        }
    }

    QtObject {
        id: windowAdapter
        property bool visible: true

        function goBack() {
        }

        function showToast() {
        }
    }

    Component {
        id: selectedStyleBusyComponent

        BusyIndicator {
            running: false
        }
    }

    function componentFor(relativePath) {
        var component = Qt.createComponent(Qt.resolvedUrl(relativePath))
        verify(component.status === Component.Ready, component.errorString())
        return component
    }

    function findDescendant(object, predicate) {
        if (!object) {
            return null
        }
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

    function findBusyIndicator(object) {
        return findDescendant(object, function(candidate) {
            return candidate
                    && candidate.contentItem !== undefined
                    && candidate.running !== undefined
                    && candidate.padding !== undefined
        })
    }

    function create(component, properties, parentObject) {
        var object = component.createObject(parentObject || testCase, properties || {})
        verify(object !== null, component.errorString())
        wait(0)
        return object
    }

    function destroyObject(object) {
        if (object) {
            object.destroy()
        }
    }

    function initTestCase() {
        launcherComponent = componentFor("../../app/gui/main.qml")
        streamComponent = componentFor("../../app/gui/StreamSegue.qml")
        quitComponent = componentFor("../../app/gui/QuitSegue.qml")
        messageComponent = componentFor("../../app/gui/NavigableMessageDialog.qml")
        busyComponent = componentFor("../../app/gui/starlight/SLBusy.qml")
    }

    function test_allFiveStyleRootsLoadWithTheSelectedControlsStyle() {
        var launcher = create(launcherComponent, {}, null)
        var stream = create(streamComponent, { appName: "Test stream" })
        var quit = create(quitComponent, { appName: "Test stream" })
        var message = create(messageComponent, {
                                 title: "Connection failed",
                                 text: "Unable to reach the host"
                             })
        var busy = create(busyComponent)

        verify(launcher !== null)
        verify(stream !== null)
        verify(quit !== null)
        verify(message !== null)
        verify(busy !== null)

        destroyObject(busy)
        destroyObject(message)
        destroyObject(quit)
        destroyObject(stream)
        launcher.close()
        destroyObject(launcher)
    }

    function test_stream_stage_errors_offer_specific_recovery_and_keep_diagnostics() {
        var stream = create(streamComponent, { appName: "Desktop" })
        var nameFailure = stream.formatStageFailure("name resolution", 5, "")
        verify(nameFailure.indexOf("address") !== -1)
        verify(nameFailure.indexOf("Technical details") === -1)
        verify(stream.technicalFailure.indexOf("5") !== -1)
        verify(!stream.showTechnicalFailure)
        var videoFailure = stream.formatStageFailure("video stream establishment", -42, "")
        verify(videoFailure.toLowerCase().indexOf("video") !== -1)
        verify(stream.technicalFailure.indexOf("-42") !== -1)
        var networkFailure = stream.formatStageFailure("RTSP handshake", -1, "47984")
        verify(networkFailure.indexOf("47984") !== -1)
        verify(networkFailure.indexOf("port forwarding") === -1,
               "LAN failures should not imply Internet port forwarding")
        destroyObject(stream)
    }

    function test_busyIndicatorsUseTheSelectedStyleAndSemanticAccent() {
        var selectedStyleBusy = selectedStyleBusyComponent.createObject(testCase)
        verify(selectedStyleBusy !== null)
        wait(0)

        var semanticBusy = create(busyComponent)
        compare(semanticBusy.padding, selectedStyleBusy.padding)
        compare(semanticBusy.contentItem.implicitWidth,
                selectedStyleBusy.contentItem.implicitWidth,
                "SLBusy must not instantiate a different explicitly imported style")
        compare(semanticBusy.contentItem.implicitHeight,
                selectedStyleBusy.contentItem.implicitHeight,
                "SLBusy must retain the selected style's indicator geometry")
        compare(semanticBusy.contentItem.color, AppStyle.Theme.accent,
                "The semantic accent token must tint the selected style implementation")

        var roots = [
                    create(streamComponent, { appName: "Test stream" }),
                    create(quitComponent, { appName: "Test stream" }),
                    create(messageComponent, {
                               title: "Connection failed",
                               text: "Unable to reach the host"
                           })
                ]
        roots[2].open()
        wait(0)
        for (var i = 0; i < roots.length; ++i) {
            var indicator = findBusyIndicator(roots[i].accessibleItem || roots[i])
            verify(indicator !== null, "Expected a real BusyIndicator in style root " + i)
            verify(indicator.contentItem !== null,
                   "Every busy state must come from the one selected Controls style")
        }

        for (var j = 0; j < roots.length; ++j) {
            destroyObject(roots[j])
        }
        destroyObject(semanticBusy)
        destroyObject(selectedStyleBusy)
    }

    function test_launcherPaletteAndPopupScaffoldingUseSemanticTokens() {
        var launcher = create(launcherComponent, {}, null)
        launcher.show()
        wait(0)
        verify(launcher.sharedToolTip !== undefined,
               "main.qml must expose its real attached ToolTip scaffold for deterministic validation")
        var toolTip = launcher.sharedToolTip
        verify(toolTip !== null, "The real attached ToolTip singleton must remain loadable")
        toolTip.parent = launcher.contentItem
        compare(toolTip.parent, launcher.contentItem)
        toolTip.text = "Style baseline"
        toolTip.visible = true
        wait(0)
        compare(toolTip.contentWidth <= 400, true)
        compare(toolTip.background.radius, AppStyle.Theme.radiusChip)
        compare(toolTip.background.color, AppStyle.Theme.darkChip)
        compare(toolTip.contentItem.color, AppStyle.Theme.accentInk)

        var message = create(messageComponent, {
                                 title: "Connection failed",
                                 text: "Unable to reach the host",
                                 standardButtons: Dialog.Ok
                             })
        message.open()
        wait(0)
        compare(message.modal, true)
        compare(message.padding, 22)
        compare(message.topPadding, 22)
        compare(message.bottomPadding, 4)
        compare(message.background.radius, AppStyle.Theme.radiusCard)
        compare(message.background.color, AppStyle.Theme.cardSolid)
        verify(message.footer !== null)
        compare(message.footer.padding, 14)
        compare(message.footer.spacing, 8)

        var messageBusy = findBusyIndicator(message.accessibleItem || message)
        verify(messageBusy !== null)
        console.info("style baseline:"
                     + " busy=" + messageBusy.contentItem.implicitWidth
                     + "x" + messageBusy.contentItem.implicitHeight
                     + " tooltip=" + toolTip.padding + "/" + toolTip.horizontalPadding
                     + "/" + toolTip.background.radius
                     + " dialog=" + message.padding + "/" + message.topPadding
                     + "/" + message.bottomPadding + "/" + message.background.radius)
        console.info("palette:"
                     + " window=" + launcher.palette.window + "/" + AppStyle.Theme.ground
                     + " windowText=" + launcher.palette.windowText + "/" + AppStyle.Theme.label
                     + " text=" + launcher.palette.text + "/" + AppStyle.Theme.label
                     + " highlight=" + launcher.palette.highlight + "/" + AppStyle.Theme.accent
                     + " highlightedText=" + launcher.palette.highlightedText + "/" + AppStyle.Theme.accentInk)

        compare(launcher.palette.window, AppStyle.Theme.ground)
        compare(launcher.palette.windowText, AppStyle.Theme.label)
        compare(launcher.palette.text, AppStyle.Theme.label)
        compare(launcher.palette.highlight, AppStyle.Theme.accent)
        compare(launcher.palette.highlightedText, AppStyle.Theme.accentInk)

        destroyObject(message)
        launcher.close()
        destroyObject(launcher)
    }

    function test_adaptivePlasmaLauncherInheritsNativeApplicationPalette() {
        if (!plasmaDesktopAdapterActive)
            skip("requires the Plasma adapter fixture")
        var originalAdaptive = adaptiveShellRequested
        adaptiveShellRequested = true
        var launcher = create(launcherComponent, {}, null)
        launcher.show()
        wait(0)

        compare(launcher.palette.window, nativePalette.window)
        compare(launcher.palette.windowText, nativePalette.windowText)
        compare(launcher.palette.text, nativePalette.text)
        compare(launcher.palette.highlight, nativePalette.highlight)
        compare(launcher.palette.highlightedText,
                nativePalette.highlightedText)

        launcher.close()
        destroyObject(launcher)
        adaptiveShellRequested = originalAdaptive
    }

    function test_materialWindowPreservesInheritedLegacySurfaceContract() {
        if (!materialStyleActive) {
            skip("Material inheritance is only applicable to the selected Material style")
        }

        var launcher = create(launcherComponent, {}, null)
        launcher.show()
        var message = create(messageComponent, {
                                 title: "Connection failed",
                                 text: "Unable to reach the host"
                             }, launcher.contentItem)
        message.open()
        wait(0)

        verify(message.header !== null,
               "The real inherited Dialog header must exist")
        compare(launcher.Material.theme,
                AppStyle.Theme.isDark ? Material.Dark : Material.Light)
        compare(launcher.Material.accent, AppStyle.Theme.accent)
        compare(launcher.Material.primary, "#3f51b5")
        compare(message.header.Material.theme, launcher.Material.theme)
        compare(message.header.Material.accent, launcher.Material.accent)
        compare(message.header.Material.primary, launcher.Material.primary)
        compare(message.header.color, message.header.Material.foreground,
                "The actual inherited title must render with Material foreground")

        destroyObject(message)
        launcher.close()
        destroyObject(launcher)
    }
}
