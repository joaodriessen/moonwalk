#include "plasmastyleprobe.h"

#include <memory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#if defined(QT_WIDGETS_LIB)
#include <QApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#endif

namespace {

constexpr auto kStyleImportProbeArgument =
        "--internal-plasma-style-import-probe";

void appendEnvironmentRoots(QStringList& roots, const QByteArray& value)
{
    const QList<QByteArray> entries = value.split(QDir::listSeparator().toLatin1());
    for (const QByteArray& entry : entries) {
        if (!entry.isEmpty()) {
            roots.append(QString::fromLocal8Bit(entry));
        }
    }
}

bool verifyStyleImportInSubprocess(QString* error)
{
    const QString executable =
            QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget();
    if (executable.isEmpty()) {
        *error = QStringLiteral("could not resolve the running executable");
        return false;
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments(
                {QString::fromLatin1(kStyleImportProbeArgument)});
    QProcessEnvironment environment =
            QProcessEnvironment::systemEnvironment();
    QTemporaryDir xdgRoot(
                QDir::tempPath()
                + QStringLiteral("/starlight-plasma-style-probe-XXXXXX"));
    if (!xdgRoot.isValid()) {
        *error = QStringLiteral(
                    "could not create isolated import-probe state");
        return false;
    }
    const QString configHome =
            xdgRoot.filePath(QStringLiteral("config"));
    const QString cacheHome =
            xdgRoot.filePath(QStringLiteral("cache"));
    const QString dataHome =
            xdgRoot.filePath(QStringLiteral("data"));
    if (!QDir().mkpath(configHome)
            || !QDir().mkpath(cacheHome)
            || !QDir().mkpath(dataHome)) {
        *error = QStringLiteral(
                    "could not create isolated import-probe XDG roots");
        return false;
    }
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), cacheHome);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), dataHome);
    environment.insert(QStringLiteral("QML_DISABLE_DISK_CACHE"),
                       QStringLiteral("1"));
    process.setProcessEnvironment(environment);
    process.start();
    if (!process.waitForStarted(3000)) {
        *error = QStringLiteral("could not start import probe: %1")
                .arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished(1000);
        *error = QStringLiteral("import probe timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) {
        QByteArray diagnostic = process.readAllStandardError().trimmed();
        if (diagnostic.isEmpty()) {
            diagnostic = process.readAllStandardOutput().trimmed();
        }
        diagnostic.truncate(4096);
        *error = diagnostic.isEmpty()
                ? QStringLiteral("import probe exited with code %1")
                  .arg(process.exitCode())
                : QString::fromLocal8Bit(diagnostic);
        return false;
    }
    return true;
}

bool readQmldirIdentity(const QString& pluginPath,
                        QString* module,
                        QString* className,
                        QString* error)
{
    QFile qmldir(
                QDir(QFileInfo(pluginPath).absolutePath())
                .filePath(QStringLiteral("qmldir")));
    if (!qmldir.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("plugin qmldir could not be read");
        return false;
    }

    for (const QByteArray& rawLine : qmldir.readAll().split('\n')) {
        const QList<QByteArray> fields =
                rawLine.simplified().split(' ');
        if (fields.size() == 2
                && fields.constFirst() == QByteArrayLiteral("module")) {
            *module = QString::fromUtf8(fields.constLast());
        }
        else if (fields.size() == 2
                 && fields.constFirst()
                    == QByteArrayLiteral("classname")) {
            *className = QString::fromUtf8(fields.constLast());
        }
    }
    if (module->isEmpty() || className->isEmpty()) {
        *error = QStringLiteral(
                    "plugin qmldir has no module/classname identity");
        return false;
    }
    return true;
}

bool verifyPluginMetadataIdentity(const QString& pluginPath,
                                  QString* error)
{
    QString expectedModule;
    QString expectedClassName;
    if (!readQmldirIdentity(pluginPath,
                            &expectedModule,
                            &expectedClassName,
                            error)) {
        return false;
    }

    const QJsonObject metadata =
            QPluginLoader(pluginPath).metaData();
    if (metadata.isEmpty()) {
        *error = QStringLiteral("invalid Qt plugin metadata");
        return false;
    }
    if (metadata.value(QStringLiteral("IID")).toString()
            != QStringLiteral(
                "org.qt-project.Qt.QQmlEngineExtensionInterface")) {
        *error = QStringLiteral(
                    "unexpected QML extension plugin IID");
        return false;
    }
    if (metadata.value(QStringLiteral("className")).toString()
            != expectedClassName) {
        *error = QStringLiteral(
                    "plugin class does not match qmldir");
        return false;
    }

    const QJsonValue uriValue =
            metadata.value(QStringLiteral("uri"));
    const bool moduleMatches =
            uriValue.isString()
            ? uriValue.toString() == expectedModule
            : uriValue.toArray().contains(expectedModule);
    if (!moduleMatches) {
        *error = QStringLiteral(
                    "plugin URI does not match qmldir module");
        return false;
    }
    return true;
}

} // namespace

QStringList PlasmaStyleProbe::systemImportRoots()
{
    QStringList roots;
    const QString executable =
            QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget();
    if (!executable.isEmpty()) {
        roots.append(QFileInfo(executable).absolutePath());
    }
    appendEnvironmentRoots(roots, qgetenv("QML2_IMPORT_PATH"));
    appendEnvironmentRoots(roots, qgetenv("QML_IMPORT_PATH"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    roots.append(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
#else
    roots.append(QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath));
#endif

    QStringList uniqueRoots;
    for (const QString& root : roots) {
        const QString cleaned = QDir::cleanPath(root);
        if (!cleaned.isEmpty() && !uniqueRoots.contains(cleaned)) {
            uniqueRoots.append(cleaned);
        }
    }
    return uniqueRoots;
}

PlasmaStyleProbeResult PlasmaStyleProbe::inspect(
        const QStringList& importRoots,
        const PluginVerifier& pluginVerifier,
        const ImportProbeVerifier& importProbeVerifier)
{
    PlasmaStyleProbeResult result;
    if (!pluginVerifier) {
        result.failureReason =
                QStringLiteral("no plugin verifier was supplied");
        return result;
    }
    if (!importProbeVerifier) {
        result.failureReason =
                QStringLiteral("no full import verifier was supplied");
        return result;
    }

    QString modulePath;
    for (const QString& root : importRoots) {
        const QString candidate =
                QDir(root).filePath(QStringLiteral("org/kde/desktop"));
        if (QFileInfo::exists(
                    QDir(candidate).filePath(QStringLiteral("qmldir")))) {
            modulePath = QDir::cleanPath(candidate);
            break;
        }
    }

    if (modulePath.isEmpty()) {
        result.failureReason =
                QStringLiteral("org.kde.desktop module was not found");
        return result;
    }
    result.modulePath = modulePath;

    const QString moduleQmldir =
            QDir(modulePath).filePath(QStringLiteral("qmldir"));
    QFile qmldirFile(moduleQmldir);
    if (!qmldirFile.open(QIODevice::ReadOnly)) {
        result.failureReason =
                QStringLiteral("org.kde.desktop qmldir could not be read");
        return result;
    }
    const QList<QByteArray> qmldirLines =
            qmldirFile.readAll().split('\n');

    const QStringList requiredControls {
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
    for (const QString& control : requiredControls) {
        if (!QFileInfo(QDir(modulePath).filePath(control)).isFile()) {
            result.failureReason =
                    QStringLiteral("org.kde.desktop is missing %1")
                    .arg(control);
            return result;
        }

        const QByteArray typeName =
                QFileInfo(control).completeBaseName().toUtf8();
        const QByteArray fileName = control.toUtf8();
        bool mapped = false;
        for (const QByteArray& rawLine : qmldirLines) {
            const QByteArray line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith('#')) {
                continue;
            }
            const QList<QByteArray> fields =
                    line.simplified().split(' ');
            qsizetype typeIndex = 0;
            if (!fields.isEmpty()
                    && (fields.constFirst()
                        == QByteArrayLiteral("singleton")
                        || fields.constFirst()
                        == QByteArrayLiteral("internal"))) {
                typeIndex = 1;
            }
            if (fields.size() > typeIndex
                    && fields.at(typeIndex) == typeName
                    && fields.constLast() == fileName) {
                mapped = true;
                break;
            }
        }
        if (!mapped) {
            result.failureReason =
                    QStringLiteral(
                        "org.kde.desktop qmldir does not map %1")
                    .arg(control);
            return result;
        }
    }

    const QString qmlRoot =
            QDir(modulePath).absoluteFilePath(QStringLiteral("../../.."));
    const QStringList pluginPaths {
        QDir(modulePath).filePath(
                    QStringLiteral("liborg_kde_desktop.so")),
        QDir(qmlRoot).filePath(
                    QStringLiteral("org/kde/desktop/private/"
                                   "liborg_kde_desktop_private.so")),
        QDir(qmlRoot).filePath(
                    QStringLiteral("org/kde/qqc2desktopstyle/private/"
                                   "libqqc2desktopstyleplugin.so")),
    };
    const QStringList dependencyQmldirs {
        QDir(qmlRoot).filePath(
                    QStringLiteral("org/kde/desktop/private/qmldir")),
        QDir(qmlRoot).filePath(
                    QStringLiteral(
                        "org/kde/qqc2desktopstyle/private/qmldir")),
    };
    for (const QString& qmldir : dependencyQmldirs) {
        if (!QFileInfo(qmldir).isFile()) {
            result.failureReason =
                    QStringLiteral("org.kde.desktop dependency is missing %1")
                    .arg(qmldir);
            return result;
        }
    }

    for (const QString& pluginPath : pluginPaths) {
        if (!QFileInfo(pluginPath).isFile()) {
            result.failureReason =
                    QStringLiteral("org.kde.desktop plugin is missing %1")
                    .arg(pluginPath);
            return result;
        }

        QString pluginError;
        if (!pluginVerifier(pluginPath, &pluginError)) {
            result.failureReason =
                    QStringLiteral("org.kde.desktop plugin failed validation: "
                                   "%1 (%2)")
                    .arg(pluginPath, pluginError);
            return result;
        }
    }

    QString importError;
    if (!importProbeVerifier(&importError)) {
        result.failureReason =
                QStringLiteral("org.kde.desktop full import failed: %1")
                .arg(importError);
        return result;
    }

    result.available = true;
    return result;
}

PlasmaStyleProbeResult PlasmaStyleProbe::inspectWithPluginMetadata(
        const QStringList& importRoots,
        const ImportProbeVerifier& importProbeVerifier)
{
    // Do not load any KDE plugin into the fallback-capable parent. Qt plugins
    // can contain GNU-unique symbols that keep their DSOs resident even after
    // QPluginLoader::unload() succeeds. The disposable child below is the only
    // process that performs actual loading and QML registration.
    return inspect(
                importRoots,
                [](const QString& path, QString* error) {
        return verifyPluginMetadataIdentity(path, error);
    },
    importProbeVerifier);
}

PlasmaStyleProbeResult PlasmaStyleProbe::inspectSystem()
{
    return inspectWithPluginMetadata(
                systemImportRoots(),
                verifyStyleImportInSubprocess);
}

#if defined(QT_WIDGETS_LIB)
bool PlasmaStyleProbe::isStyleImportProbeRequest(int argc, char* argv[])
{
    return argc == 2
            && QByteArray(argv[1]) == QByteArray(kStyleImportProbeArgument);
}

int PlasmaStyleProbe::runStyleImportProbe(int argc, char* argv[])
{
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    QApplication application(argc, argv);

    static const QByteArray probeQml = R"QML(
import QtQuick 2.15
import QtQuick.Controls 2.15
import org.kde.kirigami 2.20 as Kirigami

ApplicationWindow {
    visible: false
    Kirigami.FormLayout {}
    Kirigami.Heading {}
    Kirigami.Icon {}
    Kirigami.InlineMessage {}
    Kirigami.PlaceholderMessage {}
    Kirigami.Separator {}
    AbstractButton {}
    Button {}
    CheckBox {}
    ComboBox {}
    Dialog {}
    DialogButtonBox {}
    Frame {}
    ItemDelegate {}
    Label {}
    Menu { MenuItem {} }
    Page {}
    Pane {}
    Popup {}
    ScrollBar {}
    Slider {}
    StackView {}
    Switch {}
    TextField {}
    ToolButton {}
    ToolTip {}
    BusyIndicator {}
}
)QML";

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
                probeQml,
                QUrl(QStringLiteral(
                    "qrc:/starlight/plasma-style-import-probe.qml")));
    if (component.isError()) {
        qCritical().noquote()
                << component.errorString();
        return 1;
    }

    std::unique_ptr<QObject> probe(component.create());
    if (!probe) {
        qCritical().noquote()
                << component.errorString();
        return 1;
    }
    return 0;
}
#endif
