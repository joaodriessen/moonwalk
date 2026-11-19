#pragma once

#include "nvhttp.h"
#include "nvaddress.h"
#include "hostcapabilities.h"

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>
#include <QPair>
#include <QSharedPointer>
#include <QWeakPointer>
#include <atomic>
#include <memory>
#include <functional>

class CopySafeReadWriteLock : public QReadWriteLock
{
public:
    CopySafeReadWriteLock() = default;

    // Don't actually copy the QReadWriteLock
    CopySafeReadWriteLock(const CopySafeReadWriteLock&) : QReadWriteLock() {}
    CopySafeReadWriteLock& operator=(const CopySafeReadWriteLock &) { return *this; }
};

class NvComputer
{
    friend class PcMonitorThread;
    friend class ComputerManager;
    friend class PendingQuitTask;

private:
    void sortAppList();

    bool updateAppList(QVector<NvApp> newAppList);

    bool pendingQuit;

public:
    NvComputer() = default;

    // Background jobs keep a host alive after it leaves the visible host map.
    // Hosts constructed outside ComputerManager have no shared owner.
    QSharedPointer<NvComputer> retain() const { return m_Owner.toStrongRef(); }
    void setOwner(const QSharedPointer<NvComputer>& owner) { m_Owner = owner; }
    void markRemoved() { m_Removed->store(true, std::memory_order_release); }
    bool isRemoved() const { return m_Removed->load(std::memory_order_acquire); }

    // Caller is responsible for synchronizing read access to the other host
    NvComputer(const NvComputer&) = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer& operator=(const NvComputer &) = default;

    explicit NvComputer(NvHTTP& http, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    bool
    setDiscoveredRemoteAddress(QHostAddress);

    bool
    update(const NvComputer& that);

    bool
    wake() const;

    enum ReachabilityType
    {
        RI_UNKNOWN,
        RI_LAN,
        RI_VPN,
    };

    ReachabilityType
    getActiveAddressReachability(const std::function<bool()>& cancelled = {}) const;

    QVector<NvAddress>
    uniqueAddresses() const;

    // Race known addresses with fresh discovery. After the first reachable
    // route, a short grace collects faster peers without waiting for dead
    // candidates or DNS. Failed known routes still allow bounded discovery
    // recovery. Cancellation aborts callbacks/sockets before returning.
    // The caller owns route-generation pinning and deliberate-route policy.
    // Null means cancelled or no reachable route: keep existing selection.
    NvAddress
    probeFastestAddress(const std::function<bool()>& cancelled = {}) const;

    // Pure selection logic split out of probeFastestAddress() so it can be
    // unit tested without real sockets: given each candidate paired with its
    // probed connection latency in ms (or -1 if unreachable), returns the
    // candidate with the lowest latency, or a null NvAddress if none were
    // reachable.
    static NvAddress
    pickFastestAddress(const QVector<QPair<NvAddress, qint64>>& probeResults);

    void
    serialize(QSettings& settings, bool serializeApps) const;

    // Caller is responsible for synchronizing read access to both hosts
    bool
    isEqualSerialized(const NvComputer& that) const;

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    PairState pairState;
    NvAddress activeAddress;
    // Streaming target pin: when non-null (set by Session for MOONLIGHT_HOST_OVERRIDE),
    // update() will not let background polling overwrite activeAddress.
    NvAddress addressOverride;
    // Only a live streaming owner may pin polling's active route. Automatic
    // winners expire at cleanup; the saved deliberate choice lives in prefs.
    quint64 beginStreamingRoute(const NvAddress& preferred = NvAddress());
    bool pinStreamingRoute(quint64 generation, const NvAddress& address);
    void endStreamingRoute(quint64 generation);
    quint64 routeGeneration = 0;
    bool desktopScaleSupported = false;
    bool adaptiveBitrateSupported = false;
    bool clipboardImageSupported = false;
    bool fileTransferSupported = false;
    HostCapabilities capabilities;
    uint16_t activeHttpsPort;
    int currentGameId;
    int activeStreams = -1; // Optional; legacy currentgame may describe a paused target.
    QString gfeVersion;
    QString appVersion;
    QString hostSoftwareVersion; // Persisted with serverSoftware; see serverSoftwareLabel().
    QString hostSoftwareRevision;
    int maxAudioChannels = 0; // Optional live capability; never persisted across hosts.
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;
    QString gpuModel;
    bool isSupportedServerVersion;

    // Persisted traits
    NvAddress localAddress;
    NvAddress remoteAddress;
    NvAddress ipv6Address;
    NvAddress manualAddress;
    QByteArray macAddress;
    QString name;
    QString discoveryHostname;
    bool hasCustomName;
    // Manual host-OS override for the UI's OS icon. "" (the default, and
    // what an absent/missing settings key deserializes to) means "auto":
    // ComputerModel derives a best-effort value instead of showing a fixed
    // one. Values the UI understands: "" (auto/unset), "macos", "windows",
    // "linux", or any other string a user might have set previously (treated
    // as unknown by the UI, but preserved rather than silently discarded).
    // This is a purely client-side/manual field like hasCustomName -- it is
    // never populated from serverinfo (see nvhttp.cpp/update()'s comments:
    // there is no os/platform field in the GFE/Sunshine serverinfo XML) and
    // is deliberately excluded from NvComputer::update()'s field-merge list
    // so background polling can never overwrite a manual choice.
    QString os;
    QString uuid;
    QSslCertificate serverCert;
    QVector<NvApp> appList;
    bool isNvidiaServerSoftware;
    // Auto-detected host OS, parsed from an OPTIONAL <HostOs> tag in
    // serverinfo (values: "macos" | "windows" | "linux"; absent/empty/
    // unrecognized -> ""). This is a Starlight/Skylight protocol extension:
    // stock Sunshine/Apollo/GFE hosts don't send it and parsing is a no-op
    // no-warning miss against them (see NvHTTP::getXmlString, which returns
    // "" for any tag it doesn't find). Persisted (like isNvidiaServerSoftware,
    // NOT like the ephemeral gfeVersion/gpuModel) so the UI's OS icon
    // survives while the host is offline and there's no fresh serverinfo to
    // read. `os` above always takes precedence when non-empty; effective
    // value = os.isEmpty() ? (detectedOs.isEmpty() ? "unknown" : detectedOs)
    // : os -- see ComputerModel's hostOs role.
    QString detectedOs;

    // Server software product, derived from evidence hosts already send (no
    // new protocol): the <HostOs> tag only our forks emit => "Skylight"
    // (shown as Moonwalk Host); a SUNSHINE_* state => "Sunshine", "Apollo",
    // "Vibeshine" or "Vibepollo" by their extra serverinfo tags; the MJOLNIR
    // state codename => "GeForce Experience". Empty when nothing matched.
    // Persisted like detectedOs, together with hostSoftwareVersion, so the
    // host card stays informative while the host is offline between polls.
    QString serverSoftware;
    // "Moonwalk Host (0.4.0-alpha.15)", "Apollo", ...: the product, with its
    // version in brackets only when the host really reports one.
    QString serverSoftwareLabel() const;
    // Remember to update isEqualSerialized() when adding fields here!

    // Synchronization
    mutable CopySafeReadWriteLock lock;

private:
    QWeakPointer<NvComputer> m_Owner;
    std::shared_ptr<std::atomic_bool> m_Removed = std::make_shared<std::atomic_bool>(false);
    uint16_t externalPort;
};
