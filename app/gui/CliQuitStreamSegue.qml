import QtQuick 2.15
import QtQuick.Controls 2.15

import ComputerManager 1.0
import Session 1.0

import Starlight 1.0
import "starlight"

Item {
    function onSearchingComputer() {
        stageView.title = qsTr("Connecting to the host…")
    }

    function onQuittingApp() {
        stageView.title = qsTr("Quitting app…")
    }

    function onFailure(message) {
        errorDialog.text = message
        errorDialog.open()
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
            launcher.quittingApp.connect(onQuittingApp)
            launcher.failed.connect(onFailure)
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
            Qt.quit()
        }
    }
}
