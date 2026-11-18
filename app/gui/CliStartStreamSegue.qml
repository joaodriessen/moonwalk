import QtQuick 2.15
import QtQuick.Controls 2.15

import ComputerManager 1.0

import Starlight 1.0
import "starlight"

Item {
    function onSearchingComputer() {
        stageView.title = qsTr("Connecting to the host…")
    }

    function onSearchingApp() {
        stageView.title = qsTr("Loading app list…")
    }

    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": true
        })
        stackView.push(segue)
    }

    function onLaunchFailed(message) {
        errorDialog.text = message
        errorDialog.open()
        console.error(message)
    }

    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    // No legacy blue ToolBar to hide any more -- see StreamSegue.qml's
    // matching note. Escape still needs to work here too.
    Keys.onEscapePressed: (event) => {
        if (stackView.depth > 1) {
            window.goBack()
        } else {
            event.accepted = false
        }
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
    }

    // Opaque ground, from the window's live desktop-following palette.
    Rectangle {
        anchors.fill: parent
        color: typeof window !== "undefined" && window.transitionGround !== undefined
               ? window.transitionGround : Theme.ground
        radius: typeof window !== "undefined" && window.csdRadius !== undefined ? window.csdRadius : 0
    }

    SessionStatusView {
        id: stageView
        anchors.centerIn: parent
        width: Math.max(200, Math.min(parent.width - 48, 480))
        busy: true
        title: qsTr("Connecting to the host…")
    }

    ErrorMessageDialog {
        id: errorDialog

        onClosed: {
            Qt.quit();
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        text:qsTr("Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        property string appName : ""

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { launcher.quitRunningApp() }}
            stackView.push(component.createObject(stackView, params))
        }

        onAccepted: quitApp()
        // Deliberate: this is a one-shot CLI launch with no host page to fall
        // back to — declining the quit-and-replace confirmation exits the app
        // rather than leaving a dead-end blank segue.
        onRejected: Qt.quit()
    }
}
