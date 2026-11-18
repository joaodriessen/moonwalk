#include <QWindow>
#include <QtTest>
#include <QTcpServer>
#include <QUdpSocket>
#include <QPointer>

#include <type_traits>

#include "streaming/session.h"
#include "streaming/connectiondiagnostic.h"
#include "streaming/cursormodepolicy.h"

class SessionWindowContractTest : public QObject
{
    Q_OBJECT

private slots:
    void automaticDesktopPointerKeepsGamesAndExplicitChoicesIndependent() {
        QVERIFY(CursorModePolicy::absoluteInput(true, false, true));
        QVERIFY(!CursorModePolicy::absoluteInput(true, true, false));
        QVERIFY(!CursorModePolicy::absoluteInput(false, false, true));
        QVERIFY(CursorModePolicy::absoluteInput(false, true, false));
    }
    void extraHostCodecBitsDoNotChangeSupportedFormats() {
        SupportedVideoFormatList formats;
        formats << VIDEO_FORMAT_H264 << VIDEO_FORMAT_H265;
        QCOMPARE(formats.maskByServerCodecModes(SCM_H264 | (1 << 29)), int(VIDEO_FORMAT_H264));
    }
    void initializeAcceptsBaseQtWindow();
    void diagnosticUsesReferenceAndEchoProtocols();
    void diagnosticDeadlineAndCancellationAreFinal();
    void diagnosticUnavailableReferenceIsInconclusive();
    void destroyingDiagnosticOwnerStopsDelivery();
    void cursorContractSurvivesInputModeChanges();
    void cursorOverrideIsSharedWithHostNegotiation();
};

void SessionWindowContractTest::cursorContractSurvivesInputModeChanges()
{
    CursorModePolicy relative(true, false, true);
    QVERIFY(!relative.clientSideCursor());
    QVERIFY(!relative.initiallyShowLocalCursor());
    QVERIFY(!relative.syncShapes(true));
    QVERIFY(relative.needsReconnect(true));
    QVERIFY(!relative.mayToggleLocalCursor());

    CursorModePolicy absolute(true, true, true);
    QVERIFY(absolute.clientSideCursor());
    QVERIFY(absolute.initiallyShowLocalCursor());
    QVERIFY(!absolute.syncShapes(false));
    QVERIFY(absolute.syncShapes(true));
    QVERIFY(!absolute.needsReconnect(true));

    CursorModePolicy otherHost(true, false, false);
    QVERIFY(otherHost.initiallyShowLocalCursor());
    QVERIFY(otherHost.mayToggleLocalCursor());
    QVERIFY(!otherHost.syncShapes(true));
    QVERIFY(!otherHost.needsReconnect(true));
}

void SessionWindowContractTest::cursorOverrideIsSharedWithHostNegotiation()
{
    CursorModePolicy forced(false, true, true, 1);
    QVERIFY(forced.clientSideCursor());
    QVERIFY(forced.initiallyShowLocalCursor());
    QVERIFY(forced.syncShapes(true));
    CursorModePolicy disabled(false, true, true, -1);
    QVERIFY(!disabled.clientSideCursor());
    QVERIFY(!disabled.initiallyShowLocalCursor());
}

void SessionWindowContractTest::diagnosticUsesReferenceAndEchoProtocols()
{
    QTcpServer reference;
    QVERIFY(reference.listen(QHostAddress::LocalHost));
    QUdpSocket echo;
    QVERIFY(echo.bind(QHostAddress::LocalHost, 0));
    QByteArray received;
    connect(&echo, &QUdpSocket::readyRead, this, [&] {
        QHostAddress sender;
        quint16 port;
        received.resize(int(echo.pendingDatagramSize()));
        echo.readDatagram(received.data(), received.size(), &sender, &port);
        echo.writeDatagram(received, sender, port);
    });
    ConnectionDiagnostic diagnostic;
    QSignalSpy done(&diagnostic, &ConnectionDiagnostic::finished);
    diagnostic.start("localhost", reference.serverPort(),
                     {{1, reference.serverPort(), false}, {2, echo.localPort(), true}}, 1000);
    QCOMPARE(done.count(), 0); // Optional diagnostics never block failure publication.
    QTRY_COMPARE(done.count(), 1);
    QCOMPARE(done.first().first().toInt(), 0);
    QCOMPARE(received.size(), 1040);
    QVERIFY(received.startsWith("moonlight-ctest"));
}

void SessionWindowContractTest::diagnosticDeadlineAndCancellationAreFinal()
{
    QTcpServer reference;
    QVERIFY(reference.listen(QHostAddress::LocalHost));
    QUdpSocket silent;
    QVERIFY(silent.bind(QHostAddress::LocalHost, 0));
    ConnectionDiagnostic diagnostic;
    QSignalSpy done(&diagnostic, &ConnectionDiagnostic::finished);
    diagnostic.start("127.0.0.1", reference.serverPort(), {{4, silent.localPort(), true}}, 80);
    QTRY_COMPARE(done.count(), 1);
    QCOMPARE(done.first().first().toInt(), 4);
    diagnostic.cancel();
    QTest::qWait(120);
    QCOMPARE(done.count(), 1);

    ConnectionDiagnostic canceled;
    QSignalSpy canceledDone(&canceled, &ConnectionDiagnostic::finished);
    canceled.start("127.0.0.1", reference.serverPort(), {{4, silent.localPort(), true}}, 80);
    canceled.cancel();
    QCOMPARE(canceledDone.count(), 1);
    QCOMPARE(canceledDone.first().first().toInt(), -1);
    QTest::qWait(120);
    QCOMPARE(canceledDone.count(), 1);
}

void SessionWindowContractTest::diagnosticUnavailableReferenceIsInconclusive()
{
    QTcpServer reference;
    QVERIFY(reference.listen(QHostAddress::LocalHost));
    const auto closedPort = reference.serverPort();
    reference.close();
    ConnectionDiagnostic diagnostic;
    QSignalSpy done(&diagnostic, &ConnectionDiagnostic::finished);
    diagnostic.start("127.0.0.1", closedPort, {{1, closedPort, false}}, 200);
    QTRY_COMPARE(done.count(), 1);
    QCOMPARE(done.first().first().toInt(), -1);
}

void SessionWindowContractTest::destroyingDiagnosticOwnerStopsDelivery()
{
    auto owner = new QObject;
    auto diagnostic = new ConnectionDiagnostic(owner);
    QPointer<ConnectionDiagnostic> guard = diagnostic;
    int deliveries = 0;
    connect(diagnostic, &ConnectionDiagnostic::finished, this, [&](int) { ++deliveries; });
    diagnostic->start("localhost", 1, {{1, 1, false}}, 80);
    delete owner;
    QVERIFY(guard.isNull());
    QTest::qWait(120);
    QCOMPARE(deliveries, 0);
}

void SessionWindowContractTest::initializeAcceptsBaseQtWindow()
{
    using Initialize = bool (Session::*)(QWindow*);
    static_assert(std::is_same_v<decltype(&Session::initialize), Initialize>);
}

QTEST_GUILESS_MAIN(SessionWindowContractTest)

#include "tst_sessionwindowcontract.moc"
