#pragma once

#include "nvaddress.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

// One connection-thread election. The context owns all network callbacks;
// returning never leaves a DNS callback or a socket probing a former session.
namespace AddressProbe {
constexpr int SocketTimeoutMs = 400;
constexpr int ResolveTimeoutMs = 1500;
constexpr int SuccessGraceMs = 60;
constexpr int MaximumCandidates = 8;

using Cancelled = std::function<bool()>;
using Resolved = std::function<void(const QHostInfo&)>;
struct Resolver {
    std::function<int(const QString&, QObject*, Resolved)> lookup;
    std::function<void(int)> abort;
};

struct Result {
    NvAddress address;
    int candidates = 0;
    bool cancelled = false;
};

// Numeric routes need no remote handshake or payload: UDP connect asks the
// kernel for the same source/interface selection used by the media transport.
// Keep a bounded, cancellable TCP fallback for legacy hostname addresses.
inline QPair<QHostAddress, QHostAddress> localRoute(const NvAddress& address,
                                                 const Cancelled& cancelled = {})
{
    if (cancelled && cancelled()) return {};
    QHostAddress numeric;
    if (numeric.setAddress(address.address())) {
        QUdpSocket socket;
        socket.setProxy(QNetworkProxy::NoProxy);
        socket.connectToHost(numeric, address.port());
        if (socket.state() == QAbstractSocket::ConnectedState)
            return qMakePair(socket.localAddress(), socket.peerAddress());
        return {};
    }

    QTcpSocket socket;
    socket.setProxy(QNetworkProxy::NoProxy);
    QEventLoop loop;
    QTimer deadline, poll;
    deadline.setSingleShot(true);
    poll.setInterval(10);
    QObject::connect(&socket, &QTcpSocket::connected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QTcpSocket::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancelled && cancelled()) loop.quit();
    });
    if (QCoreApplication::instance())
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    socket.connectToHost(address.address(), address.port());
    deadline.start(3000);
    if (cancelled) poll.start();
    if (socket.state() != QAbstractSocket::ConnectedState && socket.state() != QAbstractSocket::UnconnectedState)
        loop.exec();
    if (socket.state() == QAbstractSocket::ConnectedState && !(cancelled && cancelled()))
        return qMakePair(socket.localAddress(), socket.peerAddress());
    return {};
}

inline NvAddress pickFastest(const QVector<QPair<NvAddress, qint64>>& results)
{
    NvAddress fastest;
    qint64 latency = -1;
    for (const auto& result : results) {
        if (result.second >= 0 && (latency < 0 || result.second < latency)) {
            fastest = result.first;
            latency = result.second;
        }
    }
    return fastest;
}

inline Result run(const QVector<NvAddress>& known, const QString& hostname,
                  uint16_t port, const Cancelled& cancelled = {}, Resolver resolver = {})
{
    if (cancelled && cancelled()) return {NvAddress(), 0, true};
    if (!resolver.lookup) {
        resolver.lookup = [](const QString& name, QObject* context, Resolved resolved) {
            return QHostInfo::lookupHost(name, context, std::move(resolved));
        };
        resolver.abort = [](int id) { QHostInfo::abortHostLookup(id); };
    }

    QObject context;
    QEventLoop loop;
    QTimer grace, nameDeadline, cancellationPoll;
    grace.setSingleShot(true);
    nameDeadline.setSingleShot(true);
    cancellationPoll.setInterval(10);
    struct Probe {
        NvAddress address;
        QTcpSocket* socket;
        QTimer* deadline;
        QElapsedTimer elapsed;
        bool complete = false;
    };
    std::vector<std::unique_ptr<Probe>> probes;
    QVector<QPair<NvAddress, qint64>> results;
    bool dnsPending = !hostname.isEmpty() && hostname != QStringLiteral("UNKNOWN");
    bool finished = false;
    bool wasCancelled = false;
    int lookupId = -1;
    int pending = 0;
    int addingCandidates = 1;

    auto finish = [&] { finished = true; loop.quit(); };
    auto abortLookup = [&] {
        if (lookupId >= 0 && resolver.abort) resolver.abort(lookupId);
        lookupId = -1;
    };
    auto finishIfComplete = [&] {
        if (addingCandidates == 0 && !dnsPending && pending == 0) finish();
    };
    auto complete = [&](Probe* probe, bool reachable) {
        if (finished || probe->complete) return;
        probe->complete = true;
        probe->deadline->stop();
        --pending;
        results.append(qMakePair(probe->address, reachable ? probe->elapsed.elapsed() : qint64(-1)));
        if (reachable && !grace.isActive()) grace.start(SuccessGraceMs);
        probe->socket->abort();
        finishIfComplete();
    };
    auto addCandidate = [&](const NvAddress& address) {
        if (finished || address.isNull() || probes.size() >= MaximumCandidates) return;
        for (const auto& probe : probes) if (probe->address == address) return;
        auto probe = std::make_unique<Probe>();
        probe->address = address;
        probe->socket = new QTcpSocket(&context);
        probe->socket->setProxy(QNetworkProxy::NoProxy);
        probe->deadline = new QTimer(&context);
        probe->deadline->setSingleShot(true);
        Probe* active = probe.get();
        probes.push_back(std::move(probe));
        ++pending;
        QObject::connect(active->socket, &QTcpSocket::connected, &context, [&, active] { complete(active, true); });
        QObject::connect(active->socket, &QTcpSocket::errorOccurred, &context,
                         [&, active](QAbstractSocket::SocketError) { complete(active, false); });
        QObject::connect(active->deadline, &QTimer::timeout, &context, [&, active] { complete(active, false); });
        active->elapsed.start();
        active->deadline->start(SocketTimeoutMs);
        active->socket->connectToHost(address.address(), address.port());
    };

    QObject::connect(&grace, &QTimer::timeout, &context, finish);
    QObject::connect(&nameDeadline, &QTimer::timeout, &context, [&] {
        dnsPending = false;
        abortLookup();
        finishIfComplete();
    });
    QObject::connect(&cancellationPoll, &QTimer::timeout, &context, [&] {
        if (cancelled && cancelled()) { wasCancelled = true; finish(); }
    });
    if (QCoreApplication::instance()) {
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                         &context, [&] { wasCancelled = true; finish(); });
    }

    for (const auto& address : known) addCandidate(address);
    if (dnsPending) {
        nameDeadline.start(ResolveTimeoutMs);
        lookupId = resolver.lookup(hostname, &context, [&](const QHostInfo& info) {
            if (finished || !dnsPending) return;
            dnsPending = false;
            lookupId = -1;
            nameDeadline.stop();
            ++addingCandidates;
            if (info.error() == QHostInfo::NoError) {
                for (const auto& address : info.addresses()) {
                    if (address.protocol() == QAbstractSocket::IPv4Protocol)
                        addCandidate(NvAddress(address, port));
                }
            }
            --addingCandidates;
            finishIfComplete();
        });
        // A test resolver may complete synchronously; the production lookup
        // delivers its result on this context's event loop.
        if (!dnsPending) lookupId = -1;
    }
    --addingCandidates;
    finishIfComplete();
    if (cancelled) cancellationPoll.start();
    if (!finished) loop.exec();

    grace.stop();
    nameDeadline.stop();
    cancellationPoll.stop();
    abortLookup();
    for (const auto& probe : probes) {
        probe->deadline->stop();
        QObject::disconnect(probe->socket, nullptr, &context, nullptr);
        probe->socket->abort();
    }
    // Cancellation wins even when it races the final socket or grace timer.
    wasCancelled = wasCancelled || (cancelled && cancelled());
    return {wasCancelled ? NvAddress() : pickFastest(results), int(probes.size()), wasCancelled};
}
}
