import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

TestCase {
    id: testCase
    name: "FileTransferPane"
    when: windowShown
    visible: true
    width: 640
    height: 900
    property var pane
    property var transfer
    Component {
        id: fakeTransfer
        QtObject {
            property bool supported: true
            property bool busy: false
            property bool queueActive: false
            property string collisionName: ""
            property url completedLocalFile: ""
            property var queueEntries: []
            property var remoteEntries: []
            property string error: ""
            property double progress: 0
            property double total: 0
            property var copies: []
            property string decision: ""
            property int cancellations: 0
            property int reveals: 0
            signal pickerRequested(string action, string folder, string relativeFile)
            function listRemote(folder, directory) {}
            function sendFiles(urls, folder, directory) { copies = copies.concat([{urls: urls, folder: folder, directory: directory}]) }
            function localFileUrl(path) { return path }
            function resolveCollision(action) { decision = action }
            function cancel() { ++cancellations }
            function revealReceivedFile() { ++reveals }
        }
    }
    function init() {
        transfer = fakeTransfer.createObject(testCase)
        var component = Qt.createComponent(Qt.resolvedUrl("../../app/gui/FileTransferPane.qml"))
        verify(component.status === Component.Ready, component.errorString())
        pane = component.createObject(testCase, {transfer: transfer, active: true, width: 600})
        verify(pane !== null, component.errorString())
    }
    function cleanup() { pane.destroy(); transfer.destroy(); wait(0) }
    function control(name) { var result = findChild(pane, "starlight.files." + name); verify(result !== null); return result }
    function test_nativePickerSelectsMultipleAndRejectsClosedOwnerResults() {
        var dialog = pane.createPicker(false, "")
        verify(dialog !== null)
        // Qt FileDialog.OpenFiles is the native multi-selection mode.
        compare(dialog.fileMode, 1)
        dialog.pickedFiles(["file:///private/one.txt", "file:///private/two.txt"])
        compare(transfer.copies.length, 1)
        compare(transfer.copies[0].urls.length, 2)
        compare(transfer.copies[0].folder, "Downloads")
        pane.active = false
        dialog.pickedFiles(["file:///private/stale.txt"])
        compare(transfer.copies.length, 1)
    }
    function test_dropCopiesToNamedDestinationAndQueueDecisionsRemainCancellable() {
        var folder = control("remote-folder")
        folder.currentIndex = 1
        pane.remoteDirectory = "work"
        pane.sendDroppedFiles(["file:///private/one", "file:///private/two"])
        compare(transfer.copies.length, 1)
        compare(transfer.copies[0].folder, "Documents")
        compare(transfer.copies[0].directory, "work")
        transfer.queueActive = true
        transfer.collisionName = "one"
        verify(control("cancel").visible)
        verify(!control("send").enabled)
        control("keep-both").clicked()
        compare(transfer.decision, "keepBoth")
        control("skip").clicked()
        compare(transfer.decision, "skip")
        control("cancel").clicked()
        compare(transfer.cancellations, 1)
        pane.sendDroppedFiles(["file:///private/late"])
        compare(transfer.copies.length, 1)
        verify(!control("reveal").visible)
        transfer.completedLocalFile = "file:///private/received"
        verify(control("reveal").visible)
        control("reveal").clicked()
        compare(transfer.reveals, 1)
    }
}
