#include "backend/computermanager.h"
#include "hostidentitysource.h"
#include "streaming/session.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <QPointer>

class ConnectionDiagnostic;

class ComputerModel : public QAbstractListModel,
                      public HostIdentitySource
{
    Q_OBJECT

    enum Roles
    {
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

public:
    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void deleteComputer(int computerIndex);

    Q_INVOKABLE QString generatePinString();

    Q_INVOKABLE int reservePairingAttemptId();
    Q_INVOKABLE void pairComputer(int computerIndex, QString pin, int attemptId);

    Q_INVOKABLE int reserveConnectionTestAttemptId();
    Q_INVOKABLE void testConnectionForComputer(int computerIndex, int attemptId = -1);
    Q_INVOKABLE void cancelConnectionTest(int attemptId);

    // Wake-on-LAN. Returns immediately with a synchronous pre-check result:
    // "" means the operation completed inline or was dispatched; every accepted
    // call emits wakeCompleted(), including an already-online host. The
    // wake attempt was successfully dispatched to a worker thread; a
    // non-empty string is an immediate, no-network-required failure (bad
    // index, no MAC address known). The actual magic-packet send happens on
    // a QThreadPool worker (NvComputer::wake() can block on hostname DNS
    // resolution, so it must not run on the UI thread -- see commit
    // 78730f72) and its real result arrives later via the wakeCompleted()
    // signal below: empty error = at least one packet was sent
    // successfully; non-empty = the send failed everywhere.
    //
    // Note for the caller: a successful send only means the magic packet
    // left this machine. Whether the target actually wakes up depends on
    // its own firmware/OS having Wake-on-LAN enabled -- this client has no
    // way to confirm that.
    Q_INVOKABLE QString wakeComputer(int computerIndex);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);

    // Manual host-OS override, persisted via ComputerManager::setHostOs()
    // (mirrors renameComputer()/renameHost()). Pass "" to reset to
    // auto-detection. See the `hostOs` role for the effective value QML
    // should actually display.
    Q_INVOKABLE void setHostOs(int computerIndex, const QString& os);

    // Per-host settings (docs/design/2026-07-14-per-host-settings.md §6): the
    // stable key PcView passes to StreamingPreferences::setCurrentHost()
    // whenever the sidebar selection changes. Read-locked like data(), and
    // graceful (returns "") on an out-of-range index rather than asserting --
    // callers pass currentIndex straight through, which is briefly -1/out of
    // range during list resets.
    Q_INVOKABLE QString uuidForIndex(int computerIndex) const;
    // Opaque lifetime identity for an existing host object. Unlike row or UUID,
    // this changes when a deleted host is discovered again, even with the same UUID.
    Q_INVOKABLE QString hostIdentityForIndex(int computerIndex);

    Q_INVOKABLE QString nameForIndex(int computerIndex) const;

    // Cursor-coordination host-aware defaults (2026-07-18): lets a plain JS
    // function (PcView.qml's applyNativeFirstDefaultIfNeeded(), which only
    // has an index, not a ListView delegate's "model" context) read
    // NvComputer::serverSoftware the same way a delegate would via the
    // ServerSoftwareRole/"serverSoftware" role -- mirrors uuidForIndex()
    // exactly (same read-lock, same graceful ""-on-out-of-range).
    Q_INVOKABLE QString serverSoftwareForIndex(int computerIndex) const;
    Q_INVOKABLE QVariantMap presentationForIndex(int computerIndex) const;

    Q_INVOKABLE Session* createSessionForCurrentGame(int computerIndex);

    // Starlight: the "applications" concept is gone from the UI -- Connect
    // always targets the host's Desktop app directly (the Sunshine/GFE
    // convention of an app literally named "Desktop"). These four mirror
    // AppModel's equivalent single-app helpers so PcView can replicate
    // AppView's launch/resume/quit-gate logic without pushing the app grid.
    Q_INVOKABLE int getDesktopAppId(int computerIndex);

    Q_INVOKABLE Session* createSessionForDesktop(int computerIndex, bool safeSettings = false, bool desktopTakeover = false);

    Q_INVOKABLE int getRunningAppId(int computerIndex);

    Q_INVOKABLE QString getRunningAppName(int computerIndex);

    Q_INVOKABLE void quitRunningApp(int computerIndex);

signals:
    void pairingCompleted(int attemptId, QVariant error);
    void connectionTestCompleted(int result, QString blockedPorts);
    void connectionDiagnosticCompleted(int attemptId, int result, QString blockedPorts);

    // Deferred result of a wakeComputer() call that was dispatched to the
    // worker thread. error is empty on a successful send, else a
    // human-readable failure message.
    void wakeCompleted(int computerIndex, QString error);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handlePairingCompleted(NvComputer* computer, QString error, int attemptId);

private:
    QString uuidForRow(int row) const override;

    QHash<int, QPointer<ConnectionDiagnostic>> m_ConnectionTests;
    struct HostIdentity {
        QWeakPointer<NvComputer> owner;
        QString token;
    };
    QHash<QString, HostIdentity> m_HostIdentities;
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
};
