import QtQuick 2.15
import QtQuick.Controls 2.15

import ComputerManager 1.0

import Starlight 1.0
import "starlight"

Item {
    function onSearchingComputer() {
        stageView.title = qsTr("Connecting to the host…")
    }

    function onPairing(pcName, pin) {
        stageView.title = qsTr("Pairing… Enter %1 on %2.").arg(pin).arg(pcName)
    }

    function onFailed(message) {
        stageView.busy = false
        errorDialog.text = message
        errorDialog.open()
    }

    function onSuccess(appName) {
        stageView.busy = false
        pairCompleteDialog.open()
    }

    // Allow user to back out of pairing
    Keys.onEscapePressed: {
        Qt.quit()
    }
    Keys.onBackPressed: {
        Qt.quit()
    }
    Keys.onCancelPressed: {
        Qt.quit()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            // No legacy blue ToolBar to hide any more -- see StreamSegue.qml's
            // matching note. (This page already has its own unconditional
            // Escape -> Qt.quit() above, so no change needed there.)

            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.pairing.connect(onPairing)
            launcher.failed.connect(onFailed)
            launcher.success.connect(onSuccess)
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
        id: pairCompleteDialog
        closePolicy: Popup.CloseOnEscape

        text: qsTr("Pairing completed successfully.")
        standardButtons: Dialog.Ok
        onClosed: {
            Qt.quit()
        }
    }
}
