#include "gnomechromeselector.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QStringList>

namespace {

bool sessionHasToken(
        const QString& activeSession,
        const QStringList& acceptedPrefixes)
{
    const QStringList tokens = activeSession.toUpper().split(
            QRegularExpression(QStringLiteral("[:;]")),
            Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        const QString normalized = token.trimmed();
        for (const QString& prefix : acceptedPrefixes) {
            if (normalized == prefix
                    || normalized.startsWith(prefix + QLatin1Char('-'))) {
                return true;
            }
        }
    }
    return false;
}

bool predictsWaylandSession()
{
    const QString requestedPlatform =
            qEnvironmentVariable("QT_QPA_PLATFORM").trimmed();
    if (!requestedPlatform.isEmpty()) {
        const QString primaryPlatform =
                requestedPlatform.split(QLatin1Char(';')).constFirst()
                        .trimmed().toLower();
        return primaryPlatform.startsWith(QStringLiteral("wayland"));
    }

    const QString sessionType =
            qEnvironmentVariable("XDG_SESSION_TYPE").trimmed();
    if (!sessionType.isEmpty()) {
        return sessionType.compare(
                    QStringLiteral("wayland"),
                    Qt::CaseInsensitive) == 0;
    }

    return !qEnvironmentVariable("WAYLAND_DISPLAY").trimmed().isEmpty();
}

} // namespace

GnomeChromeInputs GnomeChromeSelector::inspectSystem()
{
    GnomeChromeInputs inputs;
    inputs.explicitDecoration =
            qEnvironmentVariableIsSet("QT_WAYLAND_DECORATION");
    inputs.waylandSession = predictsWaylandSession();
    inputs.activeSession = QStringLiteral("%1:%2")
            .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                 qEnvironmentVariable("XDG_SESSION_DESKTOP"));

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QStringList pluginRoots {
        QLibraryInfo::path(QLibraryInfo::PluginsPath),
    };
    for (const QByteArray& path :
         qgetenv("QT_PLUGIN_PATH").split(':')) {
        if (!path.isEmpty()) {
            pluginRoots.append(QString::fromLocal8Bit(path));
        }
    }
    pluginRoots.removeDuplicates();

    const QStringList nameFilters {
        QStringLiteral("*adwaita*.so"),
        QStringLiteral("*gnome*decoration*.so"),
    };
    for (const QString& root : pluginRoots) {
        const QDir directory(
                root + QStringLiteral("/wayland-decoration-client"));
        if (!directory.exists()) {
            continue;
        }

        for (const QString& file :
             directory.entryList(nameFilters, QDir::Files)) {
            QPluginLoader loader(directory.absoluteFilePath(file));
            const QJsonObject metadata =
                    loader.metaData().value(
                        QStringLiteral("MetaData")).toObject();
            const QJsonArray keys =
                    metadata.value(QStringLiteral("Keys")).toArray();

            GnomeChromePlugin plugin;
            plugin.adwaitaKey =
                    keys.contains(QJsonValue(QStringLiteral("adwaita")));
            plugin.qAdwaitaKey =
                    keys.contains(
                        QJsonValue(
                            QStringLiteral("qadwaitadecorations")));
            if (!plugin.adwaitaKey && !plugin.qAdwaitaKey) {
                continue;
            }

            plugin.loadable = loader.load();
            if (plugin.loadable) {
                loader.unload();
            }
            inputs.plugins.append(plugin);
        }
    }
#endif

    return inputs;
}

GnomeChromeDecision GnomeChromeSelector::select(
        const GnomeChromeInputs& inputs)
{
    if (inputs.explicitDecoration) {
        return { GnomeChromeDecision::PreserveExplicit, {} };
    }
    if (!inputs.waylandSession) {
        return { GnomeChromeDecision::QtDefault, {} };
    }

    const bool isGnome = sessionHasToken(
            inputs.activeSession,
            { QStringLiteral("GNOME"), QStringLiteral("UNITY") });
    const bool isKde = sessionHasToken(
            inputs.activeSession,
            { QStringLiteral("KDE"), QStringLiteral("PLASMA") });
    if (!isGnome || isKde) {
        return { GnomeChromeDecision::QtDefault, {} };
    }

    for (const GnomeChromePlugin& plugin : inputs.plugins) {
        if (plugin.qAdwaitaKey && plugin.loadable) {
            return {
                GnomeChromeDecision::QAdwaita,
                QByteArrayLiteral("qadwaitadecorations"),
            };
        }
    }

    for (const GnomeChromePlugin& plugin : inputs.plugins) {
        if (plugin.adwaitaKey && plugin.loadable) {
            return {
                GnomeChromeDecision::Adwaita,
                QByteArrayLiteral("adwaita"),
            };
        }
    }

    return { GnomeChromeDecision::QtDefault, {} };
}
