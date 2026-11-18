import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

GnomeDialog {
    id: root

    objectName: "gnome-about-dialog"
    property string applicationName: Qt.application.displayName || "Moonwalk"
    property string applicationVersion: qsTranslate("PcView", "Development build")
    title: qsTranslate("PcView", "About %1").arg(applicationName)
    accessibleName: title
    accessibleDescription: applicationName + " " + applicationVersion
    closeInHeader: true
    onCloseRequested: close()
    width: fittedWidth(400)
    onOpened: headerCloseButton.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: root.tokens.spacingS

        // AdwAboutDialog: the application icon above its name.
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/res/starlight.png"
            sourceSize: Qt.size(128, 128)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.applicationName
            color: root.tokens.text
            font.pixelSize: root.tokens.largeTitleSize
            font.weight: Font.Bold
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: root.tokens.spacingM
            text: root.applicationVersion
            color: root.tokens.accentText
            font.pixelSize: root.tokens.bodySize
        }
        GnomeGroup {
            Layout.fillWidth: true
            GnomeActionRow {
                objectName: "gnome-about-website"
                title: qsTranslate("PcView", "Project Website")
                accessibleName: title
                showChevron: true
                onClicked: Qt.openUrlExternally("https://github.com/joaodriessen/moonwalk")
            }
            GnomeActionRow {
                objectName: "gnome-about-diagnostics"
                title: qsTranslate("PcView", "Copy Diagnostic Summary")
                subtitle: qsTranslate("PcView", "For reporting a problem. Review before sharing.")
                accessibleName: title
                onClicked: {
                    if (typeof window !== "undefined" && window.copyDiagnosticSummary)
                        window.copyDiagnosticSummary()
                }
            }
        }
    }
}
