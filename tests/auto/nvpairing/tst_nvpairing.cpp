#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QElapsedTimer>
#include <QFileInfo>
#include <cstring>
#include "backend/nvpairingmanager.h"
#include "backend/nvcomputer.h"

// This suite exercises HTTP/pairing, without linking the streaming engine.
extern "C" const char* LiGetLaunchUrlQueryParameters() { return ""; }

class NvPairingManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void hostErrorsReadProperlyWithOrWithoutWords()
    {
        // A host can refuse with a code and no message; callers put this in brackets.
        QCOMPARE(GfeHttpResponseException(400, "").toQString(), QStringLiteral("Error 400"));
        QCOMPARE(GfeHttpResponseException(503, "Busy").toQString(), QStringLiteral("Busy (Error 503)"));
    }

    void initTestCase() { IdentityManager::get()->getSslConfig(); }
    void identitySettingsArePrivate();
    void binaryKeyPreservesEmbeddedNul();
    void malformedCiphertextIsRejected();
    void challengeBounds_data();
    void challengeBounds();
    void rawResponses_data();
    void rawResponses();
    void truncatedStatusErrorCannotMasqueradeAsCompletedResponse();
    void chunkedResponses_data();
    void chunkedResponses();
    void cancellationBeforeArmIsSticky();
    void cancellationDuringArmIsSticky();
    void interruptionBeforeOrDuringArmIsConsumed_data();
    void interruptionBeforeOrDuringArmIsConsumed();
    void rawDeadlineStillReportsTimeout();
    void postCancellation_data();
    void postCancellation();
    void postHttpErrors_data();
    void postHttpErrors();
    void cancellationAbortsStartedLaunch();
    void cancellationAbortsStartedPairing();
    void requestLogsOmitProtocolArguments();
    void staleResumeRecovery_data();
    void staleResumeRecovery();
};

void NvPairingManagerTest::identitySettingsArePrivate()
{
#ifdef Q_OS_LINUX
    const QSettings settings;
    const QFileInfo profile(settings.fileName());
    QVERIFY(profile.isFile());
    const auto publicPermissions = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
        QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    QCOMPARE(profile.permissions() & publicPermissions, QFileDevice::Permissions());
    QVERIFY(!settings.value("key").toByteArray().isEmpty());
#endif
}

void NvPairingManagerTest::binaryKeyPreservesEmbeddedNul()
{
    const auto salt = QByteArray::fromHex("0a000000000000000000000000000000");
    const auto expected = QByteArray::fromHex("2756691500394946d808bb2bdf20096b");
    const auto key = NvPairingManager::deriveAesKey(salt + "1234", QCryptographicHash::Sha256);
    QCOMPARE(key, expected);
    QCOMPARE(key.size(), 16);
    const auto plaintext = QByteArray::fromHex("00112233445566778899aabbccddeeff");
    const auto ciphertext = NvPairingManager::encrypt(plaintext, key);
    QCOMPARE(ciphertext.size(), 16);
    QCOMPARE(NvPairingManager::decrypt(ciphertext, key), plaintext);
}

void NvPairingManagerTest::malformedCiphertextIsRejected()
{
    for (int size : {0, 1, 15, 17, 31, 47}) {
        QVERIFY(NvPairingManager::decrypt(QByteArray(size, 'x'), QByteArray(16, 'k')).isEmpty());
    }
    QVERIFY(NvPairingManager::decrypt(QByteArray(16, 'x'), QByteArray(4, 'k')).isEmpty());
}

static void respond(QTcpSocket* socket, const QByteArray& body)
{
    socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                  "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}

void NvPairingManagerTest::requestLogsOmitProtocolArguments()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int requestsWithArguments = 0;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            const auto request = socket->property("request").toByteArray() + socket->readAll();
            socket->setProperty("request", request);
            if (!request.contains("\r\n\r\n")) return;
            requestsWithArguments += request.contains("clientcert=fixture-certificate");
            respond(socket, "ok");
        });
    });

    const auto port = server.serverPort();
    const QString endpoint = QStringLiteral("http://127.0.0.1:%1").arg(port);
    const QString command = QStringLiteral("pair");
    const QString arguments = QStringLiteral("clientcert=fixture-certificate");
    const auto loggedUrl = endpoint + "/" + command;
    NvHTTP http(NvAddress("127.0.0.1", port), port, {}, true);
    QTest::ignoreMessage(QtInfoMsg, qPrintable("Executing request: \"" + loggedUrl + "\""));
    QCOMPARE(http.openConnectionToString(QUrl(endpoint), command, arguments, 2000), QString("ok"));
    QTest::ignoreMessage(QtInfoMsg, qPrintable("Executing POST request: \"" + loggedUrl + "\""));
    QCOMPARE(http.openConnectionPost(QUrl(endpoint), command, arguments, {}, "text/plain", 2000), QString("ok"));
    QTest::ignoreMessage(QtInfoMsg, qPrintable("Executing long-poll request: \"" + loggedUrl + "\""));
    QCOMPARE(http.openConnectionRaw(QUrl(endpoint), command, arguments, 2000, {}, NvHTTP::NVLL_VERBOSE).body,
             QByteArray("ok"));
    QCOMPARE(requestsWithArguments, 3);
}

void NvPairingManagerTest::challengeBounds_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<int>("size");
    for (const auto& version : {QString("7.0.0.0"), QString("6.0.0.0")}) {
        for (int size : {0, 16, 32, 47, 48}) {
            QTest::newRow(qPrintable(version + "/" + QString::number(size))) << version << size;
        }
    }
}

void NvPairingManagerTest::challengeBounds()
{
    QFETCH(QString, version);
    QFETCH(int, size);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const auto algorithm = version.startsWith('7') ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1;
    QByteArray key;
    bool reachedServerChallenge = false;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            auto request = socket->property("request").toByteArray() + socket->readAll();
            socket->setProperty("request", request);
            if (!request.contains("\r\n\r\n")) return;
            const auto path = request.split(' ').value(1);
            QUrlQuery query(QUrl("http://localhost" + QString::fromLatin1(path)));
            QByteArray fields = "<paired>1</paired>";
            if (query.queryItemValue("phrase") == "getservercert") {
                key = NvPairingManager::deriveAesKey(
                    QByteArray::fromHex(query.queryItemValue("salt").toLatin1()) + "1234", algorithm);
                fields += "<plaincert>" + IdentityManager::get()->getCertificate().toHex() + "</plaincert>";
            }
            else if (query.hasQueryItem("clientchallenge")) {
                // Aligned values exercise the decrypted-length guard; 47 also
                // verifies malformed ECB input never reaches the parser.
                const auto encrypted = size % 16 == 0 ?
                    NvPairingManager::encrypt(QByteArray(size, 'x'), key) : QByteArray(size, 'x');
                fields += "<challengeresponse>" + encrypted.toHex() + "</challengeresponse>";
            }
            else if (query.hasQueryItem("serverchallengeresp")) {
                reachedServerChallenge = true;
                fields = "<paired>0</paired>";
            }
            respond(socket, "<root status_code=\"200\">" + fields + "</root>");
        });
    });
    NvComputer computer;
    computer.activeAddress = NvAddress("127.0.0.1", server.serverPort());
    computer.activeHttpsPort = server.serverPort();
    NvPairingManager manager(&computer);
    QSslCertificate certificate;
    QCOMPARE(manager.pair(version, "1234", certificate), NvPairingManager::FAILED);
    QCOMPARE(reachedServerChallenge, size == 48);
    QVERIFY(certificate.isNull());
}

void NvPairingManagerTest::rawResponses_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("complete");
    QTest::addColumn<bool>("disconnect");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("complete-200") << 200 << true << true << false;
    QTest::newRow("complete-404") << 404 << true << true << false;
    QTest::newRow("complete-503") << 503 << true << true << false;
    QTest::newRow("bodyless-204") << 204 << true << true << false;
    QTest::newRow("bodyless-304-declared-entity") << 304 << true << true << false;
    QTest::newRow("partial-200-disconnect") << 200 << false << true << false;
    QTest::newRow("partial-404-disconnect") << 404 << false << true << false;
    QTest::newRow("partial-503-disconnect") << 503 << false << true << false;
    QTest::newRow("partial-200-timeout") << 200 << false << false << false;
    QTest::newRow("partial-200-cancel") << 200 << false << false << true;
}

void NvPairingManagerTest::rawResponses()
{
    QFETCH(int, status);
    QFETCH(bool, complete);
    QFETCH(bool, disconnect);
    QFETCH(bool, cancel);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            const bool bodyless = status == 204 || status == 304;
            const auto length = status == 204 ? "0" : status == 304 ? "20" : complete ? "7" : "20";
            socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\nContent-Length: " +
                QByteArray(length) + "\r\nX-Change: 42\r\nConnection: close\r\n\r\n" +
                (bodyless ? QByteArray() : QByteArray("partial")));
            if (disconnect) socket->disconnectFromHost();
            if (cancel) QTimer::singleShot(10, &server, [&] { token.cancel(); });
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    bool returned = false;
    try {
        const auto response = http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 150,
                                                     "X-Change", NvHTTP::NVLL_NONE, &token);
        returned = true;
        QCOMPARE(response.httpStatusCode, status);
        QCOMPARE(response.body, status == 204 || status == 304 ? QByteArray() : QByteArray("partial"));
        QCOMPARE(response.headerValue, QByteArray("42"));
    }
    catch (const QtNetworkReplyException&) { }
    QCOMPARE(returned, complete);
}

class TruncatedStatusReply : public QNetworkReply
{
public:
    explicit TruncatedStatusReply(const QNetworkRequest& request, QObject* parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
        setHeader(QNetworkRequest::ContentLengthHeader, 20);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            // The first status-derived error is all Qt exposes even if a later
            // transport failure occurs. The seven-byte body is incomplete.
            setError(ContentNotFoundError, "Not found");
            setFinished(true);
            emit readyRead();
            emit finished();
        });
    }
    void abort() override { }
    qint64 bytesAvailable() const override { return 7 - m_Offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, 7 - m_Offset);
        if (!count) return -1;
        std::memcpy(data, "partial" + m_Offset, count);
        m_Offset += count;
        return count;
    }
private:
    qint64 m_Offset = 0;
};

class TruncatedStatusManager : public QNetworkAccessManager
{
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        return new TruncatedStatusReply(request, this);
    }
};

void NvPairingManagerTest::truncatedStatusErrorCannotMasqueradeAsCompletedResponse()
{
    TruncatedStatusManager manager;
    NvHTTP http(NvAddress("127.0.0.1", 1), 1, {}, false, &manager);
    QVERIFY_EXCEPTION_THROWN(http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 150,
                                                  {}, NvHTTP::NVLL_NONE), QtNetworkReplyException);
}

void NvPairingManagerTest::chunkedResponses_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<int>("ending");
    for (int status : {200, 404, 503}) {
        for (int ending : {0, 1, 2, 3, 4}) {
            QTest::newRow(qPrintable(QString::number(status) + "/" + QString::number(ending))) << status << ending;
        }
    }
}

void NvPairingManagerTest::chunkedResponses()
{
    QFETCH(int, status);
    QFETCH(int, ending);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            QByteArray body = ending == 1 ? "14\r\npartial" : "7\r\npartial\r\n";
            if (ending == 0) body += "0\r\n\r\n";
            const QByteArray framing = ending == 3 ? QByteArray() : ending == 4 ?
                QByteArray("Content-Encoding: gzip\r\nContent-Length: 7\r\n") :
                QByteArray("Transfer-Encoding: chunked\r\n");
            if (ending >= 3) body = "partial";
            socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\n" + framing + "Connection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    bool returned = false;
    try {
        const auto response = http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 150,
                                                     {}, NvHTTP::NVLL_NONE);
        returned = true;
        QCOMPARE(response.httpStatusCode, status);
        QCOMPARE(response.body, QByteArray("partial"));
    }
    catch (const QtNetworkReplyException&) { }
    // Even a nominally complete chunked response is outside this endpoint
    // policy: Qt cannot prove the final zero-chunk arrived on all supported versions.
    QVERIFY(!returned);
}

void NvPairingManagerTest::cancellationBeforeArmIsSticky()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    NvHTTP::CancelToken token;
    token.cancel();
    QVERIFY(token.isCancelled());
    http.setCancelToken(&token);
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY_EXCEPTION_THROWN(http.openConnectionToString(http.m_BaseUrlHttp, "launch", {}, 2000,
                                                        NvHTTP::NVLL_NONE), QtNetworkReplyException);
    QVERIFY(elapsed.elapsed() < 1000);
    // Cancellation remains latched for a subsequent handshake request too.
    QVERIFY_EXCEPTION_THROWN(http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 2000,
                                                  {}, NvHTTP::NVLL_NONE, &token), QtNetworkReplyException);
    QVERIFY(elapsed.elapsed() < 1000);
}

void NvPairingManagerTest::interruptionBeforeOrDuringArmIsConsumed_data()
{
    QTest::addColumn<bool>("before");
    QTest::newRow("before-arm") << true;
    QTest::newRow("held-request") << false;
}

void NvPairingManagerTest::interruptionBeforeOrDuringArmIsConsumed()
{
    QFETCH(bool, before);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    bool hold = true;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            if (hold) token.interrupt();
            else respond(socket, "ready");
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    if (before) token.interrupt();
    QElapsedTimer elapsed; elapsed.start();
    try {
        http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 3000, {}, NvHTTP::NVLL_NONE, &token);
        QFAIL("The interrupted read must not complete normally");
    }
    catch (const QtNetworkReplyException& error) {
        QCOMPARE(error.getError(), QNetworkReply::OperationCanceledError);
    }
    QVERIFY(elapsed.elapsed() < 1000);
    QVERIFY(!token.isCancelled());
    hold = false;
    QCOMPARE(http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 1000,
                                   {}, NvHTTP::NVLL_NONE, &token).body, QByteArray("ready"));
}

void NvPairingManagerTest::rawDeadlineStillReportsTimeout()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [socket] { socket->readAll(); });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    try {
        http.openConnectionRaw(http.m_BaseUrlHttp, "clipboard", {}, 30, {}, NvHTTP::NVLL_NONE);
        QFAIL("A held request must time out");
    }
    catch (const QtNetworkReplyException& error) { QCOMPARE(error.getError(), QNetworkReply::TimeoutError); }
}

void NvPairingManagerTest::postCancellation_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("cancel-before-arm") << 0;
    QTest::newRow("cancel-active") << 1;
    QTest::newRow("interrupt-before-arm") << 2;
    QTest::newRow("deadline") << 3;
}

void NvPairingManagerTest::postCancellation()
{
    QFETCH(int, mode);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            if (mode == 1) token.cancel();
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    http.setCancelToken(&token);
    if (mode == 0) token.cancel();
    if (mode == 2) token.interrupt();
    QElapsedTimer elapsed; elapsed.start();
    try {
        http.openConnectionPost(http.m_BaseUrlHttp, "clipboard", {}, "test", "text/plain", 150, NvHTTP::NVLL_NONE);
        QFAIL("Controlled POST must abort");
    }
    catch (const QtNetworkReplyException& error) {
        QCOMPARE(error.getError(), mode == 3 ? QNetworkReply::TimeoutError : QNetworkReply::OperationCanceledError);
    }
    if (mode != 3) QVERIFY(elapsed.elapsed() < 100);
}

void NvPairingManagerTest::postHttpErrors_data()
{
    QTest::addColumn<int>("status"); QTest::addColumn<bool>("complete");
    QTest::newRow("bad-request") << 400 << true;
    QTest::newRow("disabled") << 403 << true;
    QTest::newRow("unsupported") << 404 << true;
    QTest::newRow("transient") << 503 << true;
    QTest::newRow("truncated-disabled") << 403 << false;
}

void NvPairingManagerTest::postHttpErrors()
{
    QFETCH(int, status); QFETCH(bool, complete);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\nContent-Length: " +
                          (complete ? QByteArray("0") : QByteArray("20")) + "\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    try {
        http.openConnectionPost(http.m_BaseUrlHttp, "clipboard", {}, "test", "text/plain", 200, NvHTTP::NVLL_NONE);
        QFAIL("HTTP error must not succeed");
    }
    catch (const GfeHttpResponseException& error) { QVERIFY(complete); QCOMPARE(error.getStatusCode(), status); }
    catch (const QtNetworkReplyException& error) { QVERIFY(!complete); QCOMPARE(error.getError(), QNetworkReply::RemoteHostClosedError); }
}

class CancelDuringRequestManager : public QNetworkAccessManager
{
public:
    NvHTTP::CancelToken* token = nullptr;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* data) override
    {
        auto* reply = QNetworkAccessManager::createRequest(operation, request, data);
        // The request exists, but NvHTTP has not yet armed its token.
        token->cancel();
        return reply;
    }
};

void NvPairingManagerTest::cancellationDuringArmIsSticky()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    CancelDuringRequestManager manager;
    manager.token = &token;
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false, &manager);
    http.setCancelToken(&token);
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY_EXCEPTION_THROWN(http.openConnectionToString(http.m_BaseUrlHttp, "launch", {}, 2000,
                                                        NvHTTP::NVLL_NONE), QtNetworkReplyException);
    QVERIFY(elapsed.elapsed() < 1000);
}

void NvPairingManagerTest::cancellationAbortsStartedLaunch()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    bool launchReceived = false;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            launchReceived = socket->readAll().contains("/launch?");
            token.cancel();
        });
    });
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(), {}, false);
    http.setCancelToken(&token);
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY_EXCEPTION_THROWN(http.openConnectionToString(http.m_BaseUrlHttp, "launch", {}, 120000,
                                                        NvHTTP::NVLL_NONE), QtNetworkReplyException);
    QVERIFY(launchReceived);
    QVERIFY(elapsed.elapsed() < 1000);
    // Another launch uses a fresh token and remains possible after cancellation.
    NvHTTP::CancelToken retry;
    http.setCancelToken(&retry);
    server.disconnect(&server);
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [socket] {
            socket->readAll();
            respond(socket, "retry");
        });
    });
    QCOMPARE(http.openConnectionToString(http.m_BaseUrlHttp, "launch", {}, 1000,
                                         NvHTTP::NVLL_NONE), QString("retry"));
}

void NvPairingManagerTest::cancellationAbortsStartedPairing()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    NvHTTP::CancelToken token;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            token.cancel();
        });
    });
    NvComputer computer;
    computer.activeAddress = NvAddress("127.0.0.1", server.serverPort());
    computer.activeHttpsPort = server.serverPort();
    NvPairingManager manager(&computer, &token);
    QSslCertificate certificate;
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY_EXCEPTION_THROWN(manager.pair("7.0.0.0", "1234", certificate), QtNetworkReplyException);
    QVERIFY(elapsed.elapsed() < 1000);
    QVERIFY(certificate.isNull());
}

class CancelAfterServerInfoManager : public QNetworkAccessManager
{
public:
    NvHTTP::CancelToken* cancellation = nullptr;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice* data) override
    {
        auto* reply = QNetworkAccessManager::createRequest(operation, request, data);
        if (cancellation && request.url().path() == "/serverinfo") {
            connect(reply, &QNetworkReply::finished, reply, [this] {
                // A real serverinfo response is finished; cancel precisely before
                // startApp can send the fallback launch, without shortening its body.
                cancellation->cancel();
            });
        }
        return reply;
    }
};

void NvPairingManagerTest::staleResumeRecovery_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<bool>("cancelAfterRefresh");
    QTest::newRow("fresh-free-launches-once") << QString("SUNSHINE_SERVER_FREE") << false;
    QTest::newRow("fresh-occupied-never-launches") << QString("SUNSHINE_SERVER_BUSY") << false;
    QTest::newRow("missing-state-never-launches") << QString() << false;
    QTest::newRow("cancel-after-complete-refresh-never-launches") << QString("SUNSHINE_SERVER_FREE") << true;
}

void NvPairingManagerTest::staleResumeRecovery()
{
    QFETCH(QString, state);
    QFETCH(bool, cancelAfterRefresh);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QStringList paths;
    QList<int> requestedApps;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            auto request = socket->property("request").toByteArray() + socket->readAll();
            socket->setProperty("request", request);
            if (socket->property("handled").toBool() || !request.contains("\r\n\r\n")) return;
            socket->setProperty("handled", true);
            QUrl url("http://localhost" + QString::fromLatin1(request.split(' ').value(1)));
            paths.append(url.path());
            QByteArray xml;
            if (url.path() == "/resume") {
                requestedApps.append(QUrlQuery(url).queryItemValue("appid").toInt());
                xml = "<root status_code=\"503\" status_message=\"No running app to resume\"><resume>0</resume></root>";
            }
            else if (url.path() == "/serverinfo") {
                xml = "<root status_code=\"200\"><PairStatus>1</PairStatus><state>" + state.toUtf8() + "</state><currentgame>" +
                    (state.endsWith("_SERVER_BUSY") ? "99" : "0") + "</currentgame></root>";
            }
            else if (url.path() == "/launch") {
                requestedApps.append(QUrlQuery(url).queryItemValue("appid").toInt());
                xml = "<root status_code=\"200\"><sessionUrl0>rtsp://127.0.0.1/test</sessionUrl0></root>";
            }
            respond(socket, xml);
        });
    });
    NvHTTP::CancelToken cancellation;
    CancelAfterServerInfoManager manager;
    if (cancelAfterRefresh) manager.cancellation = &cancellation;
    NvHTTP http(NvAddress("127.0.0.1", server.serverPort()), server.serverPort(),
                IdentityManager::get()->getSslConfig().localCertificate(), false, &manager);
    // Keep the protocol seam real while avoiding TLS/certificate fixture setup.
    http.m_BaseUrlHttps.setScheme("http");
    http.setCancelToken(&cancellation);
    STREAM_CONFIGURATION stream = {};
    stream.width = 1920;
    stream.height = 1080;
    stream.fps = 60;
    stream.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    QString sessionUrl;
    bool succeeded = false;
    try {
        http.startApp("resume", false, 42, &stream, false, false, 0, false, false, sessionUrl);
        succeeded = true;
    }
    catch (const GfeHttpResponseException&) { }
    catch (const QtNetworkReplyException&) { }
    const bool expectedSuccess = state.endsWith("_SERVER_FREE") && !cancelAfterRefresh;
    QCOMPARE(succeeded, expectedSuccess);
    QCOMPARE(paths, expectedSuccess ? QStringList({"/resume", "/serverinfo", "/launch"}) :
                                      QStringList({"/resume", "/serverinfo"}));
    QCOMPARE(requestedApps, expectedSuccess ? QList<int>({42, 42}) : QList<int>({42}));
    if (expectedSuccess) QCOMPARE(sessionUrl, QString("rtsp://127.0.0.1/test"));
}

QTEST_GUILESS_MAIN(NvPairingManagerTest)
#include "tst_nvpairing.moc"
