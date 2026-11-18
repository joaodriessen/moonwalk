#include <QtTest>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include "../../../app/gui/adaptive/plasma/plasmadesktopgate.h"
#include "../../../app/gui/adaptive/plasma/plasmastyleprobe.h"

class PlasmaAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void gateRequiresEveryDesktopCondition_data();
    void gateRequiresEveryDesktopCondition();
    void qpaGateAcceptsOnlyWindowedDesktopBackends_data();
    void qpaGateAcceptsOnlyWindowedDesktopBackends();
    void rawArgumentGateExcludesCliAndOptionLaunches_data();
    void rawArgumentGateExcludesCliAndOptionLaunches();
    void styleProbeMirrorsApplicationImportRootFirst();
    void styleProbeFallsBackWhenModuleIsAbsent();
    void styleProbeFallsBackWhenARequiredControlIsAbsent();
    void styleProbeFallsBackWhenAProductionControlIsAbsent();
    void styleProbeFallsBackWhenAControlMappingIsAbsent();
    void styleProbeFallsBackWhenAPluginIsAbsent();
    void styleProbeFallsBackWhenPluginLoadFails();
    void styleProbeFallsBackWhenFullImportFails();
    void styleProbeDoesNotLoadPluginsIntoFallbackParent();
    void styleImportHelperRejectsMalformedSelectedModule();
    void styleProbeAcceptsACompleteLoadableModule();

private:
    static bool writeFile(const QString& path, const QByteArray& contents = {});
    static QString makeCompleteModule(QTemporaryDir& root);
};

void PlasmaAdapterTest::gateRequiresEveryDesktopCondition_data()
{
    QTest::addColumn<QByteArray>("adaptiveOverride");
    QTest::addColumn<QString>("profileOverride");
    QTest::addColumn<QString>("activeSession");
    QTest::addColumn<QByteArray>("qpaPlatform");
    QTest::addColumn<bool>("runningWindowManager");
    QTest::addColumn<bool>("normalGuiRequest");
    QTest::addColumn<bool>("styleAvailable");
    QTest::addColumn<bool>("desktopBuildSupported");
    QTest::addColumn<bool>("expected");

    QTest::newRow("validated Plasma Wayland desktop")
            << QByteArrayLiteral("1") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << true << true
            << true;
    QTest::newRow("validated Plasma X11 desktop")
            << QByteArrayLiteral("1") << QStringLiteral("plasma")
            << QStringLiteral("plasma") << QByteArrayLiteral("xcb")
            << true << true << true << true << true;
    QTest::newRow("unset override activates by default")
            << QByteArray() << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << true << true
            << true;
    QTest::newRow("zero opts out to the legacy shell")
            << QByteArrayLiteral("0") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << true << true
            << false;
    QTest::newRow("invalid adaptive override fails closed")
            << QByteArrayLiteral("true") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << true << true
            << false;
    QTest::newRow("CLI launch stays on QGuiApplication")
            << QByteArrayLiteral("1") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << false << true << true
            << false;
    QTest::newRow("GNOME cannot activate with a plasma override")
            << QByteArrayLiteral("1") << QStringLiteral("plasma")
            << QStringLiteral("GNOME") << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("ambiguous GNOME and Plasma tokens fail closed")
            << QByteArrayLiteral("1") << QString()
            << QStringLiteral("GNOME:KDE") << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("mixed Hyprland and Plasma tokens fail closed")
            << QByteArrayLiteral("1") << QString()
            << QStringLiteral("KDE:Hyprland")
            << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("mixed sway and Plasma tokens fail closed")
            << QByteArrayLiteral("1") << QString()
            << QStringLiteral("KDE:sway") << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("mixed COSMIC and Plasma tokens fail closed")
            << QByteArrayLiteral("1") << QString()
            << QStringLiteral("KDE:COSMIC") << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("Plasma Wayland session alias is accepted")
            << QByteArrayLiteral("1") << QString()
            << QStringLiteral("KDE:plasmawayland")
            << QByteArrayLiteral("wayland")
            << true << true << true << true << true;
    QTest::newRow("explicit generic profile wins on Plasma")
            << QByteArrayLiteral("1") << QStringLiteral("generic")
            << QStringLiteral("KDE") << QByteArrayLiteral("wayland")
            << true << true << true << true << false;
    QTest::newRow("missing style module stays generic")
            << QByteArrayLiteral("1") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << false << true
            << false;
    QTest::newRow("headless launch stays generic")
            << QByteArrayLiteral("1") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("eglfs") << false << true << true << true
            << false;
    QTest::newRow("Steam Link embedded build is excluded")
            << QByteArrayLiteral("1") << QString() << QStringLiteral("KDE")
            << QByteArrayLiteral("wayland") << true << true << true << false
            << false;
}

void PlasmaAdapterTest::gateRequiresEveryDesktopCondition()
{
    QFETCH(QByteArray, adaptiveOverride);
    QFETCH(QString, profileOverride);
    QFETCH(QString, activeSession);
    QFETCH(QByteArray, qpaPlatform);
    QFETCH(bool, runningWindowManager);
    QFETCH(bool, normalGuiRequest);
    QFETCH(bool, styleAvailable);
    QFETCH(bool, desktopBuildSupported);
    QFETCH(bool, expected);

    PlasmaDesktopGateInputs inputs;
    inputs.adaptiveShellOverride = adaptiveOverride;
    inputs.platformProfileOverride = profileOverride;
    inputs.activeSession = activeSession;
    inputs.qpaPlatform = qpaPlatform;
    inputs.runningWindowManager = runningWindowManager;
    inputs.normalGuiRequest = normalGuiRequest;
    inputs.styleAvailable = styleAvailable;
    inputs.desktopBuildSupported = desktopBuildSupported;

    QCOMPARE(PlasmaDesktopGate::shouldActivate(inputs), expected);
}

void PlasmaAdapterTest::qpaGateAcceptsOnlyWindowedDesktopBackends_data()
{
    QTest::addColumn<QByteArray>("qpaPlatform");
    QTest::addColumn<bool>("runningWindowManager");
    QTest::addColumn<bool>("expected");

    QTest::newRow("automatic under a window manager")
            << QByteArray() << true << true;
    QTest::newRow("Wayland")
            << QByteArrayLiteral("wayland") << true << true;
    QTest::newRow("Wayland EGL plugin spelling")
            << QByteArrayLiteral("wayland-egl") << true << true;
    QTest::newRow("X11")
            << QByteArrayLiteral("xcb") << true << true;
    QTest::newRow("automatic without a window manager")
            << QByteArray() << false << false;
    QTest::newRow("EGLFS")
            << QByteArrayLiteral("eglfs") << false << false;
    QTest::newRow("LinuxFB")
            << QByteArrayLiteral("linuxfb") << false << false;
    QTest::newRow("KMS spelling")
            << QByteArrayLiteral("kmsdrm") << false << false;
    QTest::newRow("offscreen test backend")
            << QByteArrayLiteral("offscreen") << true << false;
    QTest::newRow("minimal backend")
            << QByteArrayLiteral("minimal") << true << false;
    QTest::newRow("Wayland with EGLFS fallback")
            << QByteArrayLiteral("wayland;eglfs") << true << false;
    QTest::newRow("X11 with minimal fallback")
            << QByteArrayLiteral("xcb;minimal") << true << false;
}

void PlasmaAdapterTest::qpaGateAcceptsOnlyWindowedDesktopBackends()
{
    QFETCH(QByteArray, qpaPlatform);
    QFETCH(bool, runningWindowManager);
    QFETCH(bool, expected);

    QCOMPARE(PlasmaDesktopGate::isWindowedDesktopQpa(
                 qpaPlatform, runningWindowManager),
             expected);
}

void PlasmaAdapterTest::rawArgumentGateExcludesCliAndOptionLaunches_data()
{
    QTest::addColumn<int>("argumentCount");
    QTest::addColumn<bool>("expected");

    QTest::newRow("ordinary launcher") << 1 << true;
    QTest::newRow("help option") << 2 << false;
    QTest::newRow("version option") << 2 << false;
    QTest::newRow("stream action") << 4 << false;
    QTest::newRow("list action") << 3 << false;
}

void PlasmaAdapterTest::rawArgumentGateExcludesCliAndOptionLaunches()
{
    QFETCH(int, argumentCount);
    QFETCH(bool, expected);

    QCOMPARE(PlasmaDesktopGate::isNormalGuiArgumentShape(argumentCount),
             expected);
}

bool PlasmaAdapterTest::writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

QString PlasmaAdapterTest::makeCompleteModule(QTemporaryDir& root)
{
    const QString qmlRoot = root.path() + QStringLiteral("/qml");
    const QString desktop = qmlRoot + QStringLiteral("/org/kde/desktop");
    const QString desktopPrivate =
            qmlRoot + QStringLiteral("/org/kde/desktop/private");
    const QString stylePrivate =
            qmlRoot + QStringLiteral("/org/kde/qqc2desktopstyle/private");
    QDir().mkpath(desktopPrivate);
    QDir().mkpath(stylePrivate);

    const QStringList controls {
        QStringLiteral("Button.qml"),
        QStringLiteral("CheckBox.qml"),
        QStringLiteral("ComboBox.qml"),
        QStringLiteral("Switch.qml"),
        QStringLiteral("Dialog.qml"),
        QStringLiteral("DialogButtonBox.qml"),
        QStringLiteral("Frame.qml"),
        QStringLiteral("ItemDelegate.qml"),
        QStringLiteral("Label.qml"),
        QStringLiteral("Menu.qml"),
        QStringLiteral("MenuItem.qml"),
        QStringLiteral("Page.qml"),
        QStringLiteral("Pane.qml"),
        QStringLiteral("Popup.qml"),
        QStringLiteral("TextField.qml"),
        QStringLiteral("Slider.qml"),
        QStringLiteral("ScrollBar.qml"),
        QStringLiteral("StackView.qml"),
        QStringLiteral("BusyIndicator.qml"),
        QStringLiteral("ToolButton.qml"),
        QStringLiteral("ToolTip.qml"),
    };
    QByteArray desktopQmldir =
            "module org.kde.desktop\n"
            "optional plugin org_kde_desktop\n"
            "depends org.kde.qqc2desktopstyle.private\n"
            "depends org.kde.desktop.private\n";
    for (const QString& control : controls) {
        desktopQmldir += QFileInfo(control).completeBaseName().toUtf8()
                + " 1.0 " + control.toUtf8() + '\n';
    }
    const QByteArray desktopPrivateQmldir =
            "module org.kde.desktop.private\n"
            "optional plugin org_kde_desktop_private\n";
    const QByteArray stylePrivateQmldir =
            "module org.kde.qqc2desktopstyle.private\n"
            "optional plugin qqc2desktopstyleplugin\n";

    if (!writeFile(desktop + QStringLiteral("/qmldir"), desktopQmldir)
            || !writeFile(desktopPrivate + QStringLiteral("/qmldir"),
                          desktopPrivateQmldir)
            || !writeFile(stylePrivate + QStringLiteral("/qmldir"),
                          stylePrivateQmldir)) {
        return {};
    }

    for (const QString& control : controls) {
        if (!writeFile(desktop + QLatin1Char('/') + control)) {
            return {};
        }
    }

    if (!writeFile(desktop + QStringLiteral("/liborg_kde_desktop.so"))
            || !writeFile(desktopPrivate
                          + QStringLiteral(
                              "/liborg_kde_desktop_private.so"))
            || !writeFile(stylePrivate
                          + QStringLiteral(
                              "/libqqc2desktopstyleplugin.so"))) {
        return {};
    }
    return qmlRoot;
}

void PlasmaAdapterTest::styleProbeMirrorsApplicationImportRootFirst()
{
    const QString executable =
            QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget();
    QVERIFY(!executable.isEmpty());
    const QStringList roots = PlasmaStyleProbe::systemImportRoots();
    QVERIFY(!roots.isEmpty());
    QCOMPARE(roots.constFirst(),
             QFileInfo(executable).absolutePath());
}

void PlasmaAdapterTest::styleProbeFallsBackWhenModuleIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {root.path()},
                [](const QString&, QString*) { return true; },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(QStringLiteral("not found")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenARequiredControlIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());
    QVERIFY(QFile::remove(
                qmlRoot + QStringLiteral("/org/kde/desktop/Slider.qml")));

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString&, QString*) { return true; },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(QStringLiteral("Slider.qml")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenAProductionControlIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());
    QVERIFY(QFile::remove(
                qmlRoot
                + QStringLiteral(
                    "/org/kde/desktop/DialogButtonBox.qml")));

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString&, QString*) { return true; },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(
                QStringLiteral("DialogButtonBox.qml")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenAControlMappingIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());
    const QString qmldir =
            qmlRoot + QStringLiteral("/org/kde/desktop/qmldir");
    QFile file(qmldir);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray contents = file.readAll();
    file.close();
    contents.replace("ItemDelegate 1.0 ItemDelegate.qml\n",
                     "# ItemDelegate 1.0 ItemDelegate.qml\n");
    QVERIFY(writeFile(qmldir, contents));

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString&, QString*) { return true; },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(
                QStringLiteral("does not map ItemDelegate.qml")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenAPluginIsAbsent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());
    QVERIFY(QFile::remove(
                qmlRoot
                + QStringLiteral(
                    "/org/kde/qqc2desktopstyle/private/"
                    "libqqc2desktopstyleplugin.so")));

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString&, QString*) { return true; },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(QStringLiteral("plugin is missing")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenPluginLoadFails()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString& path, QString* error) {
                    if (path.endsWith(QStringLiteral(
                                          "/liborg_kde_desktop_private.so"))) {
                        *error = QStringLiteral("synthetic dependency failure");
                        return false;
                    }
                    return true;
                },
                [](QString*) { return true; });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(
                QStringLiteral("synthetic dependency failure")));
}

void PlasmaAdapterTest::styleProbeFallsBackWhenFullImportFails()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [](const QString&, QString*) { return true; },
                [](QString* error) {
                    *error = QStringLiteral("synthetic QML registration failure");
                    return false;
                });

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(
                QStringLiteral("synthetic QML registration failure")));
}

void PlasmaAdapterTest::styleProbeDoesNotLoadPluginsIntoFallbackParent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());

    const QString fixturePlugin =
            QCoreApplication::applicationDirPath()
            + QStringLiteral(
                "/fixtureplugin/libplasma_style_probe_fixture.so");
    QVERIFY2(QFileInfo::exists(fixturePlugin),
             qPrintable(fixturePlugin));

    const QString desktopPlugin =
            qmlRoot
            + QStringLiteral("/org/kde/desktop/liborg_kde_desktop.so");
    const QString privatePlugin =
            qmlRoot
            + QStringLiteral(
                "/org/kde/desktop/private/"
                "liborg_kde_desktop_private.so");
    QVERIFY(QFile::remove(desktopPlugin));
    QVERIFY(QFile::copy(fixturePlugin, desktopPlugin));
    QVERIFY(QFile::remove(privatePlugin));
    QVERIFY(QFile::copy(fixturePlugin, privatePlugin));

    const QString stylePlugin =
            qmlRoot
            + QStringLiteral(
                "/org/kde/qqc2desktopstyle/private/"
                "libqqc2desktopstyleplugin.so");
    QVERIFY(QFile::remove(stylePlugin));
    QVERIFY(QFile::copy(fixturePlugin, stylePlugin));

    const QString lifecyclePath =
            root.path() + QStringLiteral("/plugin-lifecycle.log");
    qputenv("STARLIGHT_TEST_PLASMA_PLUGIN_LIFECYCLE_FILE",
            lifecyclePath.toLocal8Bit());
    const PlasmaStyleProbeResult result =
            PlasmaStyleProbe::inspectWithPluginMetadata(
                {qmlRoot},
                [](QString* error) {
                    *error = QStringLiteral("synthetic full-import failure");
                    return false;
                });
    qunsetenv("STARLIGHT_TEST_PLASMA_PLUGIN_LIFECYCLE_FILE");

    QVERIFY(!result.available);
    QVERIFY(result.failureReason.contains(
                QStringLiteral("plugin failed validation")));
    QVERIFY2(!QFileInfo::exists(lifecyclePath),
             "identity preflight must never execute plugin code in the parent");
}

void PlasmaAdapterTest::styleImportHelperRejectsMalformedSelectedModule()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString desktop =
            root.path() + QStringLiteral("/org/kde/desktop");
    QVERIFY(QDir().mkpath(desktop));
    QVERIFY(writeFile(
                desktop + QStringLiteral("/qmldir"),
                QByteArrayLiteral(
                    "module org.kde.desktop\n"
                    "Button 2.15 Button.qml\n")));
    QVERIFY(writeFile(
                desktop + QStringLiteral("/Button.qml"),
                QByteArrayLiteral("this is not valid QML\n")));

    QProcess helper;
    helper.setProgram(
                QCoreApplication::applicationDirPath()
                + QStringLiteral(
                    "/styleimportprobe/"
                    "tst_plasma_style_import_probe_helper"));
    helper.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    QProcessEnvironment environment = helper.processEnvironment();
    environment.insert(QStringLiteral("QML2_IMPORT_PATH"), root.path());
    environment.insert(QStringLiteral("QML_IMPORT_PATH"), root.path());
    environment.insert(QStringLiteral("QML_DISABLE_DISK_CACHE"),
                       QStringLiteral("1"));
    const QString configHome =
            root.filePath(QStringLiteral("xdg/config"));
    const QString cacheHome =
            root.filePath(QStringLiteral("xdg/cache"));
    const QString dataHome =
            root.filePath(QStringLiteral("xdg/data"));
    QVERIFY(QDir().mkpath(configHome));
    QVERIFY(QDir().mkpath(cacheHome));
    QVERIFY(QDir().mkpath(dataHome));
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), cacheHome);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), dataHome);
    helper.setProcessEnvironment(environment);
    helper.start();
    QVERIFY2(helper.waitForFinished(10000),
             qPrintable(helper.errorString()));
    QVERIFY2(helper.exitStatus() == QProcess::NormalExit,
             qPrintable(QString::fromLocal8Bit(helper.readAllStandardError())));
    QVERIFY2(helper.exitCode() != 0,
             "a malformed selected style module must fail the full import probe");
}

void PlasmaAdapterTest::styleProbeAcceptsACompleteLoadableModule()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString qmlRoot = makeCompleteModule(root);
    QVERIFY(!qmlRoot.isEmpty());
    QStringList inspectedPlugins;

    const PlasmaStyleProbeResult result = PlasmaStyleProbe::inspect(
                {qmlRoot},
                [&inspectedPlugins](const QString& path, QString*) {
                    inspectedPlugins.append(path);
                    return true;
                },
                [](QString*) { return true; });

    QVERIFY2(result.available, qPrintable(result.failureReason));
    QCOMPARE(result.modulePath,
             qmlRoot + QStringLiteral("/org/kde/desktop"));
    QCOMPARE(inspectedPlugins.size(), 3);
}

QTEST_MAIN(PlasmaAdapterTest)

#include "tst_plasmaadapter.moc"
