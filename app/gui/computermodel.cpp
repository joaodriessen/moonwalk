#include "computermodel.h"
#include "streaming/connectiondiagnostic.h"
#include <PlatformSockets.h>

#include <QThreadPool>
#include <QUuid>
#include <atomic>

namespace {
std::atomic<int> nextPairingAttemptId {1};
std::atomic<int> nextConnectionTestAttemptId {1};
}

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::pairingCompleted,
            this, &ComputerModel::handlePairingCompleted);

    m_Computers = m_ComputerManager->getComputers();
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->pairState == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->activeStreams >= 0 ? computer->activeStreams > 0 : computer->currentGameId != 0;
    case WakeableRole:
        return !computer->macAddress.isEmpty();
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case ServerSupportedRole:
        return computer->isSupportedServerVersion;
    case AddressRole:
        // Best-known address for display: prefer the live active address,
        // fall back to whatever we last knew this host by.
        if (!computer->activeAddress.isNull()) {
            return computer->activeAddress.toString();
        }
        else if (!computer->localAddress.isNull()) {
            return computer->localAddress.toString();
        }
        else if (!computer->manualAddress.isNull()) {
            return computer->manualAddress.toString();
        }
        else {
            return computer->remoteAddress.toString();
        }
    case HostOsRole:
        // Auto-detected only (serverinfo <HostOs> tag, or the GFE=>Windows
        // inference) -- the manual override layer was retired 2026-07-17
        // along with its Advanced-settings row: detection covers every
        // supported host family, so a stale manual choice could only shadow
        // fresher live evidence. Values are always exactly one of
        // "macos" | "windows" | "linux" | "unknown" -- never "".
        if (!computer->detectedOs.isEmpty()) {
            return computer->detectedOs;
        }
        else {
            return QStringLiteral("unknown");
        }
    case ServerSoftwareRole:
        // "Skylight" | "Sunshine" | "Apollo" | "Vibeshine" | "Vibepollo" |
        // "GeForce Experience" | "" -- see NvComputer::serverSoftware.
        return computer->serverSoftware;
    case ServerSoftwareLabelRole:
        // What to show: "Moonwalk Host (0.4.0-alpha.15)", "Apollo", ...
        return computer->serverSoftwareLabel();
    case DetailsRole: {
        QString state, pairState;

        switch (computer->state) {
        case NvComputer::CS_ONLINE:
            state = tr("Online");
            break;
        case NvComputer::CS_OFFLINE:
            state = tr("Offline");
            break;
        default:
            state = tr("Unknown");
            break;
        }

        switch (computer->pairState) {
        case NvComputer::PS_PAIRED:
            pairState = tr("Paired");
            break;
        case NvComputer::PS_NOT_PAIRED:
            pairState = tr("Unpaired");
            break;
        default:
            pairState = tr("Unknown");
            break;
        }

        QString osName;
        if (computer->detectedOs == QLatin1String("macos")) {
            osName = QStringLiteral("macOS");
        } else if (computer->detectedOs == QLatin1String("windows")) {
            osName = QStringLiteral("Windows");
        } else if (computer->detectedOs == QLatin1String("linux")) {
            osName = QStringLiteral("Linux");
        } else {
            osName = tr("Unknown");
        }

        return tr("Name: %1").arg(computer->name) + '\n' +
               tr("Status: %1").arg(state) + '\n' +
               tr("Operating System: %1").arg(osName) + '\n' +
               tr("Server Software: %1").arg(computer->serverSoftware.isEmpty() ? tr("Unknown")
                                              : computer->serverSoftwareLabel()) + '\n' +
               tr("Active Address: %1").arg(computer->activeAddress.toString()) + '\n' +
               tr("UUID: %1").arg(computer->uuid) + '\n' +
               tr("Local Address: %1").arg(computer->localAddress.toString()) + '\n' +
               tr("Remote Address: %1").arg(computer->remoteAddress.toString()) + '\n' +
               tr("IPv6 Address: %1").arg(computer->ipv6Address.toString()) + '\n' +
               tr("Manual Address: %1").arg(computer->manualAddress.toString()) + '\n' +
               tr("MAC Address: %1").arg(computer->macAddress.isEmpty() ? tr("Unknown") : QString(computer->macAddress.toHex(':'))) + '\n' +
               tr("Pair State: %1").arg(pairState) + '\n' +
               tr("Running App ID: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->currentGameId) : tr("Unknown")) + '\n' +
               tr("HTTPS Port: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->activeHttpsPort) : tr("Unknown"));
    }
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";
    names[WakeableRole] = "wakeable";
    names[StatusUnknownRole] = "statusUnknown";
    names[ServerSupportedRole] = "serverSupported";
    names[DetailsRole] = "details";
    names[AddressRole] = "address";
    names[HostOsRole] = "hostOs";
    names[ServerSoftwareRole] = "serverSoftware";
    names[ServerSoftwareLabelRole] = "serverSoftwareLabel";

    return names;
}

Session* ComputerModel::createSessionForCurrentGame(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return nullptr;
    }

    NvComputer* computer = m_Computers[computerIndex];

    // We must currently be streaming a game to use this function
    QReadLocker lock(&computer->lock);
    if (computer->currentGameId == 0) return nullptr;

    for (NvApp& app : computer->appList) {
        if (app.id == computer->currentGameId) {
            // Pin the singleton's connection-setting overlay to this launch's
            // host right before constructing the Session (design §4).
            StreamingPreferences::get()->setCurrentHost(computer->uuid);
            return new Session(computer, app);
        }
    }

    // We have a current running app but it's not in our app list
    return nullptr;
}

int ComputerModel::getDesktopAppId(int computerIndex)
{
    // Real bounds check, not Q_ASSERT: these QML-invokable indices can race a
    // background model reset, Q_ASSERT is compiled out in release builds, and
    // it never rejected negative indices in the first place.
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return -1;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    const NvApp app = NvApp::mainDesktopTarget(computer->appList);
    return app.isInitialized() ? app.id : -1;
}

Session* ComputerModel::createSessionForDesktop(int computerIndex, bool safeSettings, bool desktopTakeover)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return nullptr;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    NvApp app = NvApp::mainDesktopTarget(computer->appList);
    if (!app.isInitialized()) return nullptr;
    StreamingPreferences::get()->setCurrentHost(computer->uuid);
    return new Session(computer, app, nullptr, safeSettings, true, desktopTakeover);
}

int ComputerModel::getRunningAppId(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return 0;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    return computer->currentGameId;
}

QString ComputerModel::getRunningAppName(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    if (computer->currentGameId != 0) {
        for (const NvApp& app : std::as_const(computer->appList)) {
            if (app.id == computer->currentGameId) {
                return app.name;
            }
        }
    }

    return QString();
}

void ComputerModel::quitRunningApp(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return;
    }

    m_ComputerManager->quitRunningApp(m_Computers[computerIndex]);
}

void ComputerModel::deleteComputer(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return;
    }

    QSharedPointer<NvComputer> owner = m_Computers[computerIndex]->retain();
    beginRemoveRows(QModelIndex(), computerIndex, computerIndex);

    // m_Computer[computerIndex] will be deleted by this call
    m_ComputerManager->deleteHost(owner.data());

    // Remove the now invalid item
    m_Computers.removeAt(computerIndex);

    endRemoveRows();
}

class DeferredWakeHostTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    DeferredWakeHostTask(int computerIndex, NvComputer* computer)
        : m_ComputerIndex(computerIndex), m_ComputerOwner(computer->retain()),
          m_Computer(computer), m_ComputerName(computer->name) {}

    void run()
    {
        bool success = m_Computer->wake();
        emit wakeCompleted(m_ComputerIndex,
                            success ? QString()
                                    : tr("Failed to send wake packet to %1").arg(m_ComputerName));
    }

signals:
    void wakeCompleted(int computerIndex, QString error);

private:
    int m_ComputerIndex;
    QSharedPointer<NvComputer> m_ComputerOwner;
    NvComputer* m_Computer;
    QString m_ComputerName;
};

QString ComputerModel::wakeComputer(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        Q_ASSERT(false);
        return tr("Invalid host selection");
    }

    NvComputer* computer = m_Computers[computerIndex];

    // Fast, network-free pre-checks so obviously bad requests get feedback
    // immediately, without waiting on the deferred worker (see the
    // wakeComputer() declaration in computermodel.h for why the actual send
    // can't happen synchronously here).
    {
        QReadLocker lock(&computer->lock);

        if (computer->state == NvComputer::CS_ONLINE) {
            // Nothing to send -- matches NvComputer::wake()'s own
            // "already online" short-circuit (treated as success there too).
            lock.unlock();
            emit wakeCompleted(computerIndex, QString());
            return QString();
        }
        if (computer->macAddress.isEmpty()) {
            return tr("No MAC address is known for %1").arg(computer->name);
        }
    }

    DeferredWakeHostTask* wakeTask = new DeferredWakeHostTask(computerIndex, computer);
    connect(wakeTask, &DeferredWakeHostTask::wakeCompleted,
            this, &ComputerModel::wakeCompleted);
    QThreadPool::globalInstance()->start(wakeTask);

    // Real success/failure of the send arrives later via wakeCompleted().
    return QString();
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return;
    }

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

void ComputerModel::setHostOs(int computerIndex, const QString& os)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return;
    }

    // ComputerManager::setHostOs() -> handleComputerStateChanged() emits
    // computerStateChanged(computer), which our own handleComputerStateChanged
    // slot (below) turns into the row's dataChanged() -- same wiring
    // renameComputer() already relies on for its UI update.
    m_ComputerManager->setHostOs(m_Computers[computerIndex], os);
}

QString ComputerModel::uuidForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    return computer->uuid;
}

QString ComputerModel::hostIdentityForIndex(int computerIndex)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.size()) return {};
    const auto owner = m_Computers[computerIndex]->retain();
    if (!owner) return {};
    QReadLocker lock(&owner->lock);
    auto& identity = m_HostIdentities[owner->uuid];
    if (identity.token.isEmpty() || identity.owner.toStrongRef() != owner) {
        identity.owner = owner;
        identity.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    return identity.token;
}

QString ComputerModel::uuidForRow(int row) const
{
    return uuidForIndex(row);
}

QString ComputerModel::nameForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    return computer->name;
}

QString ComputerModel::serverSoftwareForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    // "Skylight" | "Sunshine" | "GeForce Experience" | "" -- see
    // NvComputer::serverSoftware / ServerSoftwareRole above.
    return computer->serverSoftware;
}

QVariantMap ComputerModel::presentationForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return {};
    }

    const QModelIndex modelIndex = index(computerIndex, 0);
    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("uuid"), uuidForIndex(computerIndex));
    const auto roles = roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        snapshot.insert(QString::fromUtf8(it.value()), data(modelIndex, it.key()));
    }
    QReadLocker lock(&m_Computers[computerIndex]->lock);
    snapshot.insert(QStringLiteral("performanceContext"), m_Computers[computerIndex]->serverSoftwareLabel() + QLatin1Char('/') +
                    m_Computers[computerIndex]->hostSoftwareRevision + QLatin1Char('/') + m_Computers[computerIndex]->activeAddress.toString());
    snapshot.insert(QStringLiteral("desktopScaleSupported"), m_Computers[computerIndex]->desktopScaleSupported);
    const auto& caps = m_Computers[computerIndex]->capabilities;
    snapshot.insert(QStringLiteral("apolloCompatible"), caps.apollo);
    snapshot.insert(QStringLiteral("virtualDisplaySupported"), caps.virtualDisplay);
    snapshot.insert(QStringLiteral("virtualDisplayReady"), caps.virtualDisplayReady);
    snapshot.insert(QStringLiteral("clipboardReadAllowed"), caps.allows(HostCapabilities::ClipboardRead));
    snapshot.insert(QStringLiteral("clipboardWriteAllowed"), caps.allows(HostCapabilities::ClipboardWrite));
    snapshot.insert(QStringLiteral("launchAllowed"), caps.allows(HostCapabilities::Launch));
    snapshot.insert(QStringLiteral("foreignDesktopRetained"), NvApp::hasForeignDesktop(m_Computers[computerIndex]->appList));
    snapshot.insert(QStringLiteral("desktopTakeoverAvailable"), NvApp::catalogueRole(m_Computers[computerIndex]->appList, 4).isInitialized());
    snapshot.insert(QStringLiteral("adaptiveBitrateSupported"), m_Computers[computerIndex]->adaptiveBitrateSupported);
    // The codecs it sends, as the stream plan reads them. A host without an HEVC size
    // limit doesn't really send HEVC.
    int codecModes = m_Computers[computerIndex]->serverCodecModeSupport;
    if (m_Computers[computerIndex]->maxLumaPixelsHEVC == 0) codecModes &= ~SCM_MASK_HEVC;
    snapshot.insert(QStringLiteral("serverCodecModeSupport"), codecModes);
    return snapshot;
}

QString ComputerModel::generatePinString()
{
    return m_ComputerManager->generatePinString();
}

int ComputerModel::reserveConnectionTestAttemptId()
{
    return nextConnectionTestAttemptId.fetch_add(1, std::memory_order_relaxed);
}

void ComputerModel::testConnectionForComputer(int, int attemptId)
{
    const bool legacy = attemptId < 0;
    if (legacy) attemptId = reserveConnectionTestAttemptId();
    if (m_ConnectionTests.contains(attemptId)) return;
    auto diagnostic = new ConnectionDiagnostic(this);
    m_ConnectionTests.insert(attemptId, diagnostic);
    connect(diagnostic, &ConnectionDiagnostic::finished, this,
            [this, diagnostic, attemptId, legacy](int result) {
        const bool current = m_ConnectionTests.remove(attemptId);
        diagnostic->deleteLater();
        if (!current) return;
        char blockedPorts[512] = {};
        if (result > 0) LiStringifyPortFlags(result, "\n", blockedPorts, sizeof(blockedPorts));
        if (legacy) emit connectionTestCompleted(result, QString::fromUtf8(blockedPorts));
        else emit connectionDiagnosticCompleted(attemptId, result, QString::fromUtf8(blockedPorts));
    });
    QVector<ConnectionDiagnostic::Port> ports;
    for (int i = 0; i < 32; ++i) {
        if (ML_PORT_FLAG_ALL & (1U << i))
            ports.append({1U << i, LiGetPortFromPortFlagIndex(i),
                          LiGetProtocolFromPortFlagIndex(i) == IPPROTO_UDP});
    }
    diagnostic->start(QStringLiteral("qt.conntest.moonlight-stream.org"), 443, ports);
}

void ComputerModel::cancelConnectionTest(int attemptId)
{
    auto diagnostic = m_ConnectionTests.take(attemptId);
    if (diagnostic) diagnostic->cancel();
}

int ComputerModel::reservePairingAttemptId()
{
    return nextPairingAttemptId.fetch_add(1, std::memory_order_relaxed);
}

void ComputerModel::pairComputer(int computerIndex, QString pin, int attemptId)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return;
    }

    m_ComputerManager->pairHost(m_Computers[computerIndex], pin, attemptId);
}

void ComputerModel::handlePairingCompleted(NvComputer*, QString error, int attemptId)
{
    emit pairingCompleted(attemptId, error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    QVector<NvComputer*> newComputerList = m_ComputerManager->getComputers();

    // Reset the model if the structural layout of the list has changed
    if (m_Computers != newComputerList) {
        beginResetModel();
        m_Computers = newComputerList;
        endResetModel();
    }
    else {
        // Let the view know that this specific computer changed
        int index = m_Computers.indexOf(computer);
        if (index >= 0) {
            emit dataChanged(createIndex(index, 0), createIndex(index, 0));
        }
    }
}

#include "computermodel.moc"
