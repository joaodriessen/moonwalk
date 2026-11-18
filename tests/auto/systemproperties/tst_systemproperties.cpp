#include "SDL_compat.h"
#include "backend/systemproperties.h"
#include <QGuiApplication>
#include <QtTest>
#include <thread>

class SystemPropertiesTest : public QObject {
    Q_OBJECT
private slots:
    void init() { ProbeFixture::reset(); }
    void refreshRateFollowsCurrentModeInsteadOfFastestSupportedMode() {
        ProbeFixture::displayPresent = true;
        SystemProperties properties;
        properties.refreshDisplays();
        QCOMPARE(properties.getRefreshRate(0), 60);
        ProbeFixture::currentRefreshRate = 120;
        properties.refreshDisplays();
        QCOMPARE(properties.getRefreshRate(0), 120);
        ProbeFixture::displayPresent = false;
        properties.refreshDisplays();
        QCOMPARE(properties.getRefreshRate(0), 0);
    }
    void videoInitializationFailureSettlesAndCanBeRetried() {
        SystemProperties properties;
        ProbeFixture::initFails = true;
        properties.startAsyncLoad();
        QVERIFY(!properties.isDecoderProbeReady());
        QVERIFY(!properties.property("decoderProbeError").toString().isEmpty());
        QVERIFY(!properties.property("decoderProbeRunning").toBool());
        QCOMPARE(ProbeFixture::references,0);
        ProbeFixture::initFails = false;
        properties.startAsyncLoad();
        QTRY_VERIFY_WITH_TIMEOUT(properties.isDecoderProbeReady(),1000);
        QVERIFY(properties.property("decoderProbeError").toString().isEmpty());
        QCOMPARE(ProbeFixture::references,0);
    }
    void failedWindowReleasesVideoAndDoesNotClaimReady() {
        SystemProperties properties;
        ProbeFixture::windowFails = true;
        properties.startAsyncLoad();
        QVERIFY(!properties.isDecoderProbeReady());
        QVERIFY(!properties.property("decoderProbeError").toString().isEmpty());
        QVERIFY(!properties.property("decoderProbeRunning").toBool());
        QCOMPARE(ProbeFixture::references,0);
    }
    void unavailableDecoderPublishesSafeCapabilitiesAndFailure() {
        SystemProperties properties;
        ProbeFixture::usable = false;
        properties.startAsyncLoad();
        QTRY_VERIFY_WITH_TIMEOUT(ProbeFixture::destroyed == 1,1000);
        QVERIFY(!properties.isDecoderProbeReady());
        QVERIFY(!properties.property("decoderProbeError").toString().isEmpty());
        QVERIFY(!properties.property("hasHardwareAcceleration").toBool());
        QVERIFY(!properties.property("supportsHdr").toBool());
        QCOMPARE(properties.property("maximumResolution").toSize(),QSize());
        QVERIFY(!ProbeFixture::destroyedBeforeReturn);
    }
    void nativeThreadCleanupCannotBlockCancellationOrReleaseSdlEarly() {
        SystemProperties properties;
        ProbeFixture::holdThreadExit = true;
        std::atomic<bool> releaseRequested{false}, failsafeReleased{false};
        std::thread release([&] {
            QElapsedTimer deadline; deadline.start();
            while (!releaseRequested && deadline.elapsed() < 5000) QThread::msleep(1);
            failsafeReleased = !releaseRequested;
            ProbeFixture::holdThreadExit = false;
        });
        properties.startAsyncLoad();
        QElapsedTimer enteredDeadline; enteredDeadline.start();
        while (!ProbeFixture::threadExitEntered && enteredDeadline.elapsed() < 1000) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        const bool entered = ProbeFixture::threadExitEntered;
        bool guiHeartbeatWhileHeld = false;
        QTimer::singleShot(0, &properties, [&] {
            guiHeartbeatWhileHeld = ProbeFixture::holdThreadExit && !ProbeFixture::threadExitReturned;
        });
        QTest::qWait(60);
        const bool running = properties.property("decoderProbeRunning").toBool();
        const bool ready = properties.isDecoderProbeReady();
        const int destroyed = ProbeFixture::destroyed;
        releaseRequested = true;
        release.join();
        QTRY_VERIFY_WITH_TIMEOUT(properties.isDecoderProbeReady(),1000);
        QVERIFY(entered);
        QVERIFY2(!failsafeReleased, "Native TLS teardown blocked the GUI until the failsafe released it");
        QVERIFY(guiHeartbeatWhileHeld);
        QVERIFY(running); QVERIFY(!ready); QCOMPARE(destroyed,0);
        QVERIFY(!ProbeFixture::destroyedBeforeReturn);
    }
    void timeoutDoesNotReleaseLiveProbeOrPermitConcurrentRetry() {
        SystemProperties properties;
        ProbeFixture::hold = true;
        properties.startAsyncLoad();
        QTRY_VERIFY_WITH_TIMEOUT(ProbeFixture::entered.load(),1000);
        QTest::qWait(60);
        const bool timedOut = !properties.property("decoderProbeError").toString().isEmpty();
        const bool readyDuringProbe = properties.isDecoderProbeReady();
        const bool runningDuringProbe = properties.property("decoderProbeRunning").toBool();
        const int destroyedDuringProbe = ProbeFixture::destroyed;
        properties.startAsyncLoad();
        ProbeFixture::hold = false;
        QTRY_VERIFY_WITH_TIMEOUT(ProbeFixture::destroyed == 1,1000);
        QVERIFY(timedOut);
        QVERIFY(!readyDuringProbe);
        QVERIFY(runningDuringProbe);
        QCOMPARE(destroyedDuringProbe,0);
        QVERIFY(!ProbeFixture::destroyedBeforeReturn);
        QVERIFY(properties.isDecoderProbeReady());
        QVERIFY(properties.property("decoderProbeError").toString().isEmpty());
        QVERIFY(!properties.property("decoderProbeRunning").toBool());
        QCOMPARE(ProbeFixture::references,0);
    }
};
QTEST_MAIN(SystemPropertiesTest)
#include "tst_systemproperties.moc"
