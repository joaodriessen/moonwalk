import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtTest 1.3

TestCase {
    id: testCase
    name: "HostAddressDialogs"
    when: windowShown
    property var page: null
    property var hostWindow: null
    property var dialogLayer: null
    property string platform: ""
    Component {
        id: windowComponent
        ApplicationWindow { width: 640; height: 600; visible: true }
    }
    function cleanup() {
        if (dialogLayer) dialogLayer.destroy()
        if (hostWindow) { hostWindow.close(); hostWindow.destroy() }
        if (page) page.destroy()
        wait(0)
        dialogLayer = null
        hostWindow = null
        page = null
    }
    function create(platformName) {
        if (platformName !== "gnome" && !kirigamiAvailable)
            skip("Kirigami has no Qt 6 package on the compatibility-floor runner")
        platform = platformName
        var facade = Qt.createComponent(Qt.resolvedUrl("fixtures/GnomeHostFacade.qml"))
        verify(facade.status === Component.Ready, facade.errorString())
        page = facade.createObject(testCase)
        hostWindow = windowComponent.createObject(testCase)
        var filename = platform === "gnome" ? "gnome/GnomeHostDialogLayer.qml" : "plasma/PlasmaHostDialogLayer.qml"
        var component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/adaptive/" + filename))
        verify(component.status === Component.Ready, component.errorString())
        dialogLayer = component.createObject(hostWindow.contentItem, { page: page })
        verify(dialogLayer !== null, component.errorString())
        details(41, "192.0.2.10:47989")
    }
    function details(operationId, address) {
        page.baseOverlay = ({ kind: "details", operationId: operationId,
            hostUuid: "host-a", modelGeneration: 0,
            payload: { details: Array(70).join("Computer information\n"), connectionAddress: address } })
        wait(0)
    }
    function item(suffix) {
        var name = suffix === "connection-address" ? "starlight.hosts.connection-address"
                                                  : platform + "-host-dialog-" + suffix
        var control = findChild(dialogLayer, name)
        verify(control !== null, name)
        return control
    }
    function platforms() { return [{ tag: "GNOME", platform: "gnome" }, { tag: "Plasma", platform: "plasma" }] }
    function test_draftSurvivesRefreshAndInterrupt_data() { return platforms() }
    function test_draftSurvivesRefreshAndInterrupt(data) {
        create(data.platform)
        var field = item("connection-address")
        compare(field.text, "192.0.2.10:47989")
        field.text = "my-host.example:48000"
        details(41, "198.51.100.20")
        compare(field.text, "my-host.example:48000")
        page.interruptOverlay = ({ kind: "error", operationId: 99, payload: { message: "Connection interrupted" } })
        wait(0)
        dialogLayer.rejectCurrent()
        tryCompare(dialogLayer, "visibleOverlayKind", "details")
        compare(field.text, "my-host.example:48000")
        compare(page.addressSubmissions.length, 0)
        item("save-address").clicked()
        compare(page.addressSubmissions.length, 1)
        compare(page.addressSubmissions[0].value, "my-host.example:48000")
        compare(page.addressSubmissions[0].hostUuid, "host-a")
        compare(page.baseOverlay.kind, "details")
        dialogLayer.rejectCurrent()
        compare(page.addressSubmissions.length, 1)
    }
    function test_automaticIsExplicitAndNewOperationResetsDraft_data() { return platforms() }
    function test_automaticIsExplicitAndNewOperationResetsDraft(data) {
        create(data.platform)
        item("connection-address").text = "unsaved.example"
        item("automatic-address").clicked()
        compare(page.addressSubmissions.length, 1)
        compare(page.addressSubmissions[0].value, "")
        compare(item("connection-address").text, "")
        compare(page.baseOverlay.kind, "details")
        details(42, "next.example")
        compare(item("connection-address").text, "next.example")
        item("connection-address").text = "discard.example"
        dialogLayer.rejectCurrent()
        compare(page.addressSubmissions.length, 1)
    }
    function test_rejectedOrOtherHostSaveKeepsDraft_data() { return platforms() }
    function test_rejectedOrOtherHostSaveKeepsDraft(data) {
        create(data.platform)
        item("connection-address").text = "draft.example"
        page.connectionAddressAccepted = false
        item("save-address").clicked()
        compare(page.addressSubmissions.length, 0)
        compare(item("connection-address").text, "draft.example")
        verify(item("address-status").text.length > 0)
        page.connectionAddressAccepted = true
        page.selectedHostUuid = "host-b"
        item("save-address").clicked()
        compare(page.addressSubmissions.length, 0)
        compare(item("connection-address").text, "draft.example")
    }
    function test_detailsScrollLeavesEditorAndFooterVisible_data() { return platforms() }
    function test_detailsScrollLeavesEditorAndFooterVisible(data) {
        create(data.platform)
        hostWindow.width = 440
        wait(0)
        var field = item("connection-address")
        var save = item("save-address")
        var close = item("reject")
        verify(field.visible && save.visible && close.visible)
        verify(item("details-scroll").height < item("details-scroll").contentHeight,
               "Long details must scroll while the address editor stays visible")
        for (var control of [field, save, close]) {
            var bounds = control.mapToItem(hostWindow.contentItem, 0, 0)
            verify(bounds.y >= 0)
            verify(bounds.y + control.height <= hostWindow.height)
        }
        compare(page.addressSubmissions.length, 0)
    }
}
