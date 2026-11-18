import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

PlasmaDialog {
    id: root

    objectName: "plasma-about-dialog"
    property string applicationName: Qt.application.displayName || "Moonwalk"
    property string applicationVersion: qsTranslate("PcView", "Development build")
    title: qsTranslate("PcView", "About %1").arg(applicationName)
    accessibleName: title
    accessibleDescription: applicationName + " " + applicationVersion
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 6

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/res/starlight.png"
            sourceSize: Qt.size(96, 96)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.applicationName
            font.weight: Font.DemiBold
            font.pointSize: Qt.application.font.pointSize + 2
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.applicationVersion
            opacity: 0.7
        }
        ToolButton {
            id: websiteButton
            Layout.alignment: Qt.AlignHCenter
            text: qsTranslate("PcView", "Project Website")
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.Button
            Accessible.name: text
            Accessible.onPressAction: websiteButton.clicked()
            onClicked: Qt.openUrlExternally(
                           "https://github.com/joaodriessen/moonwalk")
        }
        ToolButton {
            objectName: "plasma-about-diagnostics"
            Layout.alignment: Qt.AlignHCenter
            text: qsTranslate("PcView", "Copy Diagnostic Summary")
            icon.name: "lucide-copy"
            focusPolicy: Qt.TabFocus
            Accessible.name: text
            onClicked: {
                if (typeof window !== "undefined" && window.copyDiagnosticSummary)
                    window.copyDiagnosticSummary()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            PlasmaButton {
                text: qsTranslate("PcView", "Close")
                accessibleName: text
                onClicked: root.close()
            }
        }
    }
}
