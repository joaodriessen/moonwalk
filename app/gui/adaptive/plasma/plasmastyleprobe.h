#pragma once

#include <functional>

#include <QString>
#include <QStringList>

struct PlasmaStyleProbeResult {
    bool available = false;
    QString modulePath;
    QString failureReason;
};

class PlasmaStyleProbe final
{
public:
    using PluginVerifier =
            std::function<bool(const QString& path, QString* error)>;
    using ImportProbeVerifier =
            std::function<bool(QString* error)>;

    static QStringList systemImportRoots();
    static PlasmaStyleProbeResult inspect(
            const QStringList& importRoots,
            const PluginVerifier& pluginVerifier,
            const ImportProbeVerifier& importProbeVerifier);
    static PlasmaStyleProbeResult inspectWithPluginMetadata(
            const QStringList& importRoots,
            const ImportProbeVerifier& importProbeVerifier);
    static PlasmaStyleProbeResult inspectSystem();

#if defined(QT_WIDGETS_LIB)
    static bool isStyleImportProbeRequest(int argc, char* argv[]);
    static int runStyleImportProbe(int argc, char* argv[]);
#endif
};
