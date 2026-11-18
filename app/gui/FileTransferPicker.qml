import QtQuick 2.15
import QtQuick.Dialogs 6.2
import QtCore 6.2

FileDialog {
    property bool saving: false
    property string suggestion: ""
    signal picked(url location)
    signal pickedFiles(var locations)
    fileMode: saving ? FileDialog.SaveFile : FileDialog.OpenFiles
    title: saving ? qsTr("Save file from host") : qsTr("Send files to host")
    currentFolder: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
    Component.onCompleted: if (saving && suggestion) selectedFile = currentFolder.toString() + "/" + encodeURIComponent(suggestion)
    onAccepted: { if (saving) picked(selectedFile); else pickedFiles(selectedFiles); destroy() }
    onRejected: destroy()
}
