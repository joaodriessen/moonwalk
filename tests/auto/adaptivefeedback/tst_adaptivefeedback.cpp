#include <QtTest>
#include <QtEndian>
#include "transport.h"
#ifdef ADAPTIVE_HOST_CONTROLLER
#include "adaptive_bitrate.h"
#endif

class AdaptiveFeedbackTest : public QObject {
    Q_OBJECT
    QList<QByteArray> drain() {
        QList<QByteArray> reports;
        const int count = AdaptiveTestDrain();
        if (count < 0) return reports;
        for (int i = 0; i < count; ++i) {
            unsigned char wire[256], plain[256];
            int bytes = AdaptiveTestWire(i, wire, sizeof(wire));
            if (bytes != 49 || qFromLittleEndian<quint16>(wire) != 1) return {};
            int size = AdaptiveTestDecrypt(i, plain, sizeof(plain));
            if (size != 25 || qFromLittleEndian<quint16>(plain) != 0x5502 ||
                    qFromLittleEndian<quint16>(plain + 2) != 21) return {};
            reports.append(QByteArray(reinterpret_cast<const char*>(plain + 4), 21));
        }
        return reports;
    }
    static quint32 frame(const QByteArray& report) { return qFromBigEndian<quint32>(report.constData()); }
    static unsigned data(const QByteArray& report) { return qFromBigEndian<quint16>(report.constData() + 10); }
    static unsigned received(const QByteArray& report) { return qFromBigEndian<quint16>(report.constData() + 14); }
private slots:
    void negotiatedProtectionDoesNotMistakeControlForMedia() {
        QCOMPARE(AdaptiveTestEncryption(false, false), 0);
        QCOMPARE(AdaptiveTestEncryption(true, false), 1);
        QCOMPARE(AdaptiveTestEncryption(false, true), 2);
        QCOMPARE(AdaptiveTestEncryption(true, true), 3);
    }
    void offAndOtherHostsPreserveExistingFeedback() {
        AdaptiveTestBegin(false, true);
        QCOMPARE(AdaptiveTestBlock(1, 0, 1, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestBlock(2, 0, 1, 4, 25, 1), 0);
        QCOMPARE(AdaptiveTestBlock(3, 0, 1, 2, 50, 3), 0);
        auto reports = drain();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(frame(reports[0]), 2u);
        QVERIFY(received(reports[0]) < data(reports[0]));
        AdaptiveTestEnd();
        AdaptiveTestBegin(true, false);
        QCOMPARE(AdaptiveTestBlock(1, 0, 1, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0);
        AdaptiveTestEnd();
    }
    void actualCompletionBoundsFreshHealthyReports() {
        AdaptiveTestBegin(true, true);
        QCOMPARE(AdaptiveTestBlock(1, 0, 1, 2, 50, 1), 0); // Partial is not healthy.
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestPacket(1, 0, 1, 2, 50, 0, 1), 1); // Duplicate.
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestBlock(2, 0, 1, 2, 50, 3), 0);
        auto reports = drain();
        QCOMPARE(reports.size(), 2); // Original loss plus actual clean frame.
        QCOMPARE(frame(reports[1]), 2u);
        QCOMPARE(data(reports[1]), received(reports[1]));
        QCOMPARE(AdaptiveTestPacket(2, 0, 1, 2, 50, 2, 6), 1); // Late parity.
        QCOMPARE(drain().size(), 0);
        AdaptiveTestTime(1100000);
        QCOMPARE(AdaptiveTestBlock(3, 0, 1, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0);
        AdaptiveTestTime(1250000);
        QCOMPARE(AdaptiveTestBlock(4, 0, 1, 2, 50, 3), 0);
        reports = drain();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(frame(reports[0]), 4u);
        AdaptiveTestEnd();
    }
    void everyBlockMustBeReceivedWithoutRecovery() {
        AdaptiveTestBegin(true, true);
        QCOMPARE(AdaptiveTestBlock(1, 0, 2, 2, 50, 5), 0); // Actual Reed-Solomon recovery.
        QCOMPARE(AdaptiveTestBlock(1, 1, 2, 2, 50, 3), 0);
        auto reports = drain();
        QCOMPARE(reports.size(), 1); // Recovery report remains; no clean report.
        QVERIFY(received(reports[0]) < data(reports[0]));
        QCOMPARE(AdaptiveTestSubmitted(), 4u);
        AdaptiveTestTime(2000000);
        QCOMPARE(AdaptiveTestBlock(2, 0, 2, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0); // A complete first block is not a complete frame.
        QCOMPARE(AdaptiveTestBlock(2, 1, 2, 2, 50, 3), 0);
        reports = drain();
        QCOMPARE(reports.size(), 1);
        QCOMPARE(frame(reports[0]), 2u);
        QCOMPARE(static_cast<unsigned char>(reports[0][19]), 1);
        QCOMPARE(static_cast<unsigned char>(reports[0][20]), 2);
        QCOMPARE(data(reports[0]), received(reports[0]));
        AdaptiveTestEnd();
    }
    void healthySamplingCannotFillControlQueueAtFrameRate() {
        AdaptiveTestBegin(true, true);
        for (unsigned f = 1; f <= 1000; ++f) {
            AdaptiveTestTime(1000000 + (f - 1) * 1000);
            QCOMPARE(AdaptiveTestBlock(f, 0, 1, 2, 50, 3), 0);
        }
        QCOMPARE(drain().size(), 4);
        AdaptiveTestEnd();
    }
    void missingFecBlockCannotReusePreviousBlockAsHealthy() {
        AdaptiveTestBegin(true, true);
        QCOMPARE(AdaptiveTestBlock(1, 0, 3, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestBlock(1, 2, 3, 2, 50, 3), 1); // Entire middle block missing.
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestSubmitted(), 0u);
        AdaptiveTestEnd();
        AdaptiveTestBegin(false, true);
        AdaptiveTestBlock(1, 0, 3, 2, 50, 3);
        AdaptiveTestBlock(1, 2, 3, 2, 50, 3);
        QCOMPARE(drain().size(), 1); // Off mode preserves upstream behavior.
        AdaptiveTestEnd();
    }
    void reorderedPartialFrameAndQueueReuseDoNotInventHealth() {
        AdaptiveTestBegin(true, true);
        // EOF arriving first is still incomplete. Only actual completion after
        // the missing data arrives can produce a fresh healthy observation.
        QCOMPARE(AdaptiveTestPacket(1, 0, 1, 2, 50, 1, 2), 0);
        QCOMPARE(drain().size(), 0);
        QCOMPARE(AdaptiveTestPacket(1, 0, 1, 2, 50, 0, 1), 0);
        QCOMPARE(drain().size(), 1);
        AdaptiveTestRestartQueueWithoutNegotiation();
        AdaptiveTestTime(2000000);
        QCOMPARE(AdaptiveTestBlock(1, 0, 1, 2, 50, 3), 0);
        QCOMPARE(drain().size(), 0); // Cleanup revoked the previous opt-in.
        AdaptiveTestEnd();
    }
#ifdef ADAPTIVE_HOST_CONTROLLER
    void actualReceiverEncryptedWireDrivesHostDownAndRecovery() {
        AdaptiveTestBegin(true, true);
        video::adaptive_bitrate_controller controller(20000);
        const auto start = std::chrono::steady_clock::time_point{};
        unsigned healthyReports = 0;
        auto consume = [&](int milliseconds) {
            auto reports = drain();
            for (const auto& bytes : reports) {
                auto status = video::parse_frame_fec_status(std::string_view(bytes.constData(), bytes.size()));
                if (!status) return false;
                if (status->received_data == status->data && status->missing == 0) ++healthyReports;
                controller.report(*status, start + std::chrono::milliseconds(milliseconds));
            }
            return !reports.isEmpty();
        };
        AdaptiveTestBlock(1, 0, 1, 2, 50, 3);
        consume(0);
        AdaptiveTestBlock(2, 0, 1, 4, 25, 1);
        AdaptiveTestBlock(3, 0, 1, 2, 50, 3);
        QVERIFY(consume(0));
        QCOMPARE(controller.target(), 15000);
        // No packets/reports during this gap: no synthetic healthy timer signal.
        AdaptiveTestTime(61000000);
        QCOMPARE(drain().size(), 0);
        QCOMPARE(controller.target(), 15000);
        AdaptiveTestBlock(4, 0, 1, 2, 50, 3);
        consume(60000);
        QCOMPARE(controller.target(), 15000); // First fresh frame cannot erase the gap.
        for (unsigned n = 1; n <= 240; ++n) {
            AdaptiveTestTime(61000000 + n * 250000);
            QCOMPARE(AdaptiveTestBlock(4 + n, 0, 1, 2, 50, 3), 0);
            consume(60000 + n * 250);
            QVERIFY(controller.target() >= 15000 && controller.target() <= 20000);
            if (n < 40) QCOMPARE(controller.target(), 15000);
        }
        qInfo() << "receiver/wire/host:" << "loss target" << 15000 << "recovered target"
                << controller.target() << "actual clean reports" << healthyReports;
        QCOMPARE(controller.target(), 20000);
        QVERIFY(healthyReports >= 240);
        AdaptiveTestEnd();
    }
#endif
};
QTEST_GUILESS_MAIN(AdaptiveFeedbackTest)
#include "tst_adaptivefeedback.moc"
