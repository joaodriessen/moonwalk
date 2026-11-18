import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: pane
    property var transfer
    property bool active: false
    property var selectedEntry: ({})
    property string pickerError: ""
    property string lastTransfer: ""
    property string remoteDirectory: ""
    property string folder: folderPicker.currentText || "Downloads"
    property var currentPicker: null
    property int pickerGeneration: 0
    SystemPalette { id: systemColors }
    // The hosting window's palette follows the desktop on every surface.
    readonly property var colors: ApplicationWindow.window ? ApplicationWindow.window.palette : systemColors
    readonly property bool available: !!transfer && transfer.supported
    readonly property bool busy: !!transfer && transfer.busy
    readonly property bool queueActive: !!transfer && !!transfer.queueActive
    readonly property string destination: folder + (remoteDirectory ? "/" + remoteDirectory : "")
    spacing: 8

    function refresh() {
        lastTransfer = ""
        selectedEntry = ({})
        var owner = transfer
        if (owner && owner.supported && !owner.busy && !pane.queueActive) owner.listRemote(folder, remoteDirectory)
    }
    function invalidatePicker() {
        pickerGeneration += 1
        var previous = currentPicker
        currentPicker = null
        if (previous) { previous.close(); previous.destroy() }
    }
    function createPicker(saving, suggestion) {
        if (!active || !available || busy || queueActive) return null
        invalidatePicker()
        var generation = pickerGeneration
        var owner = transfer
        var component = Qt.createComponent("FileTransferPicker.qml")
        if (component.status !== Component.Ready) {
            pickerError = qsTr("The system file picker is unavailable. Enter a local file path below.")
            return null
        }
        var dialog = component.createObject(pane, {saving: saving, suggestion: suggestion || ""})
        if (!dialog) { pickerError = component.errorString(); return null }
        currentPicker = dialog
        dialog.picked.connect(function(location) {
            if (generation !== pane.pickerGeneration || owner !== pane.transfer || !pane.active || !pane.available || pane.busy) return
            if (saving) localDestination.text = location.toString()
            else useSource(location.toString())
        })
        dialog.pickedFiles.connect(function(locations) {
            if (generation !== pane.pickerGeneration || owner !== pane.transfer || !pane.active || !pane.available || pane.busy || pane.queueActive) return
            pane.sendDroppedFiles(locations)
        })
        return dialog
    }
    function choose(saving, suggestion) {
        var dialog = createPicker(saving, suggestion)
        if (dialog) dialog.open()
    }
    function useSource(location) {
        source.text = location
        var part = location.substring(location.lastIndexOf("/") + 1)
        try { uploadName.text = decodeURIComponent(part) } catch (error) { uploadName.text = part }
        pickerError = ""
    }
    function sendDroppedFiles(locations) {
        if (!available || busy || queueActive) return
        pickerError = ""
        lastTransfer = ""
        transfer.sendFiles(locations, folder, remoteDirectory)
    }
    function localUrl(path) {
        return transfer.localFileUrl(path)
    }
    onAvailableChanged: { if (!available) invalidatePicker(); else if (active) refresh() }
    onActiveChanged: { if (!active) invalidatePicker(); else if (available) refresh() }
    onTransferChanged: {
        invalidatePicker()
        remoteDirectory = ""
        selectedEntry = ({})
        source.text = ""
        uploadName.text = ""
        localDestination.text = ""
        replaceRemote.checked = false
        replaceLocal.checked = false
        pickerError = ""
        lastTransfer = ""
        if (active && available) refresh()
    }
    onBusyChanged: if (busy) invalidatePicker()
    Component.onDestruction: invalidatePicker()
    Component.onCompleted: if (active && available) refresh()
    Connections {
        target: pane.transfer
        ignoreUnknownSignals: true
        function onPickerRequested(action, folder, relativeFile) {
            pane.choose(action === "receive", relativeFile.substring(relativeFile.lastIndexOf("/") + 1))
        }
    }
    Label {
        visible: !pane.available
        text: qsTr("File transfer needs Moonwalk Host on the other computer and an active stream. This host does not offer it.")
        wrapMode: Text.Wrap; Layout.fillWidth: true
    }
    ColumnLayout {
        visible: pane.available
        Layout.fillWidth: true
        spacing: 8
        Label { text: qsTr("Destination on the host"); font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: folderPicker
                objectName: "starlight.files.remote-folder"
                Layout.fillWidth: true
                Layout.minimumWidth: 160
                model: ["Downloads", "Documents", "Desktop"]
                enabled: !pane.busy && !pane.queueActive
                onActivated: { pane.remoteDirectory = ""; pane.refresh() }
            }
            Button { text: qsTr("Refresh"); enabled: !pane.busy && !pane.queueActive; onClicked: pane.refresh() }
        }
        Label { text: pane.folder + (pane.remoteDirectory ? "/" + pane.remoteDirectory : ""); wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button {
            text: qsTr("Up One Folder"); visible: pane.remoteDirectory !== ""; enabled: !pane.busy && !pane.queueActive
            onClicked: {
                var slash = pane.remoteDirectory.lastIndexOf("/")
                pane.remoteDirectory = slash < 0 ? "" : pane.remoteDirectory.substring(0, slash)
                pane.refresh()
            }
        }
        ListView {
            id: remoteFiles
            objectName: "starlight.files.remote-list"
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            clip: true
            model: pane.transfer ? pane.transfer.remoteEntries : []
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                objectName: "starlight.files.entry." + modelData.name
                width: remoteFiles.width
                text: (modelData.isDirectory ? "▸ " : "") + modelData.name
                highlighted: pane.selectedEntry.relativePath === modelData.relativePath
                enabled: !pane.busy && !pane.queueActive
                onClicked: {
                    if (modelData.isDirectory) {
                        pane.remoteDirectory = modelData.relativePath
                        pane.refresh()
                    } else pane.selectedEntry = modelData
                }
            }
            Label { anchors.centerIn: parent; visible: remoteFiles.count === 0 && !pane.busy; text: qsTr("No files in this folder") }
        }
        Label { text: qsTr("Send files"); font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: source; objectName: "starlight.files.source"; placeholderText: qsTr("Local file path or file URL"); Layout.fillWidth: true; enabled: !pane.busy && !pane.queueActive; selectByMouse: true }
            Button { text: qsTr("Choose Files…"); enabled: !pane.busy && !pane.queueActive; onClicked: pane.choose(false, "") }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            color: drop.containsDrag ? Qt.rgba(pane.colors.highlight.r, pane.colors.highlight.g, pane.colors.highlight.b, 0.25) : "transparent"
            border.color: drop.containsDrag ? pane.colors.highlight : Qt.rgba(pane.colors.windowText.r, pane.colors.windowText.g, pane.colors.windowText.b, 0.3); radius: 6
            Label { anchors.centerIn: parent; text: qsTr("Drop files to copy to %1 on the host").arg(pane.destination) }
            DropArea {
                id: drop; anchors.fill: parent; enabled: !pane.busy && !pane.queueActive
                onDropped: function(event) {
                    if (event.hasUrls && event.urls.length > 0) {
                        pane.sendDroppedFiles(event.urls); event.acceptProposedAction()
                    } else pane.pickerError = qsTr("Drop regular local files here.")
                }
            }
        }
        TextField { id: uploadName; visible: false; objectName: "starlight.files.remote-name"; placeholderText: qsTr("Name to save on the host"); Layout.fillWidth: true; enabled: !pane.busy && !pane.queueActive; selectByMouse: true }
        CheckBox { id: replaceRemote; visible: false; checked: false }
        Label { text: qsTr("Copies keep original files. Existing names need a choice before copying."); wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button {
            objectName: "starlight.files.send"
            text: qsTr("Copy to %1 on the Host").arg(pane.destination); Layout.fillWidth: true
            enabled: !pane.busy && !pane.queueActive && source.text.length > 0
            onClicked: {
                pane.lastTransfer = ""
                pane.transfer.sendFiles([pane.localUrl(source.text)], pane.folder, pane.remoteDirectory)
            }
        }
        Label { text: qsTr("Receive selected file"); font.bold: true }
        Label { text: pane.selectedEntry.name || qsTr("Choose a file in the host folder above."); wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: localDestination; objectName: "starlight.files.destination"; placeholderText: qsTr("Local save path or file URL"); Layout.fillWidth: true; enabled: !pane.busy && !pane.queueActive; selectByMouse: true }
            Button { text: qsTr("Save As…"); enabled: !pane.busy && !pane.queueActive && !!pane.selectedEntry.name; onClicked: pane.choose(true, pane.selectedEntry.name) }
        }
        CheckBox { id: replaceLocal; text: qsTr("Replace an existing local file"); Layout.fillWidth: true; enabled: !pane.busy && !pane.queueActive }
        Label { text: qsTr("The existing file is replaced only after the transfer completes."); wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button {
            objectName: "starlight.files.receive"
            text: qsTr("Receive File"); Layout.fillWidth: true
            enabled: !pane.busy && !pane.queueActive && !!pane.selectedEntry.name && localDestination.text.length > 0
            onClicked: {
                pane.lastTransfer = qsTr("File received and checksum verified.")
                pane.transfer.receiveFile(pane.folder, pane.selectedEntry.relativePath, pane.localUrl(localDestination.text), replaceLocal.checked)
            }
        }
        Label { text: qsTr("Copy queue"); font.bold: true; visible: copyQueue.count > 0 }
        ListView {
            id: copyQueue
            objectName: "starlight.files.queue"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(4, count) * 48
            clip: true
            model: pane.transfer && pane.transfer.queueEntries ? pane.transfer.queueEntries : []
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                width: copyQueue.width
                height: 48
                text: modelData.name + " → " + modelData.destination + " · " + modelData.status + (modelData.message ? " · " + modelData.message : "")
                ToolTip.visible: hovered
                ToolTip.text: text
                Accessible.name: text
            }
        }
        Label {
            visible: !!pane.transfer && !!pane.transfer.collisionName
            text: qsTr("%1 already exists in %2 on the host. Keep both with a new name, or skip this file.").arg(pane.transfer && pane.transfer.collisionName ? pane.transfer.collisionName : "").arg(pane.destination)
            wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        RowLayout {
            visible: !!pane.transfer && !!pane.transfer.collisionName
            Button { objectName: "starlight.files.keep-both"; text: qsTr("Keep Both"); enabled: !pane.busy; onClicked: pane.transfer.resolveCollision("keepBoth") }
            Button { objectName: "starlight.files.skip"; text: qsTr("Skip"); enabled: !pane.busy; onClicked: pane.transfer.resolveCollision("skip") }
        }
        Button {
            objectName: "starlight.files.reveal"
            text: qsTr("Open Received Folder")
            visible: !!pane.transfer && !!pane.transfer.completedLocalFile && pane.transfer.completedLocalFile.toString() !== ""
            onClicked: pane.transfer.revealReceivedFile()
        }
        ProgressBar { Layout.fillWidth: true; visible: pane.busy; from: 0; to: Math.max(1, pane.transfer ? pane.transfer.total : 1); value: pane.transfer ? pane.transfer.progress : 0 }
        Label { visible: pane.busy; text: pane.transfer ? qsTr("%1 / %2 bytes").arg(pane.transfer.progress).arg(pane.transfer.total) : "" }
        Button { objectName: "starlight.files.cancel"; visible: pane.busy || pane.queueActive; text: pane.queueActive ? qsTr("Cancel Copy Queue") : qsTr("Cancel Transfer"); onClicked: pane.transfer.cancel() }
    }
    Label { text: pane.pickerError || (pane.transfer ? pane.transfer.error : ""); visible: text !== ""; wrapMode: Text.Wrap; Layout.fillWidth: true }
    Label { text: pane.lastTransfer; visible: !!text && !pane.busy && !!pane.transfer && pane.transfer.error === ""; wrapMode: Text.Wrap; Layout.fillWidth: true }
}
