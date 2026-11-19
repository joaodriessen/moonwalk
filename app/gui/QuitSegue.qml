import QtQuick 2.0
import QtQuick.Controls 2.15

import ComputerManager 1.0
import Session 1.0

import Starlight 1.0
import "starlight"

Item {
    property string appName
    property var quitRunningAppFn
    property Session nextSession : null
    property string nextAppName : ""
    // GUI chains delegate their terminal state to main.qml. The existing
    // fallback is retained for CLI callers that have no application window.
    property var streamHandoffFn
    property var terminalRecoveryFn
    property bool quitDispatchStarted: false
    property bool terminalOutcomeHandled: false

    property string stageText : qsTr("Quitting %1…").arg(appName)

    function quitAppCompleted(error)
    {
        if (terminalOutcomeHandled)
            return
        terminalOutcomeHandled = true
        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
            console.error(error)
        }

        // If we're supposed to launch another game after this, do so now
        if (error === undefined && nextSession !== null) {
            if (streamHandoffFn) {
                if (streamHandoffFn(nextSession, nextAppName))
                    return
                if (terminalRecoveryFn)
                    terminalRecoveryFn(qsTr("Couldn't start a new stream. Go back to the host page and try again."))
                stackView.pop()
                return
            }
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": nextAppName, "session": nextSession})
            stackView.replace(segue)
        }
        else {
            if (terminalRecoveryFn)
                terminalRecoveryFn(error)
            // Exit this view
            stackView.pop()
        }
    }

    // No legacy blue ToolBar to hide/show any more -- see StreamSegue.qml's
    // matching note. Escape still needs to work here too.
    Keys.onEscapePressed: (event) => {
        if (stackView.depth > 1) {
            window.goBack()
        } else {
            event.accepted = false
        }
    }

    StackView.onActivated: {
        // Connect the quit completion signal
        ComputerManager.quitAppCompleted.connect(quitAppCompleted)

        // Start the quit operation if requested
        if (quitRunningAppFn && !quitDispatchStarted) {
            quitDispatchStarted = true
            if (quitRunningAppFn() === false)
                quitAppCompleted(qsTr("Couldn't ask the host to end the running session."))
        }
    }

    StackView.onDeactivating: {
        // Disconnect the signal
        ComputerManager.quitAppCompleted.disconnect(quitAppCompleted)
    }

    // Opaque ground, from the window's live desktop-following palette.
    Rectangle {
        anchors.fill: parent
        color: typeof window !== "undefined" && window.transitionGround !== undefined
               ? window.transitionGround : Theme.ground
        radius: typeof window !== "undefined" && window.csdRadius !== undefined ? window.csdRadius : 0
    }

    SessionStatusView {
        anchors.centerIn: parent
        width: Math.max(200, Math.min(parent.width - 48, 480))
        busy: true
        title: stageText
    }

    ErrorMessageDialog {
        id: errorDialog
    }
}
