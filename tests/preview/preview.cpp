// Developer-only visual tool: runs the production main.qml against the test
// doubles with private state, drives named scenes and saves what it renders.
// See preview.py for the isolated launcher. Never ship or link this.
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStyleHints>
#include <QUrl>

#include "application/hostworkflow.h"
#include "application/pollinglifecycle.h"
#include "gui/hostlistmodel.h"
#include "gui/lucideiconprovider.h"
#include "testhostsource.h"

class PreviewSupportInfo : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool copySummary() { return true; }
    // Unlike Item.grabToImage this includes the Overlay layer, so dialogs and
    // menus appear exactly as the window renders them.
    Q_INVOKABLE bool grab(QObject* window, const QString& path)
    {
        auto* quick = qobject_cast<QQuickWindow*>(window);
        return quick && quick->grabWindow().save(path);
    }
};

int main(int argc, char** argv)
{
    const QString desktop = qEnvironmentVariable("STARLIGHT_PREVIEW_DESKTOP", "gnome");
    const bool plasma = desktop == QLatin1String("plasma");
    const bool gnome = desktop == QLatin1String("gnome");
    QCoreApplication::setOrganizationName("StarlightPreview");
    QCoreApplication::setApplicationName("Starlight");
    QCoreApplication::setApplicationVersion("preview");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    if (gnome) QQuickWindow::setDefaultAlphaBuffer(true);
    QQuickStyle::setStyle(plasma ? "org.kde.desktop" : "StarlightAdwaita");
        QApplication app(argc, argv);
    app.setProperty("starlightVersion", "0.3.1-preview");
    QGuiApplication::setApplicationDisplayName("Moonwalk");
    if (qEnvironmentVariableIsSet("STARLIGHT_PREVIEW_RTL")) QGuiApplication::setLayoutDirection(Qt::RightToLeft);
    if (qEnvironmentVariableIsSet("STARLIGHT_PREVIEW_FONT")) {
        const QStringList spec = qEnvironmentVariable("STARLIGHT_PREVIEW_FONT").split(',');
        QFont font(spec.value(0)); font.setPointSize(spec.value(1, "10").toInt());
        QApplication::setFont(font);
    }
    qmlRegisterType<HostWorkflow>("Starlight.Application", 1, 0, "HostWorkflow");
    qmlRegisterType<PollingLifecycle>("Starlight.Application", 1, 0, "PollingLifecycle");
    qmlRegisterType<HostListModel>("HostListModel", 1, 0, "HostListModel");
    qmlRegisterType<TestHostSource>("Starlight.Test", 1, 0, "TestHostSource");

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(PREVIEW_SOURCE_DIR "/../qml/imports"));
    LucideIconProvider::install(&engine);
    engine.addImportPath(QStringLiteral(PREVIEW_SOURCE_DIR "/../../app/gui/style"));
    PreviewSupportInfo support;
    auto* context = engine.rootContext();
    context->setContextProperty("supportInfo", &support);
    context->setContextProperty("runConfigChecks", false);
    context->setContextProperty("adaptiveShellRequested", true);
    context->setContextProperty("gnomeDesktopAdapterActive", gnome);
    context->setContextProperty("plasmaDesktopAdapterActive", plasma);
    context->setContextProperty("materialStyleActive", false);
    context->setContextProperty("initialView", QUrl::fromLocalFile(
            PREVIEW_SOURCE_DIR "/../../app/gui/PcView.qml"));
    context->setContextProperty("previewDesktop", desktop);
    context->setContextProperty("previewContrast", qEnvironmentVariableIsSet("STARLIGHT_PREVIEW_CONTRAST"));
    context->setContextProperty("previewAccent", qEnvironmentVariable("STARLIGHT_PREVIEW_ACCENT"));
    context->setContextProperty("previewOut", qEnvironmentVariable("STARLIGHT_PREVIEW_OUT"));
    context->setContextProperty("previewScheme", qEnvironmentVariable("STARLIGHT_PREVIEW_SCHEME", "light"));
    context->setContextProperty("previewScenes", qEnvironmentVariable("STARLIGHT_PREVIEW_SCENES"));
    context->setContextProperty("previewWidth", qEnvironmentVariableIntValue("STARLIGHT_PREVIEW_W"));
    context->setContextProperty("previewHeight", qEnvironmentVariableIntValue("STARLIGHT_PREVIEW_H"));
    context->setContextProperty("previewControlsUrl", QUrl::fromLocalFile(
            PREVIEW_SOURCE_DIR "/../../app/gui/SessionControls.qml"));
    context->setContextProperty("previewErrorUrl", QUrl::fromLocalFile(
            PREVIEW_SOURCE_DIR "/../../app/gui/ErrorMessageDialog.qml"));
    context->setContextProperty("previewMainUrl", QUrl::fromLocalFile(
            PREVIEW_SOURCE_DIR "/../../app/gui/main.qml"));
    engine.load(QUrl::fromLocalFile(PREVIEW_SOURCE_DIR "/Driver.qml"));
    if (engine.rootObjects().isEmpty()) return 3;
    return app.exec();
}

#include "preview.moc"
