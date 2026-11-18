#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include "../../app/application/hostworkflow.h"
#include "../../app/application/materialstylebootstrap.h"
#include "../../app/application/pollinglifecycle.h"
#include "../../app/gui/hostlistmodel.h"
#include "../../app/gui/lucideiconprovider.h"
#include "testhostsource.h"

namespace {

// The GNOME suites certify their components the way the shipped GNOME
// route runs them: with the adapter context active. Deriving that from
// the -input file name keeps every suite self-sufficient under the
// sharded certification matrix, where no per-suite environment exists;
// the explicit environment variable remains an override for manual runs.
bool inputFileTargetsGnomeSuite()
{
    const QStringList arguments = QCoreApplication::arguments();
    for (int index = 0; index + 1 < arguments.size(); ++index) {
        if (arguments.at(index) == QStringLiteral("-input")
                && QFileInfo(arguments.at(index + 1)).fileName()
                   .startsWith(QStringLiteral("tst_gnome"))) {
            return true;
        }
    }
    return false;
}

}

class QmlCharacterizationSetup : public QObject
{
    Q_OBJECT

public:
    QmlCharacterizationSetup()
    {
        qputenv("QML_XHR_ALLOW_FILE_READ", "1");

        const bool materialStyleSelected =
                qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE")
                .compare(QStringLiteral("Material"),
                         Qt::CaseInsensitive) == 0;
        MaterialStyleBootstrap::applyLegacyDefaults(
                    materialStyleSelected);

        // Reduced-motion characterization must let KDE's style provider read
        // the isolated kdeglobals fixture supplied through XDG_CONFIG_HOME.
        // Explicit XDG roots are also authoritative: turning on Qt's global
        // test mode would silently redirect those runs back into ~/.qttest.
        const auto hasExplicitAbsoluteRoot = [](const char* name) {
            const QString value = qEnvironmentVariable(name);
            return !value.isEmpty() && QDir::isAbsolutePath(value);
        };
        const bool hasExplicitXdgRoots =
                hasExplicitAbsoluteRoot("XDG_CONFIG_HOME")
                && hasExplicitAbsoluteRoot("XDG_CACHE_HOME")
                && hasExplicitAbsoluteRoot("XDG_DATA_HOME");
        if (!qEnvironmentVariableIsSet(
                    "STARLIGHT_TEST_PLASMA_REDUCED_MOTION")
                && !hasExplicitXdgRoots) {
            QStandardPaths::setTestModeEnabled(true);
        }
        if (!m_SettingsDirectory.isValid()) {
            qFatal("Unable to create an isolated QML settings directory");
        }
    }

public slots:
    void applicationAvailable()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("StarlightTask3Tests"));
        QCoreApplication::setOrganizationDomain(QStringLiteral("tests.invalid"));
        QCoreApplication::setApplicationName(QStringLiteral("qml-characterization"));

        const QStringList arguments = QCoreApplication::arguments();
        const int input = arguments.indexOf(QStringLiteral("-input"));
        if (input >= 0 && input + 1 < arguments.size()
                && QFileInfo(arguments.at(input + 1)).fileName()
                   == QStringLiteral("tst_gnomeheadericons.qml")) {
            QIcon::setThemeSearchPaths({QStringLiteral(QUICK_TEST_SOURCE_DIR)
                                       + QStringLiteral("/fixtures/icon-themes")});
            QIcon::setThemeName(QStringLiteral("opaque-header"));
        }

        if (qEnvironmentVariable("STARLIGHT_TEST_FORCE_THEME") == QStringLiteral("dark")) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif
            QPalette palette;
            palette.setColor(QPalette::Window, QColor(32, 32, 36));
            palette.setColor(QPalette::WindowText, QColor(230, 230, 232));
            palette.setColor(QPalette::Base, QColor(42, 42, 46));
            palette.setColor(QPalette::Text, QColor(230, 230, 232));
            QGuiApplication::setPalette(palette);
        }

        // main.qml owns a QtCore Settings object for launcher size. Route both
        // formats into this temporary directory before its QML is created.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_SettingsDirectory.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_SettingsDirectory.path());

        qmlRegisterType<HostWorkflow>("Starlight.Application", 1, 0, "HostWorkflow");
        qmlRegisterType<PollingLifecycle>("Starlight.Application", 1, 0, "PollingLifecycle");
        qmlRegisterType<HostListModel>("HostListModel", 1, 0, "HostListModel");
        qmlRegisterType<TestHostSource>("Starlight.Test", 1, 0, "TestHostSource");
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->addImportPath(QStringLiteral(TASK3_QML_IMPORT_PATH));
        LucideIconProvider::install(engine);
        // The Qt compatibility-floor runner has no Qt 6 Kirigami package, and
        // the Plasma surfaces need it.
        QQmlComponent kirigami(engine);
        kirigami.setData("import QtQml\nimport org.kde.kirigami 2.20 as Kirigami\nQtObject {}", QUrl());
        engine->rootContext()->setContextProperty(
                    QStringLiteral("kirigamiAvailable"),
                    kirigami.isReady());
        const bool plasmaAdapterExpected = qEnvironmentVariableIsSet(
                    "STARLIGHT_TEST_PLASMA_ADAPTER_ACTIVE");
        const bool gnomeAdapterExpected = qEnvironmentVariableIsSet(
                    "STARLIGHT_TEST_GNOME_ADAPTER_ACTIVE")
                || inputFileTargetsGnomeSuite();
        engine->rootContext()->setContextProperty(
                    QStringLiteral("plasmaDesktopAdapterActive"),
                    plasmaAdapterExpected);
        engine->rootContext()->setContextProperty(
                    QStringLiteral("materialStyleActive"),
                    qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE")
                    .compare(QStringLiteral("Material"),
                             Qt::CaseInsensitive) == 0);
        engine->rootContext()->setContextProperty(
                    QStringLiteral("plasmaAdapterRouteExpected"),
                    plasmaAdapterExpected);
        engine->rootContext()->setContextProperty(
                    QStringLiteral("plasmaReducedMotionExpected"),
                    qEnvironmentVariableIsSet(
                        "STARLIGHT_TEST_PLASMA_REDUCED_MOTION"));
        engine->rootContext()->setContextProperty(
                    QStringLiteral("plasmaWidgetStyleExpected"),
                    qEnvironmentVariable(
                        "STARLIGHT_TEST_EXPECT_WIDGET_STYLE"));
        engine->rootContext()->setContextProperty(
                    QStringLiteral("gnomeDesktopAdapterActive"),
                    gnomeAdapterExpected);
        engine->rootContext()->setContextProperty(
                    QStringLiteral("gnomeAdapterRouteExpected"),
                    gnomeAdapterExpected);
    }

private:
    QTemporaryDir m_SettingsDirectory;
};

QUICK_TEST_MAIN_WITH_SETUP(qml_characterization, QmlCharacterizationSetup)

#include "tst_qml.moc"
