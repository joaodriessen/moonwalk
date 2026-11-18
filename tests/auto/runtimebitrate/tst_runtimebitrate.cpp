#include <QtTest>
#include "streaming/runtimebitrate.h"
class RuntimeBitrateTest : public QObject {
    Q_OBJECT
private slots:
    void init(){ RuntimeTransport::reset(); }
    void policyCutsQuicklyRecoversSlowlyAndRespectsCeiling() {
        QCOMPARE(RuntimeBitratePolicy::next(40000,40000,true,true,2999),40000);
        QCOMPARE(RuntimeBitratePolicy::next(40000,40000,true,true,3000),30000);
        QCOMPARE(RuntimeBitratePolicy::next(30000,40000,false,true,9999),30000);
        QCOMPARE(RuntimeBitratePolicy::next(30000,40000,false,true,10000),33000);
        QCOMPARE(RuntimeBitratePolicy::next(30000,40000,false,false,30000),30000);
        QCOMPARE(RuntimeBitratePolicy::next(39000,40000,false,true,10000),40000);
        QCOMPARE(RuntimeBitratePolicy::next(700,700,true,true,3000),700);
    }
    void protocolRequiresFeatureAndSuccessfulAcknowledgement() {
        QVERIFY(RuntimeBitratePolicy::supported("{\"supported\":false,\"version\":1,\"features\":[\"runtime_bitrate\"]}"));
        QVERIFY(!RuntimeBitratePolicy::supported("{\"supported\":true}"));
        QCOMPARE(RuntimeBitratePolicy::applied("<root status_code=\"200\"><bitrate>24000</bitrate></root>"),24000);
        QCOMPARE(RuntimeBitratePolicy::applied("<root status_code=\"403\"><bitrate>24000</bitrate></root>"),0);
        QCOMPARE(RuntimeBitratePolicy::applied("<root status_code=\"200\"><bitrate>24000"),0);
    }
    void workerCoalescesManualRequestsAndUsesHostAcknowledgement() {
        RuntimeBitrate worker(nullptr,40000,false,[]{return RuntimeBitrate::Observation{false,true};});
        QSignalSpy changes(&worker,&RuntimeBitrate::changed);
        worker.requestBitrate(30000); worker.requestBitrate(20000); worker.start();
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(),2,1000);
        // The host hears the encoder's share of 20000; the answer comes back in launch units.
        QCOMPARE(RuntimeTransport::writes.load(),1); QCOMPARE(RuntimeTransport::lastTarget.load(),14988);
        QCOMPARE(changes.last()[1].toInt(),20000);
        worker.stopAndWait();
    }
    void runtimeValuesAreInTheEncodersUnits() {
        // Gaming PC's log: a 53000 kbps launch made a 41388 kbps encoder.
        QCOMPARE(RuntimeBitratePolicy::encoderKbps(53000),41388);
        // The Mini PC launched at 27179 and was then sent 21200 raw as a "decrease",
        // which raised its encoder and rebuilt it. In encoder units it stays a decrease.
        QVERIFY(RuntimeBitratePolicy::encoderKbps(27179) < 21200);
        QVERIFY(RuntimeBitratePolicy::encoderKbps(RuntimeBitratePolicy::wireKbps(19000)) < RuntimeBitratePolicy::encoderKbps(27179));
        QVERIFY(RuntimeBitratePolicy::encoderKbps(27179,6) < RuntimeBitratePolicy::encoderKbps(27179));
        for (int encoder=500; encoder<=150000; encoder+=137) {
            const int wire=RuntimeBitratePolicy::wireKbps(encoder);
            QVERIFY(RuntimeBitratePolicy::encoderKbps(wire)>=encoder);
            QVERIFY(RuntimeBitratePolicy::encoderKbps(wire-1)<encoder);
        }
        for (int wire=500; wire<=150000; wire+=500)
            QCOMPARE(RuntimeBitratePolicy::wireKbps(RuntimeBitratePolicy::encoderKbps(wire)),wire);
    }
    void nothingIsSentBeforeVideoOrTwice() {
        std::atomic<bool> video{false};
        RuntimeBitrate worker(nullptr,40000,false,[&video]{return RuntimeBitrate::Observation{false,video.load()};});
        QSignalSpy confirmed(&worker,&RuntimeBitrate::applied);
        worker.requestBitrate(20000); worker.start();
        QTest::qWait(300);
        QCOMPARE(RuntimeTransport::writes.load(),0);
        video=true;
        QTRY_COMPARE_WITH_TIMEOUT(confirmed.size(),1,1000);
        QCOMPARE(RuntimeTransport::writes.load(),1);
        // The same for the encoder, so nothing to restart it for.
        worker.requestBitrate(20000); worker.requestBitrate(20001);
        QTest::qWait(300);
        QCOMPARE(RuntimeTransport::writes.load(),1);
        worker.stopAndWait();
    }
    void unsupportedHostReceivesNoBitrateWrites() {
        RuntimeTransport::unsupported=true;
        RuntimeBitrate worker(nullptr,40000,true,[]{return RuntimeBitrate::Observation{true,true};});
        QSignalSpy changes(&worker,&RuntimeBitrate::changed); worker.start();
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(),1,1000);
        QVERIFY(!changes.first()[0].toBool()); QCOMPARE(RuntimeTransport::writes.load(),0);
        worker.stopAndWait();
    }
    void rejectedTargetDoesNotBecomeCurrentAndStopCancelsRequests() {
        RuntimeTransport::reject=true;
        RuntimeBitrate worker(nullptr,40000,false,[]{return RuntimeBitrate::Observation{false,true};});
        QSignalSpy changes(&worker,&RuntimeBitrate::changed); worker.requestBitrate(20000); worker.start();
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(),2,1000);
        QCOMPARE(changes.last()[1].toInt(),40000);
        QVERIFY(!changes.last()[2].toString().isEmpty()); worker.stopAndWait();
        RuntimeTransport::hold=true;
        RuntimeBitrate held(nullptr,40000,false,[]{return RuntimeBitrate::Observation{false,true};}); held.start();
        QTest::qWait(20); QElapsedTimer timer; timer.start(); held.stopAndWait();
        QVERIFY(timer.elapsed()<300);
    }
};
QTEST_GUILESS_MAIN(RuntimeBitrateTest)
#include "tst_runtimebitrate.moc"
