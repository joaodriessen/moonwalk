#include "computermanager.h"
#include <QUuid>
#include "boxartmanager.h"
#include "nvhttp.h"
#include "nvpairingmanager.h"

#include <Limelight.h>
#include <QtEndian>

#include <QThread>
#include <QThreadPool>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QSettings>
#include <atomic>

#define SER_HOSTS "hosts"
#define SER_HOSTS_BACKUP "hostsbackup"

class PcMonitorThread : public QThread
{
    Q_OBJECT

#define TRIES_BEFORE_OFFLINING 2
#define POLLS_PER_APPLIST_FETCH 10

public:
    PcMonitorThread(NvComputer* computer)
        : m_Computer(computer)
    {
        setObjectName("Polling thread for " + computer->name);
    }

private:
    bool tryPollComputer(QNetworkAccessManager* nam, NvAddress address, bool& changed)
    {
        NvHTTP http(address, 0, m_Computer->serverCert, !m_Computer->isNvidiaServerSoftware, nam);

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo(NvHTTP::NvLogLevel::NVLL_NONE, true);
        } catch (...) {
            return false;
        }

        NvComputer newState(http, serverInfo);

        // Ensure the machine that responded is the one we intended to contact
        if (m_Computer->uuid != newState.uuid) {
            qInfo() << "Found unexpected PC" << newState.name << "looking for" << m_Computer->name;
            return false;
        }

        changed = m_Computer->update(newState);
        return true;
    }

    bool updateAppList(QNetworkAccessManager* nam, bool& changed)
    {
        NvHTTP http(m_Computer, nam);

        QVector<NvApp> appList;

        try {
            appList = http.getAppList();
            if (appList.isEmpty()) {
                return false;
            }
        } catch (...) {
            return false;
        }

        QWriteLocker lock(&m_Computer->lock);
        changed = m_Computer->updateAppList(appList);
        return true;
    }

    void run() override
    {
        // Reduce the power and performance impact of our
        // computer status polling while it's running.
        setPriority(QThread::LowPriority);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        setServiceLevel(QThread::QualityOfService::Eco);
#endif

        // Share the QNetworkAccessManager to conserve resources when polling.
        // Each instance creates a worker thread, so sharing them ensures that
        // we are not spamming a new thread for every single polling attempt.
        //
        // Since QThread inherit the priority of the current thread, this also
        // ensures that the NAM's worker thread will inherit our lower priority.
        QNetworkAccessManager nam;

        // Always fetch the applist the first time
        int pollsSinceLastAppListFetch = POLLS_PER_APPLIST_FETCH;
        while (!isInterruptionRequested()) {
            bool stateChanged = false;
            bool online = false;
            bool wasOnline = m_Computer->state == NvComputer::CS_ONLINE;
            for (int i = 0; i < (wasOnline ? TRIES_BEFORE_OFFLINING : 1) && !online; i++) {
                for (auto& address : m_Computer->uniqueAddresses()) {
                    if (isInterruptionRequested()) {
                        return;
                    }

                    if (tryPollComputer(&nam, address, stateChanged)) {
                        if (!wasOnline) {
                            qInfo() << m_Computer->name << "is now online at" << m_Computer->activeAddress.toString();
                        }
                        online = true;
                        break;
                    }
                }
            }

            // Check if we failed after all retry attempts
            // Note: we don't need to acquire the read lock here,
            // because we're on the writing thread.
            if (!online && m_Computer->state != NvComputer::CS_OFFLINE) {
                qInfo() << m_Computer->name << "is now offline";
                m_Computer->state = NvComputer::CS_OFFLINE;
                stateChanged = true;
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    m_Computer->pairState == NvComputer::PS_PAIRED &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
                // Notify prior to the app list poll since it may take a while, and we don't
                // want to delay onlining of a machine, especially if we already have a cached list.
                if (stateChanged) {
                    emit computerStateChanged(m_Computer);
                    stateChanged = false;
                }

                if (updateAppList(&nam, stateChanged)) {
                    pollsSinceLastAppListFetch = 0;
                }
            }

            if (stateChanged) {
                // Tell anyone listening that we've changed state
                emit computerStateChanged(m_Computer);
            }

            // Wait a bit to poll again, but do it in 100 ms chunks
            // so we can be interrupted reasonably quickly.
            // FIXME: QWaitCondition would be better.
            for (int i = 0; i < 30 && !isInterruptionRequested(); i++) {
                QThread::msleep(100);
            }
        }
    }

signals:
   void computerStateChanged(NvComputer* computer);

private:
    NvComputer* m_Computer;
};

ComputerManager::ComputerManager(StreamingPreferences* prefs)
    : m_Prefs(prefs),
      m_PollingRef(0),
      m_MdnsBrowser(nullptr),
      m_CompatFetcher(nullptr),
      m_NeedsDelayedFlush(false)
{
    QSettings settings;

    // If there's a hosts backup copy, we must have failed to commit
    // a previous update before exiting. Restore the backup now.
    int hosts = settings.beginReadArray(SER_HOSTS_BACKUP);
    if (hosts == 0) {
        // If there's no host backup, read from the primary location.
        settings.endArray();
        hosts = settings.beginReadArray(SER_HOSTS);
    }

    // Inflate our hosts from QSettings
    for (int i = 0; i < hosts; i++) {
        settings.setArrayIndex(i);
        NvComputer* computer = new NvComputer(settings);
        QSharedPointer<NvComputer> owner(computer);
        computer->setOwner(owner);
        m_KnownHosts[computer->uuid] = owner;
        m_LastSerializedHosts[computer->uuid] = *computer;
    }
    settings.endArray();

    // Start the delayed flush thread to handle saveHosts() calls
    m_DelayedFlushThread = new DelayedFlushThread(this);
    m_DelayedFlushThread->start();

    // To quit in a timely manner, we must block additional requests
    // after we receive the aboutToQuit() signal. This is necessary
    // because NvHTTP uses aboutToQuit() to abort requests in progress
    // while quitting, however this is a one time signal - additional
    // requests would not be aborted and block termination.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &ComputerManager::handleAboutToQuit);
}

ComputerManager::~ComputerManager()
{
    // Stop the delayed flush thread before acquiring the lock in write mode
    // to avoid deadlocking with a flush that needs the lock in read mode.
    {
        // Wake the delayed flush thread
        m_DelayedFlushThread->requestInterruption();
        m_DelayedFlushCondition.wakeOne();

        // Wait for it to terminate (and finish any pending flush)
        m_DelayedFlushThread->wait();
        delete m_DelayedFlushThread;

        // Delayed flushes should have completed by now
        Q_ASSERT(!m_NeedsDelayedFlush);
    }

    QWriteLocker lock(&m_Lock);

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        delete computer;
        m_PendingResolution.removeFirst();
    }

    // Delete the browser to stop discovery
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;

    // Interrupt polling
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }

    // Delete all polling entries (and associated threads)
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        delete entry;
    }

    // Destroy all NvComputer objects now that polling is halted
    m_KnownHosts.clear();
}

void DelayedFlushThread::run() {
    for (;;) {
        // Wait for a delayed flush request or an interruption
        {
            QMutexLocker locker(&m_ComputerManager->m_DelayedFlushMutex);

            while (!QThread::currentThread()->isInterruptionRequested() && !m_ComputerManager->m_NeedsDelayedFlush) {
                m_ComputerManager->m_DelayedFlushCondition.wait(&m_ComputerManager->m_DelayedFlushMutex);
            }

            // Bail without flushing if we woke up for an interruption alone.
            // If we have both an interruption and a flush request, do the flush.
            if (!m_ComputerManager->m_NeedsDelayedFlush) {
                Q_ASSERT(QThread::currentThread()->isInterruptionRequested());
                break;
            }

            // Reset the delayed flush flag to ensure any racing saveHosts() call will set it again
            m_ComputerManager->m_NeedsDelayedFlush = false;

            // Update the last serialized hosts map under the delayed flush mutex
            m_ComputerManager->m_LastSerializedHosts.clear();
            QReadLocker hostsLock(&m_ComputerManager->m_Lock);
            for (const auto& owner : std::as_const(m_ComputerManager->m_KnownHosts)) {
                const NvComputer* computer = owner.data();
                // Copy the current state of the NvComputer to allow us to check later if we need
                // to serialize it again when attribute updates occur.
                QReadLocker computerLock(&computer->lock);
                m_ComputerManager->m_LastSerializedHosts[computer->uuid] = *computer;
            }
        }

        // Perform the flush
        {
            QSettings settings;

            // First, write to the backup location
            settings.beginWriteArray(SER_HOSTS_BACKUP);
            {
                QReadLocker lock(&m_ComputerManager->m_Lock);
                int i = 0;
                for (const auto& owner : std::as_const(m_ComputerManager->m_KnownHosts)) {
                    const NvComputer* computer = owner.data();
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, false);
                }
            }
            settings.endArray();

            // Next, write to the primary location
            settings.remove(SER_HOSTS);
            settings.beginWriteArray(SER_HOSTS);
            {
                QReadLocker lock(&m_ComputerManager->m_Lock);
                int i = 0;
                for (const auto& owner : std::as_const(m_ComputerManager->m_KnownHosts)) {
                    const NvComputer* computer = owner.data();
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, true);
                }
            }
            settings.endArray();

            // Finally, delete the backup copy
            settings.remove(SER_HOSTS_BACKUP);
        }
    }
}

void ComputerManager::saveHosts()
{
    Q_ASSERT(m_DelayedFlushThread != nullptr && m_DelayedFlushThread->isRunning());

    // Punt to a worker thread because QSettings on macOS can take ages (> 500 ms)
    // to persist our host list to disk (especially when a host has a bunch of apps).
    QMutexLocker locker(&m_DelayedFlushMutex);
    m_NeedsDelayedFlush = true;
    m_DelayedFlushCondition.wakeOne();
}

QHostAddress ComputerManager::getBestGlobalAddressV6(QVector<QHostAddress> &addresses)
{
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            if (address.isInSubnet(QHostAddress("fe80::"), 10)) {
                // Link-local
                continue;
            }

            if (address.isInSubnet(QHostAddress("fec0::"), 10)) {
                qInfo() << "Ignoring site-local address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("fc00::"), 7)) {
                qInfo() << "Ignoring ULA:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2002::"), 16)) {
                qInfo() << "Ignoring 6to4 address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2001::"), 32)) {
                qInfo() << "Ignoring Teredo address:" << address;
                continue;
            }

            return address;
        }
    }

    return QHostAddress();
}

void ComputerManager::startPolling()
{
    QWriteLocker lock(&m_Lock);

    if (++m_PollingRef > 1) {
        return;
    }

    if (m_Prefs->enableMdns) {
        // Start an MDNS query for GameStream hosts
        m_MdnsServer.reset(new QMdnsEngine::Server());
        m_MdnsBrowser = new QMdnsEngine::Browser(m_MdnsServer.data(), "_nvstream._tcp.local.");
        connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceAdded,
                this, [this](const QMdnsEngine::Service& service) {
            qInfo() << "Discovered mDNS host:" << service.hostname();

            MdnsPendingComputer* pendingComputer = new MdnsPendingComputer(m_MdnsServer, service);
            connect(pendingComputer, &MdnsPendingComputer::resolvedHost,
                    this, &ComputerManager::handleMdnsServiceResolved);
            m_PendingResolution.append(pendingComputer);
        });
    }
    else {
        qWarning() << "mDNS is disabled by user preference";
    }

    // Start polling threads for each known host
    QMapIterator<QString, QSharedPointer<NvComputer>> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value().data());
    }
}

// Must hold m_Lock for write
void ComputerManager::startPollingComputer(NvComputer* computer)
{
    if (m_PollingRef == 0) {
        return;
    }

    ComputerPollingEntry* pollingEntry;

    if (!m_PollEntries.contains(computer->uuid)) {
        pollingEntry = m_PollEntries[computer->uuid] = new ComputerPollingEntry();
    }
    else {
        pollingEntry = m_PollEntries[computer->uuid];
    }

    if (!pollingEntry->isActive() && !pollingEntry->hasRunningInactiveThreads()) {
        PcMonitorThread* thread = new PcMonitorThread(computer);
        connect(thread, &PcMonitorThread::computerStateChanged,
                this, &ComputerManager::handleComputerStateChanged);
        const QString uuid = computer->uuid;
        connect(thread, &QThread::finished, this, [this, uuid, computer]() {
            QWriteLocker lock(&m_Lock);
            if (m_KnownHosts.value(uuid).data() == computer) {
                startPollingComputer(computer);
            }
        });
        pollingEntry->setActiveThread(thread);
        thread->start();
    }
}

void ComputerManager::handleMdnsServiceResolved(MdnsPendingComputer* computer,
                                                QVector<QHostAddress>& addresses)
{
    QHostAddress v6Global = getBestGlobalAddressV6(addresses);
    bool added = false;

    // Add the host using the IPv4 address
    for (const QHostAddress& address : std::as_const(addresses)) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            // NB: We don't just call addNewHost() here with v6Global because the IPv6
            // address may not be reachable (if the user hasn't installed the IPv6 helper yet
            // or if this host lacks outbound IPv6 capability). We want to add IPv6 even if
            // it's not currently reachable.
            addNewHost(NvAddress(address, computer->port()),
                       true, computer->hostname(),
                       NvAddress(v6Global, computer->port()));
            added = true;
            break;
        }
    }

    if (!added) {
        // If we get here, there wasn't an IPv4 address so we'll do it v6-only
        for (const QHostAddress& address : std::as_const(addresses)) {
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                // Use a link-local or site-local address for the "local address"
                if (address.isInSubnet(QHostAddress("fe80::"), 10) ||
                        address.isInSubnet(QHostAddress("fec0::"), 10) ||
                        address.isInSubnet(QHostAddress("fc00::"), 7)) {
                    addNewHost(NvAddress(address, computer->port()),
                               true, computer->hostname(),
                               NvAddress(v6Global, computer->port()));
                    break;
                }
            }
        }
    }

    m_PendingResolution.removeOne(computer);
    computer->deleteLater();
}

void ComputerManager::saveHost(NvComputer *computer)
{
    // If no serializable properties changed, don't bother saving hosts
    QMutexLocker lock(&m_DelayedFlushMutex);
    QReadLocker computerLock(&computer->lock);
    if (!m_LastSerializedHosts.value(computer->uuid).isEqualSerialized(*computer)) {
        // Queue a request for a delayed flush to QSettings outside of the lock
        computerLock.unlock();
        lock.unlock();
        saveHosts();
    }
}

void ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    QSharedPointer<NvComputer> owner;
    {
        QReadLocker lock(&m_Lock);
        for (const auto& candidate : std::as_const(m_KnownHosts)) {
            if (candidate.data() == computer) {
                owner = candidate;
                break;
            }
        }
    }
    if (!owner) return;
    bool pairedNvidiaHost;
    {
        QReadLocker lock(&computer->lock);
        pairedNvidiaHost = computer->isNvidiaServerSoftware && !computer->serverCert.isNull();
    }
    if (pairedNvidiaHost) m_CompatFetcher.start();
    emit computerStateChanged(computer);

    if (computer->pendingQuit && computer->currentGameId == 0) {
        computer->pendingQuit = false;
        emit quitAppCompleted(QVariant());
    }

    // Save updates to this host
    saveHost(computer);
}

QVector<NvComputer*> ComputerManager::getComputers()
{
    QReadLocker lock(&m_Lock);

    // Return a sorted host list
    QVector<NvComputer*> hosts;
    hosts.reserve(m_KnownHosts.size());
    for (const auto& owner : std::as_const(m_KnownHosts)) {
        hosts.append(owner.data());
    }
    std::stable_sort(hosts.begin(), hosts.end(), [](const NvComputer* host1, const NvComputer* host2) {
        return host1->name.toLower() < host2->name.toLower();
    });
    return hosts;
}

class DeferredHostDeletionTask : public QRunnable
{
public:
    DeferredHostDeletionTask(QSharedPointer<NvComputer> computer, ComputerPollingEntry* pollingEntry)
        : m_Computer(std::move(computer)), m_PollingEntry(pollingEntry) {}

    void run()
    {
        // Remove this host's per-host settings overlay
        // (docs/design/2026-07-14-per-host-settings.md §7). Must match the
        // "hostsettings/<uuid>/*" key format StreamingPreferences uses to
        // write these values (see SER_HOSTSETTINGS_GROUP in
        // streamingpreferences.cpp) -- QSettings::remove() on the group
        // prefix recursively removes every key underneath it. m_Computer is
        // not deleted until the end of this function, so its uuid is still
        // valid here.
        {
            QSettings settings;
            settings.remove(QStringLiteral("hostsettings/") + m_Computer->uuid);
        }

        // Delete the polling entry first. This will stop all polling threads too.
        delete m_PollingEntry;

        // Delete cached box art
        BoxArtManager::deleteBoxArt(m_Computer.data());

        // The host is released after this task and all outstanding users finish.
    }

private:
    QSharedPointer<NvComputer> m_Computer;
    ComputerPollingEntry* m_PollingEntry;
};

void ComputerManager::deleteHost(NvComputer* computer)
{
    QSharedPointer<NvComputer> owner;
    ComputerPollingEntry* pollingEntry;
    {
        QWriteLocker lock(&m_Lock);
        QString uuid;
        for (auto it = m_KnownHosts.cbegin(); it != m_KnownHosts.cend(); ++it) {
            if (it.value().data() == computer) {
                uuid = it.key();
                break;
            }
        }
        if (uuid.isNull()) return;
        owner = m_KnownHosts.take(uuid);
        owner->markRemoved();
        pollingEntry = m_PollEntries.take(uuid);
        if (pollingEntry) pollingEntry->interrupt();
    }
    saveHosts();
    // Slow polling joins and cache cleanup stay off the GUI thread.
    QThreadPool::globalInstance()->start(new DeferredHostDeletionTask(owner, pollingEntry));
}

void ComputerManager::renameHost(NvComputer* computer, QString name)
{
    {
        QWriteLocker lock(&computer->lock);

        computer->name = name;
        computer->hasCustomName = true;
    }

    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::setHostOs(NvComputer* computer, QString os)
{
    {
        QWriteLocker lock(&computer->lock);

        computer->os = os;
    }

    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::clientSideAttributeUpdated(NvComputer* computer)
{
    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::handleAboutToQuit()
{
    for (const auto& cancellation : std::as_const(m_PairingCancellations)) {
        cancellation->cancel();
    }
    QReadLocker lock(&m_Lock);

    // Interrupt polling threads immediately, so they
    // avoid making additional requests while quitting
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

class PendingPairingTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingPairingTask(ComputerManager* computerManager, NvComputer* computer, QString pin, int attemptId, QSharedPointer<NvHTTP::CancelToken> cancellation)
        : m_ComputerManager(computerManager),
          m_ComputerOwner(computer->retain()),
          m_Computer(computer),
          m_Pin(pin),
          m_AttemptId(attemptId),
          m_Cancellation(cancellation)
    {
        // Keep the host alive until the queued completion reaches the manager.
        // The worker itself may be destroyed before the GUI processes it.
        connect(this, &PendingPairingTask::pairingCompleted, computerManager,
                [computerManager, owner = m_ComputerOwner, cancellation](NvComputer* computer, QString error, int attemptId) {
            if (computerManager->m_PairingCancellations.value(attemptId) == cancellation) {
                computerManager->m_PairingCancellations.remove(attemptId);
            }
            if (cancellation->isCancelled()) return;
            emit computerManager->pairingCompleted(owner ? owner.data() : computer, error, attemptId);
        });
    }

signals:
    void pairingCompleted(NvComputer* computer, QString error, int attemptId);

private:
    void run()
    {
        try {
           // Loading the client certificate and key can throw. Catch that here
           // so pairing fails without terminating the worker process.
           NvPairingManager pairingManager(m_Computer, m_Cancellation.data());

           NvPairingManager::PairState result = pairingManager.pair(m_Computer->appVersion, m_Pin, m_Computer->serverCert);
           if (m_Cancellation->isCancelled()) {
               emit pairingCompleted(m_Computer, QString(), m_AttemptId);
               return;
           }
           switch (result)
           {
           case NvPairingManager::PairState::PIN_WRONG:
               emit pairingCompleted(m_Computer, tr("The PIN from the host didn't match. Please try again."), m_AttemptId);
               break;
           case NvPairingManager::PairState::FAILED:
               if (m_Computer->currentGameId != 0) {
                   emit pairingCompleted(m_Computer, tr("Pairing failed while the host had a session open. End that session, then try pairing again."), m_AttemptId);
               }
               else {
                   emit pairingCompleted(m_Computer, tr("Pairing failed. Please try again."), m_AttemptId);
               }
               break;
           case NvPairingManager::PairState::ALREADY_IN_PROGRESS:
               emit pairingCompleted(m_Computer, tr("Another pairing attempt is already in progress."), m_AttemptId);
               break;
           case NvPairingManager::PairState::PAIRED:
               // Persist the newly pinned server certificate for this host
               m_ComputerManager->saveHost(m_Computer);

               emit pairingCompleted(m_Computer, nullptr, m_AttemptId);
               break;
           }
        } catch (const GfeHttpResponseException& e) {
            emit pairingCompleted(m_Computer, tr("Couldn't pair with the host: %1").arg(e.toQString().trimmed()), m_AttemptId);
        } catch (const QtNetworkReplyException& e) {
            emit pairingCompleted(m_Computer, tr("Couldn't pair with the host: %1").arg(e.toQString().trimmed()), m_AttemptId);
        } catch (const std::exception& e) {
            // Catch-all backstop (e.g. an identity-cert load failure thrown from the
            // NvPairingManager constructor now inside the try): report it as a pairing
            // failure rather than letting it escape this worker thread and terminate
            // the app.
            emit pairingCompleted(m_Computer, tr("Couldn't pair with the host: %1").arg(QString::fromUtf8(e.what()).trimmed()), m_AttemptId);
        }
    }

    ComputerManager* m_ComputerManager;
    QSharedPointer<NvComputer> m_ComputerOwner;
    NvComputer* m_Computer;
    QString m_Pin;
    int m_AttemptId;
    QSharedPointer<NvHTTP::CancelToken> m_Cancellation;
};

void ComputerManager::pairHost(NvComputer* computer, QString pin, int attemptId)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for pairing to complete
    cancelPairing(attemptId);
    auto cancellation = QSharedPointer<NvHTTP::CancelToken>::create();
    m_PairingCancellations.insert(attemptId, cancellation);
    PendingPairingTask* pairing = new PendingPairingTask(this, computer, pin, attemptId, cancellation);
    QThreadPool::globalInstance()->start(pairing);
}

void ComputerManager::cancelPairing(int attemptId)
{
    auto cancellation = m_PairingCancellations.value(attemptId);
    if (cancellation) cancellation->cancel();
}

class PendingQuitTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingQuitTask(ComputerManager* computerManager, NvComputer* computer)
        : m_ComputerOwner(computer->retain()), m_Computer(computer)
    {
        connect(this, &PendingQuitTask::quitAppFailed,
                computerManager, &ComputerManager::quitAppCompleted);
    }

signals:
    void quitAppFailed(QString error);

private:
    void run()
    {
        NvHTTP http(m_Computer);

        try {
            if (m_Computer->currentGameId != 0) {
                http.quitApp();
            }
        } catch (const GfeHttpResponseException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            if (e.getStatusCode() == 599) {
                // 599 is a special code we make a custom message for
                emit quitAppFailed(tr("The running app wasn't started by this computer. "
                                      "You must quit the app on the host manually or use the device that originally started it."));
            }
            else {
                emit quitAppFailed(tr("Couldn't quit the app on the host. (%1)").arg(e.toQString()));
            }
        } catch (const QtNetworkReplyException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            emit quitAppFailed(tr("Couldn't quit the app on the host. (%1)").arg(e.toQString()));
        }
    }

    QSharedPointer<NvComputer> m_ComputerOwner;
    NvComputer* m_Computer;
};

void ComputerManager::quitRunningApp(NvComputer* computer)
{
    QWriteLocker lock(&computer->lock);
    computer->pendingQuit = true;

    PendingQuitTask* quit = new PendingQuitTask(this, computer);
    QThreadPool::globalInstance()->start(quit);
}

void ComputerManager::stopPollingAsync()
{
    QWriteLocker lock(&m_Lock);

    Q_ASSERT(m_PollingRef > 0);
    if (--m_PollingRef > 0) {
        return;
    }

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        computer->deleteLater();
        m_PendingResolution.removeFirst();
    }

    // Delete the browser and server to stop discovery and refresh polling
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;
    m_MdnsServer.reset();

    // Interrupt all threads, but don't wait for them to terminate
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

bool ComputerManager::isValidHostAddress(QString address) const
{
    address = address.trimmed();
    if (address.isEmpty()) {
        return false;
    }

    if (QHostAddress(address).protocol() == QAbstractSocket::IPv6Protocol) {
        return true;
    }

    QUrl url("moonlight://" + address, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty() && url.scheme() == "moonlight" &&
           url.userInfo().isEmpty() && url.path().isEmpty() &&
           !url.hasQuery() && !url.hasFragment() && url.port(-1) != 0;
}

QString ComputerManager::reserveManualAddAttemptId()
{
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_ManualAddAttempts.insert(id, QSharedPointer<NvHTTP::CancelToken>::create());
    return id;
}

void ComputerManager::cancelManualAdd(const QString& attemptId)
{
    auto cancellation = m_ManualAddAttempts.take(attemptId);
    if (cancellation) cancellation->cancel();
}

void ComputerManager::addNewHostManually(QString address, QString attemptId)
{
    if (!attemptId.isEmpty() && !m_ManualAddAttempts.contains(attemptId)) return;
    address = address.trimmed();
    if (!isValidHostAddress(address)) {
        if (attemptId.isEmpty()) emit computerAddCompleted(false, false);
        else if (m_ManualAddAttempts.remove(attemptId)) emit manualHostAddCompleted(attemptId, false, false);
    }
    else if (QHostAddress(address).protocol() == QAbstractSocket::IPv6Protocol) {
        // The user specified an IPv6 literal without URL escaping, so use the default port
        addNewHost(NvAddress(address, DEFAULT_HTTP_PORT), false, {}, {}, attemptId);
    }
    else {
        QUrl url("moonlight://" + address, QUrl::StrictMode);
        // If there wasn't a port specified, use the default
        addNewHost(NvAddress(url.host(), url.port(DEFAULT_HTTP_PORT)), false, {}, {}, attemptId);
    }
}

class PendingAddTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingAddTask(ComputerManager* computerManager, QString name, NvAddress address, NvAddress mdnsIpv6Address, bool mdns, QString attemptId, QSharedPointer<NvHTTP::CancelToken> cancellation)
        : m_ComputerManager(computerManager),
          m_Name(name),
          m_Address(address),
          m_MdnsIpv6Address(mdnsIpv6Address),
          m_Mdns(mdns),
          m_AllowExternalLookup(computerManager->m_Prefs->detectNetworkBlocking),
          m_AboutToQuit(false),
          m_Cancellation(cancellation)
    {
        connect(this, &PendingAddTask::computerAddCompleted,
                computerManager, [computerManager, attemptId](QVariant success, QVariant blocked) {
                    if (attemptId.isEmpty()) emit computerManager->computerAddCompleted(success, blocked);
                    else {
                        auto token = computerManager->m_ManualAddAttempts.take(attemptId);
                        if (token && !token->isCancelled()) emit computerManager->manualHostAddCompleted(attemptId, success.toBool(), blocked.toBool());
                    }
                });
        connect(this, &PendingAddTask::computerStateChanged,
                computerManager, &ComputerManager::handleComputerStateChanged);
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &PendingAddTask::handleAboutToQuit);
    }

signals:
    void computerAddCompleted(QVariant success, QVariant detectedPortBlocking);

    void computerStateChanged(NvComputer* computer);

private:
    void handleAboutToQuit()
    {
        m_AboutToQuit = true;
    }

    QString fetchServerInfo(NvHTTP& http)
    {
        QString serverInfo;

        // Do nothing if we're quitting
        if (m_AboutToQuit || (m_Cancellation && m_Cancellation->isCancelled())) {
            return QString();
        }

        try {
            // There's a race condition between GameStream servers reporting presence over
            // mDNS and the HTTPS server being ready to respond to our queries. To work
            // around this issue, we will issue the request again after a few seconds if
            // we see a ServiceUnavailableError error.
            try {
                serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
            } catch (const QtNetworkReplyException& e) {
                if (e.getError() == QNetworkReply::ServiceUnavailableError) {
                    qWarning() << "Retrying request in 5 seconds after ServiceUnavailableError";
                    QThread::sleep(5);
                    serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
                    qInfo() << "Retry successful";
                }
                else {
                    // Rethrow other errors
                    throw e;
                }
            }
            return serverInfo;
        } catch (...) {
            if (m_Cancellation && m_Cancellation->isCancelled()) return {};
            if (!m_Mdns) {
                unsigned int portTestResult;

                if (m_AllowExternalLookup) {
                    // We failed to connect to the specified PC. Let's test to make sure this network
                    // isn't blocking Moonlight, so we can tell the user about it.
                    portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443,
                                                              ML_PORT_FLAG_TCP_47984 | ML_PORT_FLAG_TCP_47989);
                }
                else {
                    portTestResult = 0;
                }

                emit computerAddCompleted(false, portTestResult != 0 && portTestResult != ML_TEST_RESULT_INCONCLUSIVE);
            }
            return QString();
        }
    }

    void learnRemoteAddress(NvComputer& computer)
    {
        const auto cancelled = [this]() {
            return m_AboutToQuit || (m_Cancellation && m_Cancellation->isCancelled());
        };
        if (!m_AllowExternalLookup || cancelled()) return;
        {
            QReadLocker lock(&computer.lock);
            if (!computer.remoteAddress.isNull()) return;
        }
        if (!m_Mdns && computer.getActiveAddressReachability(cancelled) == NvComputer::RI_VPN) return;
        if (cancelled()) return;

        quint32 address;
        const int error = LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &address);
        if (cancelled()) return;
        if (error != 0) {
            qWarning() << "STUN failed to get WAN address:" << error;
        } else if (computer.setDiscoveredRemoteAddress(QHostAddress(qFromBigEndian(address)))) {
            emit computerStateChanged(&computer);
        }
    }

    void run()
    {
        // Use the placeholder UID for the initial poll, then we'll switch to the real one if it's not GFE
        NvHTTP http(m_Address, 0, QSslCertificate(), false);
        http.setCancelToken(m_Cancellation.data());

        if (m_Mdns) {
            if (m_MdnsIpv6Address.isNull()) {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString();
            }
            else {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString()
                        << "and IPv6 address" << m_MdnsIpv6Address.toString();
            }
        }
        else {
            qInfo() << "Processing new PC at" << m_Address.toString() << "from user";
        }

        // Perform initial serverinfo fetch over HTTP since we don't know which cert to use
        QString serverInfo = fetchServerInfo(http);
        if (serverInfo.isEmpty() && !m_MdnsIpv6Address.isNull()) {
            // Retry using the global IPv6 address if the IPv4 or link-local IPv6 address fails
            http.setAddress(m_MdnsIpv6Address);
            serverInfo = fetchServerInfo(http);
        }
        if (serverInfo.isEmpty()) {
            return;
        }

        // Create initial newComputer using HTTP serverinfo with no pinned cert
        NvComputer* newComputer = new NvComputer(http, serverInfo);
        if (m_Mdns) {
            newComputer->discoveryHostname = m_Name;
        }
        http.setTrueUid(!newComputer->isNvidiaServerSoftware);

        // Check if we have a record of this host UUID to pull the pinned cert
        NvComputer* existingComputer;
        {
            QReadLocker lock(&m_ComputerManager->m_Lock);
            existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid).data();
            if (existingComputer != nullptr) {
                http.setServerCert(existingComputer->serverCert);
            }
        }

        // Fetch serverinfo again over HTTPS with the pinned cert
        if (existingComputer != nullptr) {
            Q_ASSERT(http.httpsPort() != 0);
            serverInfo = fetchServerInfo(http);
            if (serverInfo.isEmpty()) {
                delete newComputer;
                return;
            }

            // Update the polled computer with the HTTPS information
            NvComputer httpsComputer(http, serverInfo);
            newComputer->update(httpsComputer);
        }

        // Update addresses depending on the context
        if (m_Mdns) {
            // Only update local address if we actually reached the PC via this address.
            // If we reached it via the IPv6 address after the local address failed,
            // don't store the non-working local address.
            if (http.address() == m_Address) {
                newComputer->localAddress = m_Address;
            }

            if (!m_MdnsIpv6Address.isNull()) {
                Q_ASSERT(QHostAddress(m_MdnsIpv6Address.address()).protocol() == QAbstractSocket::IPv6Protocol);
                newComputer->ipv6Address = m_MdnsIpv6Address;
            }
        }
        else {
            newComputer->manualAddress = m_Address;
        }

        QHostAddress hostAddress(m_Address.address());
        bool addressIsSiteLocalV4 =
                hostAddress.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
                hostAddress.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
                hostAddress.isInSubnet(QHostAddress("192.168.0.0"), 16);
        const bool mdnsCanLearnRemote = m_Mdns &&
            QHostAddress(newComputer->localAddress.address()).protocol() == QAbstractSocket::IPv4Protocol;

        {
            // Check if this PC already exists using opportunistic read lock
            m_ComputerManager->m_Lock.lockForRead();
            QSharedPointer<NvComputer> existingOwner = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
            NvComputer* existingComputer = existingOwner.data();

            // If it doesn't already exist, convert to a write lock in preparation for updating.
            //
            // NB: ComputerManager's lock protects the host map itself, not the elements inside.
            // Those are protected by their individual locks. Since we only mutate the map itself
            // when the PC doesn't exist, we need the lock in write-mode for that case only.
            if (existingComputer == nullptr) {
                m_ComputerManager->m_Lock.unlock();
                m_ComputerManager->m_Lock.lockForWrite();

                // Since we had to unlock to lock for write, someone could have raced and added
                // this PC before us. We have to check again whether it already exists.
                existingOwner = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
                existingComputer = existingOwner.data();
            }

            if (m_Cancellation && m_Cancellation->isCancelled()) {
                m_ComputerManager->m_Lock.unlock();
                delete newComputer;
                return;
            }

            if (existingComputer != nullptr) {
                // Fold it into the existing PC
                bool changed = existingComputer->update(*newComputer);
                delete newComputer;

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // For non-mDNS clients, let them know it succeeded
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false);
                }

                // Tell our client if something changed
                if (changed) {
                    qInfo() << existingComputer->name << "is now at" << existingComputer->activeAddress.toString();
                    emit computerStateChanged(existingComputer);
                }
                if (mdnsCanLearnRemote) learnRemoteAddress(*existingOwner);
            }
            else {
                // Store this in our active sets
                QSharedPointer<NvComputer> owner(newComputer);
                newComputer->setOwner(owner);
                m_ComputerManager->m_KnownHosts[newComputer->uuid] = owner;

                // Start polling if enabled (write lock required)
                m_ComputerManager->startPollingComputer(newComputer);

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // For non-mDNS clients, let them know it succeeded
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false);
                }

                // Tell our client about this new PC
                emit computerStateChanged(newComputer);
                if (mdnsCanLearnRemote || (!m_Mdns && addressIsSiteLocalV4)) {
                    learnRemoteAddress(*owner);
                }
            }
        }
    }

    ComputerManager* m_ComputerManager;
    QString m_Name;
    NvAddress m_Address;
    NvAddress m_MdnsIpv6Address;
    bool m_Mdns;
    const bool m_AllowExternalLookup;
    std::atomic_bool m_AboutToQuit;
    QSharedPointer<NvHTTP::CancelToken> m_Cancellation;
};

void ComputerManager::addNewHost(NvAddress address, bool mdns, QString name, NvAddress mdnsIpv6Address, QString attemptId)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for serverinfo query to complete
    PendingAddTask* addTask = new PendingAddTask(this, name, address, mdnsIpv6Address, mdns, attemptId, m_ManualAddAttempts.value(attemptId));
    QThreadPool::globalInstance()->start(addTask);
}

QString ComputerManager::generatePinString()
{
    return QString::asprintf("%04u", QRandomGenerator::system()->bounded(10000));
}

#include "computermanager.moc"
