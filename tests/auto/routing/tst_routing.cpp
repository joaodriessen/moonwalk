#include <QtTest>
#include <QTcpServer>
#include <QPointer>
#include "backend/addressprobe.h"

class RoutingTest : public QObject {
    Q_OBJECT
    struct DelayedResolver {
        int calls = 0, aborted = 0, delivered = 0;
        QPointer<QObject> context;
        AddressProbe::Resolver make(int delay, QList<QHostAddress> addresses) {
            return {
                [this, delay, addresses](const QString&, QObject* owner, AddressProbe::Resolved ready) {
                    ++calls; context = owner;
                    QTimer::singleShot(delay, owner, [this, addresses, ready] {
                        ++delivered;
                        QHostInfo info; info.setAddresses(addresses); ready(info);
                    });
                    return 42;
                },
                [this](int id) { QCOMPARE(id, 42); ++aborted; }
            };
        }
    };
private slots:
    void healthyRouteDoesNotWaitForDnsAndDestroysItsCallbacks() {
        QTcpServer healthy;
        QVERIFY(healthy.listen(QHostAddress::LocalHost));
        const NvAddress address(QHostAddress::LocalHost, healthy.serverPort());
        DelayedResolver dns;
        QElapsedTimer timer; timer.start();
        const auto result = AddressProbe::run({address}, "owned.invalid", address.port(), {},
                                             dns.make(500, {QHostAddress::LocalHost}));
        QVERIFY(result.address == address);
        QVERIFY2(timer.elapsed() < 300, "a live route must not wait for fresh DNS");
        QCOMPARE(dns.aborted, 1);
        QVERIFY(dns.context.isNull());
        QTest::qWait(550);
        QCOMPARE(dns.delivered, 0);
        QVERIFY(healthy.hasPendingConnections());
        auto* peer = healthy.nextPendingConnection();
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
    }

    void liveRouteDoesNotJoinAnUnansweredTcpProbe() {
        QTcpServer healthy, stalled;
        QVERIFY(healthy.listen(QHostAddress::LocalHost));
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        stalled.setListenBacklogSize(1);
#endif
        QVERIFY(stalled.listen(QHostAddress::LocalHost));
        stalled.pauseAccepting();
        // Fill only this owned listener's kernel queue. The next loopback
        // SYN gets no answer, without firewall or network-policy changes.
        std::vector<std::unique_ptr<QTcpSocket>> fillers;
        bool saturated = false;
        for (int index = 0; index < 16; ++index) {
            auto socket = std::make_unique<QTcpSocket>();
            socket->connectToHost(QHostAddress::LocalHost, stalled.serverPort());
            if (!socket->waitForConnected(30)) { saturated = true; break; }
            fillers.push_back(std::move(socket));
        }
        QVERIFY2(saturated, "owned loopback queue must actually be saturated");
        const NvAddress address(QHostAddress::LocalHost, healthy.serverPort());
        QElapsedTimer timer; timer.start();
        const auto result = AddressProbe::run({NvAddress(QHostAddress::LocalHost, stalled.serverPort()), address}, "", address.port());
        QVERIFY(result.address == address);
        QCOMPARE(result.candidates, 2);
        QVERIFY2(timer.elapsed() < 300, "return must abort the slow socket instead of waiting for its400ms deadline");
    }

    void lateDnsRecoversFailedKnownRoute() {
        QTcpServer healthy;
        QVERIFY(healthy.listen(QHostAddress::LocalHost));
        DelayedResolver dns;
        const NvAddress address(QHostAddress::LocalHost, healthy.serverPort());
        const auto result = AddressProbe::run({NvAddress("127.0.0.2", address.port())}, "owned.invalid", address.port(), {},
                                             dns.make(100, {QHostAddress::LocalHost}));
        QVERIFY(result.address == address);
        QCOMPARE(dns.delivered, 1);
        QCOMPARE(result.candidates, 2);
    }

    void cancellationAbortsDnsWithoutReturningARoute() {
        DelayedResolver dns;
        bool cancelled = false;
        QTimer::singleShot(25, [&] { cancelled = true; });
        QElapsedTimer timer; timer.start();
        const auto result = AddressProbe::run({}, "owned.invalid", 47989, [&] { return cancelled; }, dns.make(1000, {}));
        QVERIFY(result.cancelled);
        QVERIFY(result.address.isNull());
        QVERIFY(timer.elapsed() < 200);
        QCOMPARE(dns.aborted, 1);
        QVERIFY(dns.context.isNull());
        DelayedResolver neverStarted;
        const auto preCancelled = AddressProbe::run({}, "owned.invalid", 47989, [] { return true; }, neverStarted.make(0, {}));
        QVERIFY(preCancelled.cancelled);
        QCOMPARE(neverStarted.calls, 0);
    }

    void cancellationDuringSuccessGraceDoesNotReturnTheLiveRoute() {
        QTcpServer healthy;
        QVERIFY(healthy.listen(QHostAddress::LocalHost));
        DelayedResolver dns;
        bool cancelled = false;
        QTimer::singleShot(25, [&] { cancelled = true; });
        const auto result = AddressProbe::run({NvAddress(QHostAddress::LocalHost, healthy.serverPort())},
            "owned.invalid", healthy.serverPort(), [&] { return cancelled; }, dns.make(1000, {}));
        QVERIFY(result.cancelled);
        QVERIFY(result.address.isNull());
        QCOMPARE(dns.aborted, 1);
        QVERIFY(dns.context.isNull());
    }

    void numericRouteNeedsNeitherListenerNorDatagrams_data() {
        QTest::addColumn<QHostAddress>("address");
        QTest::newRow("IPv4") << QHostAddress(QHostAddress::LocalHost);
        QTest::newRow("IPv6") << QHostAddress(QHostAddress::LocalHostIPv6);
    }
    void numericRouteNeedsNeitherListenerNorDatagrams() {
        QFETCH(QHostAddress, address);
        QUdpSocket receiver;
        QVERIFY(receiver.bind(address));
        QElapsedTimer timer; timer.start();
        const auto route = AddressProbe::localRoute(NvAddress(address, receiver.localPort()));
        QCOMPARE(route.first, address);
        QCOMPARE(route.second, address);
        QVERIFY(timer.elapsed() < 200);
        QCoreApplication::processEvents();
        QVERIFY(!receiver.hasPendingDatagrams());
        const quint16 unusedPort = receiver.localPort();
        receiver.close();
        const auto withoutListener = AddressProbe::localRoute(NvAddress(address, unusedPort));
        QCOMPARE(withoutListener.first, address);
        QCOMPARE(withoutListener.second, address);
        const auto cancelled = AddressProbe::localRoute(NvAddress(address, unusedPort), [] { return true; });
        QVERIFY(cancelled.first.isNull());
    }

    void deduplicationCapAndSingleRouteUseActualPorts() {
        QTcpServer healthy;
        QVERIFY(healthy.listen(QHostAddress::LocalHost));
        const NvAddress address(QHostAddress::LocalHost, healthy.serverPort());
        DelayedResolver dns;
        QList<QHostAddress> addresses;
        for (int index = 1; index <= 12; ++index) addresses.append(QHostAddress(QString("127.0.0.%1").arg(index)));
        const auto capped = AddressProbe::run({address, address}, "owned.invalid", address.port(), {}, dns.make(0, addresses));
        QVERIFY(capped.address == address);
        QCOMPARE(capped.candidates, AddressProbe::MaximumCandidates);
        const auto single = AddressProbe::run({address}, "", address.port());
        QVERIFY(single.address == address);
        QCOMPARE(single.candidates, 1);
        const auto empty = AddressProbe::run({}, "", address.port());
        QVERIFY(empty.address.isNull());
    }

    void hostnameRouteFallbackRemainsCancellable() {
        QTcpServer stalled;
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        stalled.setListenBacklogSize(1);
#endif
        QVERIFY(stalled.listen(QHostAddress::AnyIPv4));
        stalled.pauseAccepting();
        std::vector<std::unique_ptr<QTcpSocket>> fillers;
        bool saturated = false;
        for (int index = 0; index < 16; ++index) {
            auto socket = std::make_unique<QTcpSocket>();
            socket->connectToHost("localhost", stalled.serverPort());
            if (!socket->waitForConnected(100)) { saturated = true; break; }
            fillers.push_back(std::move(socket));
        }
        QVERIFY(saturated);
        bool cancelled = false;
        QTimer::singleShot(25, [&] { cancelled = true; });
        QElapsedTimer timer; timer.start();
        const auto route = AddressProbe::localRoute(NvAddress("localhost", stalled.serverPort()), [&] { return cancelled; });
        QVERIFY(cancelled);
        QVERIFY(route.first.isNull());
        QVERIFY(timer.elapsed() < 200);
    }

    void selectionPreservesFastestAndFirstTie() {
        const NvAddress first("127.0.0.1", 1), second("127.0.0.1", 2);
        QVERIFY(AddressProbe::pickFastest({{first, 5}, {second, 5}}) == first);
        QVERIFY(AddressProbe::pickFastest({{first, 5}, {second, 4}}) == second);
        QVERIFY(AddressProbe::pickFastest({{first, -1}, {second, -1}}).isNull());
    }
};
QTEST_GUILESS_MAIN(RoutingTest)
#include "tst_routing.moc"
