#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonDocument>
#include <QPair>
#include <QPointer>
#include <QVariantList>
#include <atomic>
#include <QUuid>

#include "gui/hostidentitysource.h"

class TestSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hostUuid READ hostUuid CONSTANT)

public:
    TestSession(const QString& hostUuid, int* releaseCount, QObject* parent)
        : QObject(parent), m_HostUuid(hostUuid), m_ReleaseCount(releaseCount)
    {
    }

    QString hostUuid() const { return m_HostUuid; }
    Q_INVOKABLE void release()
    {
        ++(*m_ReleaseCount);
    }

private:
    QString m_HostUuid;
    int* m_ReleaseCount;
};

class TestHostSource : public QAbstractListModel,
                       public HostIdentitySource
{
    Q_OBJECT
    Q_PROPERTY(int pairDispatchCount READ pairDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int pairComputerCalls READ pairDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int lastPairComputerIndex READ lastPairComputerIndex NOTIFY recorderChanged)
    Q_PROPERTY(QString lastPairComputerPin READ lastPairComputerPin NOTIFY recorderChanged)
    Q_PROPERTY(QObject* pairingStateSource MEMBER m_PairingStateSource)
    Q_PROPERTY(QVariantList pendingPairingAttemptsAtDispatch
               READ pendingPairingAttemptsAtDispatch NOTIFY recorderChanged)
    Q_PROPERTY(int connectionTestDispatchCount READ connectionTestDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int wakeDispatchCount READ wakeDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int addDispatchCount READ addDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int quitDispatchCount READ quitDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(int lastQuitIndex READ lastQuitIndex NOTIFY recorderChanged)
    Q_PROPERTY(int sessionDispatchCount READ sessionDispatchCount NOTIFY recorderChanged)
    Q_PROPERTY(bool lastDesktopTakeover READ lastDesktopTakeover NOTIFY recorderChanged)
    Q_PROPERTY(int sessionReleaseCount READ sessionReleaseCount NOTIFY recorderChanged)
    Q_PROPERTY(QObject* sessionOverride READ sessionOverride WRITE setSessionOverride
               NOTIFY recorderChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        WakeableRole,
        StatusUnknownRole,
        ServerSupportedRole,
        DetailsRole,
        AddressRole,
        HostOsRole,
        ServerSoftwareRole,
        ServerSoftwareLabelRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_Hosts.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_Hosts.size()) {
            return {};
        }
        const QVariantMap& host = m_Hosts.at(index.row());
        switch (role) {
        case NameRole: return host.value(QStringLiteral("name"), QString());
        case OnlineRole: return host.value(QStringLiteral("online"), false);
        case PairedRole: return host.value(QStringLiteral("paired"), false);
        case BusyRole: return host.value(QStringLiteral("busy"), false);
        case WakeableRole: return host.value(QStringLiteral("wakeable"), false);
        case StatusUnknownRole: return host.value(QStringLiteral("statusUnknown"), false);
        case ServerSupportedRole: return host.value(QStringLiteral("serverSupported"), true);
        case DetailsRole: return host.value(QStringLiteral("details"), QString());
        case AddressRole: return host.value(QStringLiteral("address"), QString());
        case HostOsRole: return host.value(QStringLiteral("hostOs"), QString());
        case ServerSoftwareRole: return host.value(QStringLiteral("serverSoftware"), QString());
        case ServerSoftwareLabelRole: return host.value(QStringLiteral("serverSoftwareLabel"), QString());
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            { NameRole, "name" },
            { OnlineRole, "online" },
            { PairedRole, "paired" },
            { BusyRole, "busy" },
            { WakeableRole, "wakeable" },
            { StatusUnknownRole, "statusUnknown" },
            { ServerSupportedRole, "serverSupported" },
            { DetailsRole, "details" },
            { AddressRole, "address" },
            { HostOsRole, "hostOs" },
            { ServerSoftwareRole, "serverSoftware" },
            { ServerSoftwareLabelRole, "serverSoftwareLabel" }
        };
    }

    QString uuidForRow(int row) const override
    {
        return row >= 0 && row < m_Hosts.size() ? m_Hosts.at(row).value("uuid").toString()
                                                : QString();
    }

    Q_INVOKABLE QString uuidForIndex(int row) const { return uuidForRow(row); }
    Q_INVOKABLE QString hostIdentityForIndex(int row) const
    {
        return m_HostIdentities.value(uuidForRow(row));
    }
    Q_INVOKABLE QString nameForIndex(int row) const
    {
        return row >= 0 && row < m_Hosts.size()
                ? m_Hosts.at(row).value(QStringLiteral("name")).toString()
                : QString();
    }
    Q_INVOKABLE QString serverSoftwareForIndex(int row) const
    {
        return row >= 0 && row < m_Hosts.size()
                ? m_Hosts.at(row).value(QStringLiteral("serverSoftware")).toString()
                : QString();
    }
    Q_INVOKABLE QVariantMap presentationForIndex(int row) const
    {
        if (row < 0 || row >= m_Hosts.size()) return {};
        QVariantMap snapshot = m_Hosts.at(row);
        snapshot.insert(QStringLiteral("uuid"), uuidForRow(row));
        snapshot.insert(QStringLiteral("name"),
                        snapshot.value(QStringLiteral("name"), QString()));
        snapshot.insert(QStringLiteral("online"),
                        snapshot.value(QStringLiteral("online"), false));
        snapshot.insert(QStringLiteral("paired"),
                        snapshot.value(QStringLiteral("paired"), false));
        snapshot.insert(QStringLiteral("busy"),
                        snapshot.value(QStringLiteral("busy"), false));
        snapshot.insert(QStringLiteral("wakeable"),
                        snapshot.value(QStringLiteral("wakeable"), false));
        snapshot.insert(QStringLiteral("statusUnknown"),
                        snapshot.value(QStringLiteral("statusUnknown"), false));
        snapshot.insert(QStringLiteral("serverSupported"),
                        snapshot.value(QStringLiteral("serverSupported"), true));
        snapshot.insert(QStringLiteral("details"),
                        snapshot.value(QStringLiteral("details"), QString()));
        snapshot.insert(QStringLiteral("address"),
                        snapshot.value(QStringLiteral("address"), QString()));
        snapshot.insert(QStringLiteral("hostOs"),
                        snapshot.value(QStringLiteral("hostOs"), QString()));
        snapshot.insert(QStringLiteral("serverSoftware"),
                        snapshot.value(QStringLiteral("serverSoftware"), QString()));
        return snapshot;
    }

    Q_INVOKABLE void updateHost(const QString& uuid, const QVariantMap& values)
    {
        for (int row = 0; row < m_Hosts.size(); ++row) {
            if (uuidForRow(row) != uuid) continue;
            for (auto it = values.cbegin(); it != values.cend(); ++it) m_Hosts[row].insert(it.key(), it.value());
            emit dataChanged(index(row), index(row));
            return;
        }
    }
    Q_INVOKABLE void replaceHosts(const QVariantList& hosts)
    {
        beginResetModel();
        m_Hosts.clear();
        m_HostIdentities.clear();
        for (const QVariant& host : hosts) {
            m_Hosts.append(host.toMap());
            m_HostIdentities.insert(host.toMap().value("uuid").toString(),
                                   QUuid::createUuid().toString());
        }
        endResetModel();
    }
    Q_INVOKABLE void reorderHosts(const QStringList& uuids)
    {
        QList<QVariantMap> reordered;
        for (const QString& uuid : uuids) {
            const int row = indexForUuid(uuid);
            if (row < 0) return;
            reordered.append(m_Hosts.at(row));
        }
        if (reordered.size() != m_Hosts.size()) return;
        beginResetModel();
        m_Hosts = reordered;
        endResetModel();
    }
    Q_INVOKABLE void appendHost(const QVariantMap& host)
    {
        const int row = m_Hosts.size();
        beginInsertRows({}, row, row);
        m_Hosts.append(host);
        m_HostIdentities.insert(host.value("uuid").toString(), QUuid::createUuid().toString());
        endInsertRows();
    }

    int pairDispatchCount() const { return m_PairDispatchCount; }
    int lastPairComputerIndex() const { return m_LastPairIndex; }
    QString lastPairComputerPin() const { return m_LastPairPin; }
    QVariantList pendingPairingAttemptsAtDispatch() const
    {
        return m_PendingPairingAttemptsAtDispatch;
    }
    int connectionTestDispatchCount() const { return m_ConnectionTestDispatchCount; }
    int wakeDispatchCount() const { return m_WakeDispatchCount; }
    int addDispatchCount() const { return m_AddDispatchCount; }
    int quitDispatchCount() const { return m_QuitDispatchCount; }
    int lastQuitIndex() const { return m_LastQuitIndex; }
    int sessionDispatchCount() const { return m_SessionDispatchCount; }
    bool lastDesktopTakeover() const { return m_LastDesktopTakeover; }
    int sessionReleaseCount() const { return m_SessionReleaseCount; }
    QObject* sessionOverride() const { return m_SessionOverride.data(); }

    Q_INVOKABLE QString generatePinString() const { return QStringLiteral("1234"); }
    Q_INVOKABLE int reservePairingAttemptId()
    {
        static std::atomic<int> nextId {1};
        return nextId.fetch_add(1, std::memory_order_relaxed);
    }
    Q_INVOKABLE void pairComputer(int index, const QString& pin, int attemptId)
    {
        const QVariant pending = m_PairingStateSource
                ? m_PairingStateSource->property("pendingPairingAttempts")
                : QVariant();
        m_PendingPairingAttemptsAtDispatch.append(
                    QString::fromUtf8(QJsonDocument::fromVariant(pending)
                                      .toJson(QJsonDocument::Compact)));
        m_LastPairIndex = index;
        m_LastPairPin = pin;
        m_PairingAttempts.append(attemptId);
        ++m_PairDispatchCount;
        emit recorderChanged();
    }
    Q_INVOKABLE void completePairing(const QVariant& error = QVariant())
    {
        if (!m_PairingAttempts.isEmpty())
            emit pairingCompleted(m_PairingAttempts.takeFirst(), error);
    }
    Q_INVOKABLE void completePairingAttempt(int attemptId, const QVariant& error = QVariant())
    {
        if (m_PairingAttempts.removeOne(attemptId))
            emit pairingCompleted(attemptId, error);
    }
    Q_INVOKABLE int reserveConnectionTestAttemptId() { static int id = 1; return id++; }
    Q_INVOKABLE void cancelConnectionTest(int) {}
    Q_INVOKABLE void testConnectionForComputer(int, int attemptId = -1)
    {
        m_ConnectionAttempt = attemptId;
        ++m_ConnectionTestDispatchCount;
        emit recorderChanged();
    }
    Q_INVOKABLE void completeConnectionTest(int result, const QString& blockedPorts = QString())
    {
        emit connectionDiagnosticCompleted(m_ConnectionAttempt, result, blockedPorts);
    }
    Q_INVOKABLE QString wakeComputer(int index)
    {
        ++m_WakeDispatchCount;
        m_LastWakeIndex = index;
        emit recorderChanged();
        return QString();
    }
    Q_INVOKABLE void completeWake(int index, const QString& error = QString()) { emit wakeCompleted(index, error); }
    Q_INVOKABLE void emitWakeError(const QString& uuid, const QString& error = QStringLiteral("wake failed"))
    {
        emit wakeCompleted(indexForUuid(uuid), error);
    }
    Q_INVOKABLE void emitAsyncError(const QString& message, const QString& uuid = QString())
    {
        emit asyncError(message, uuid);
    }
    Q_INVOKABLE void addHost(const QString&, int)
    {
        ++m_AddDispatchCount;
        emit recorderChanged();
    }
    Q_INVOKABLE void completeAdd(bool success, bool blocked = false) { emit addCompleted(success, blocked); }
    Q_INVOKABLE void setRunningApp(const QString& uuid, int appId, const QString& name)
    {
        m_RunningApps.insert(uuid, { appId, name });
        for (int row = 0; row < m_Hosts.size(); ++row)
            if (uuidForRow(row) == uuid) emit dataChanged(index(row), index(row));
    }
    Q_INVOKABLE void setSessionAvailable(bool available) { m_SessionAvailable = available; }
    Q_INVOKABLE void setSessionOverride(QObject* session)
    {
        if (m_SessionOverride == session) return;
        m_SessionOverride = session;
        emit recorderChanged();
    }
    Q_INVOKABLE void setDesktopAppId(int appId) { m_DesktopAppId = appId; }
    Q_INVOKABLE int getDesktopAppId(int) const { return m_DesktopAppId; }
    Q_INVOKABLE int getRunningAppId(int index) const
    {
        return m_RunningApps.value(uuidForRow(index)).first;
    }
    Q_INVOKABLE QString getRunningAppName(int index) const
    {
        return m_RunningApps.value(uuidForRow(index)).second;
    }
    Q_INVOKABLE QObject* createSessionForDesktop(int index, bool = false, bool desktopTakeover = false)
    {
        m_LastDesktopTakeover = desktopTakeover;
        ++m_SessionDispatchCount;
        emit recorderChanged();
        if (!m_SessionAvailable || uuidForRow(index).isEmpty()) return nullptr;
        if (m_SessionOverride) return m_SessionOverride.data();
        return new TestSession(uuidForRow(index), &m_SessionReleaseCount, this);
    }
    Q_INVOKABLE QObject* createSessionForCurrentGame(int index) { return createSessionForDesktop(index); }
    Q_INVOKABLE void quitRunningApp(int index)
    {
        ++m_QuitDispatchCount;
        m_LastQuitIndex = index;
        emit recorderChanged();
    }
    Q_INVOKABLE void renameComputer(int row, const QString& name)
    {
        if (row < 0 || row >= m_Hosts.size()) return;
        m_Hosts[row].insert(QStringLiteral("name"), name);
        emit dataChanged(index(row, 0), index(row, 0), { NameRole });
    }
    Q_INVOKABLE void deleteComputer(int row)
    {
        if (row < 0 || row >= m_Hosts.size()) return;
        beginRemoveRows({}, row, row);
        m_HostIdentities.remove(uuidForRow(row));
        m_Hosts.removeAt(row);
        endRemoveRows();
    }

signals:
    void pairingCompleted(int attemptId, QVariant error);
    void connectionTestCompleted(int result, QString blockedPorts);
    void connectionDiagnosticCompleted(int attemptId, int result, QString blockedPorts);
    void wakeCompleted(int index, QString error);
    void addCompleted(bool success, bool blocked);
    void asyncError(QString message, QString uuid);
    void recorderChanged();

private:
    QObject* m_PairingStateSource = nullptr;
    QVariantList m_PendingPairingAttemptsAtDispatch;
    int indexForUuid(const QString& uuid) const
    {
        for (int row = 0; row < m_Hosts.size(); ++row) {
            if (m_Hosts.at(row).value(QStringLiteral("uuid")).toString() == uuid) return row;
        }
        return -1;
    }
    QList<QVariantMap> m_Hosts;
    QHash<QString, QString> m_HostIdentities;
    int m_PairDispatchCount = 0;
    QList<int> m_PairingAttempts;
    int m_ConnectionTestDispatchCount = 0;
    int m_ConnectionAttempt = -1;
    int m_WakeDispatchCount = 0;
    int m_AddDispatchCount = 0;
    int m_QuitDispatchCount = 0;
    int m_LastQuitIndex = -1;
    int m_SessionDispatchCount = 0;
    bool m_LastDesktopTakeover = false;
    int m_SessionReleaseCount = 0;
    bool m_SessionAvailable = true;
    QPointer<QObject> m_SessionOverride;
    int m_DesktopAppId = 0;
    int m_LastPairIndex = -1;
    QString m_LastPairPin;
    int m_LastWakeIndex = -1;
    QHash<QString, QPair<int, QString>> m_RunningApps;
};
