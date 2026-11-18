#include <QtTest>
#include <QTemporaryDir>
#include "../../../app/supportinfo.h"

class SupportInfoTest : public QObject
{
    Q_OBJECT
private slots:
    void boundsLogAndKeepsUsefulSessionFacts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile log(dir.filePath("session.log"));
        QVERIFY(log.open(QIODevice::WriteOnly));
        log.write(QByteArray(100000, 'x'));
        log.write("\nHost version: 0.4.0-alpha.3\nStage: RTSP failed\nDecoder: VAAPI H.264\n");
        log.close();
        const auto result = SupportInfo(log.fileName()).summary();
        QVERIFY(result.size() < 68000);
        QVERIFY(!result.contains(QString(100, 'x')));
        QVERIFY(result.contains("Host version: 0.4.0-alpha.3"));
        QVERIFY(result.contains("Stage: RTSP failed"));
        QVERIFY(result.contains("Decoder: VAAPI H.264"));
        QVERIFY(result.contains("Source:"));
    }

    void removesSecretsWithoutRemovingFailureContext()
    {
        const auto result = SupportInfo::redact(QStringLiteral(
            "RTSP failed at 192.168.1.2\nrikey=abcdef\nclientcert=123456\npin=1234\n/home/someone/log\n"
            "-----BEGIN PRIVATE KEY-----\nsecret\n-----END PRIVATE KEY-----\n"));
        QVERIFY(result.contains("RTSP failed"));
        for (const auto* secret : {"192.168.1.2", "abcdef", "123456", "1234", "someone", "secret"}) {
            QVERIFY(!result.contains(QString::fromLatin1(secret)));
        }
    }

    void removesIpv6WithoutHidingTimings()
    {
        const auto result = SupportInfo::redact(QStringLiteral(
            "Connect [2001:db8::1234]:47984 fe80::abcd%eth0 ::ffff:192.0.2.7 ::1\n"
            "RTT: 12.5 ms at 12:34:56; decoder: VAAPI; version: 0.3.1-alpha.9\n"));
        for (const auto* secret : {"2001:db8", "fe80", "eth0", "::ffff", "192.0.2.7", "::1"}) {
            QVERIFY(!result.contains(QString::fromLatin1(secret)));
        }
        QVERIFY(result.contains("[address]:47984"));
        QVERIFY(result.contains("RTT: 12.5 ms at 12:34:56"));
        QVERIFY(result.contains("version: 0.3.1-alpha.9"));
    }

    void removesUrlCredentialsAndWindowsHomes()
    {
        const auto result = SupportInfo::redact(QStringLiteral(
            "Failed https://alice:p%40ss@host.example:47984/serverinfo\n"
            "C:\\Users\\Alice\\AppData\\log.txt D:/Users/Bob/log.txt\n"));
        for (const auto* secret : {"alice", "p%40ss", "Alice", "Bob"}) {
            QVERIFY(!result.contains(QString::fromLatin1(secret)));
        }
        QVERIFY(result.contains("https://[credentials omitted]@host.example:47984/serverinfo"));
        QVERIFY(result.contains("log.txt"));
    }

    void removesTruncatedPemBlocks()
    {
        const auto head = SupportInfo::redact(QStringLiteral(
            "Stage: pairing\n-----BEGIN PRIVATE KEY-----\nprivate-material\n"));
        QVERIFY(head.contains("Stage: pairing"));
        QVERIFY(!head.contains("private-material"));
        const auto tail = SupportInfo::redact(QStringLiteral(
            "private-material\n-----END CERTIFICATE-----\nStage: failed\n"));
        QVERIFY(!tail.contains("private-material"));
        QVERIFY(tail.contains("Stage: failed"));
    }

    void removesQuotedAndOAuthSecrets()
    {
        const auto result = SupportInfo::redact(QStringLiteral(
            "{\"token\":\"json-secret\"}\naccess_token=query-secret\n"
            "'password': 'quoted-secret'\nrefresh_token=refresh-secret\nStage: failed\n"));
        QVERIFY(!result.contains("secret"));
        QVERIFY(result.contains("Stage: failed"));
    }

    void noLogIsExplicit()
    {
        QVERIFY(SupportInfo(QString()).summary().contains("No application log is available"));
    }

    void consoleOnlyBuildRetainsBoundedRecentFacts()
    {
        SupportInfo::recordLog(QString(100000, 'x'));
        SupportInfo::recordLog(QStringLiteral("\nHost: Skylight 0.4.0-alpha.3\nStage: connected\nDecoder: VAAPI\n"));
        const auto summary = SupportInfo(QString()).summary();
        QVERIFY(summary.size() < 68000);
        QVERIFY(!summary.contains(QString(100, 'x')));
        QVERIFY(summary.contains("Stage: connected"));
        QVERIFY(summary.contains("Decoder: VAAPI"));
        QVERIFY(summary.contains("in memory"));
    }
};

QTEST_GUILESS_MAIN(SupportInfoTest)
#include "tst_supportinfo.moc"
