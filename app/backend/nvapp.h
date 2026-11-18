#pragma once

#include <QSettings>
#include <QUuid>
#include <QVector>

class NvApp
{
public:
    NvApp() {}
    explicit NvApp(QSettings& settings);

    bool operator==(const NvApp& other) const
    {
        return id == other.id &&
                name == other.name &&
                uuid == other.uuid &&
                hdrSupported == other.hdrSupported &&
                isAppCollectorGame == other.isAppCollectorGame &&
                hidden == other.hidden &&
                directLaunch == other.directLaunch;
    }

    bool operator!=(const NvApp& other) const
    {
        return !operator==(other);
    }

    // Vibepollo can advertise the running app a second time under the same
    // title. Its stable UUID identifies a join/resume action, not a new app.
    bool isRunningAppShortcut() const
    {
        return QUuid(uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000007"));
    }

    bool isRemoteMonitor() const
    {
        return QUuid(uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000005"));
    }

    static bool ownsRemoteMonitor(const QVector<NvApp>& apps)
    {
        bool disconnect = false, resume = false;
        for (const auto& app : apps) {
            disconnect |= QUuid(app.uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000002"));
            resume |= QUuid(app.uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000001"));
        }
        return disconnect && resume;
    }

    static NvApp catalogueRole(const QVector<NvApp>& apps, int role)
    {
        NvApp result;
        const QUuid identity(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f0000000") + QString::number(role));
        for (const auto& app : apps) if (QUuid(app.uuid) == identity) {
            if (result.isInitialized() || !app.isInitialized() || app.id <= 0) return {};
            result = app;
        }
        return result;
    }

    static bool hasForeignDesktop(const QVector<NvApp>& apps)
    {
        int desktops = 0;
        NvApp target;
        for (const auto& app : apps) if (app.isDesktopApp()) { ++desktops; target = app; }
        const auto running = catalogueRole(apps, 7);
        return desktops == 1 && !QUuid(target.uuid).isNull() && running.isInitialized() &&
            running.name.trimmed().compare(target.name.trimmed(), Qt::CaseInsensitive) == 0;
    }

    static NvApp mainDesktopTarget(const QVector<NvApp>& apps)
    {
        return desktopLaunchTarget(apps, false);
    }

    static NvApp desktopLaunchTarget(const QVector<NvApp>& apps, bool privateDisplay)
    {
        const auto app = desktopTarget(apps, privateDisplay);
        // Let the connection worker release this client's retained Monitor
        // before switching to physical/host mode, then resolve a fresh app.
        return !app.isInitialized() && ownsRemoteMonitor(apps) ? desktopTarget(apps, true) : app;
    }

    static NvApp desktopTarget(const QVector<NvApp>& apps, bool privateDisplay)
    {
        if (privateDisplay) {
            bool retainedMonitor = false, resume = false;
            for (const auto& app : apps) {
                if (app.isRemoteMonitor()) return app;
                retainedMonitor |= QUuid(app.uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000002"));
                resume |= QUuid(app.uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000001"));
            }
            if (retainedMonitor && resume) {
                // This paired client's retained Monitor catalogue advertises
                // Resume/Disconnect only. Monitor activation is idempotent.
                NvApp app; app.id = 2147483505;
                app.uuid = QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000005");
                app.name = QStringLiteral("Remote Monitor");
                return app;
            }
        }
        for (const auto& app : apps) if (app.isDesktopApp()) return app;
        return {};
    }

    bool isDesktopApp() const
    {
        return !isRunningAppShortcut() &&
               name.trimmed().compare(QStringLiteral("Desktop"), Qt::CaseInsensitive) == 0;
    }

    bool isDesktopTarget() const
    {
        return isRemoteMonitor() || isDesktopApp() || (!isRunningAppShortcut() &&
               name.trimmed().compare(QStringLiteral("Remote Monitor"), Qt::CaseInsensitive) == 0);
    }

    bool matchesRunningTarget(int currentId, const QString& currentUuid = QString()) const
    {
        if (currentId <= 0) return false;
        const QUuid current(currentUuid), expected(uuid);
        return !current.isNull() && !expected.isNull() ? current == expected : currentId == id;
    }

    bool isInitialized() const
    {
        // We use isNull() instead of isEmpty() here because we want
        // to detect cases where the name is unassigned, not empty.
        return id != 0 && !name.isNull();
    }

    void
    serialize(QSettings& settings) const;

    int id = 0;
    QString name;
    QString uuid;
    bool hdrSupported = false;
    bool isAppCollectorGame = false;
    bool hidden = false;
    bool directLaunch = false;
};

Q_DECLARE_METATYPE(NvApp)
