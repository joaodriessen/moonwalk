#include <QtTest>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>

#include "../../../app/backend/boundedreply.h"

class BoundedReplyTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptsSmallResponse();
    void rejectsDeclaredOversize();
    void rejectsChunkedOversize();
};

static QUrl serve(QTcpServer &server, const QByteArray &response)
{
    if (!server.listen(QHostAddress::LocalHost)) {
        return {};
    }
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, response]() {
        QTcpSocket *client = server.nextPendingConnection();
        client->write(response);
        client->disconnectFromHost();
        QObject::connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
    });
    return QUrl(QStringLiteral("http://127.0.0.1:%1/test").arg(server.serverPort()));
}

void BoundedReplyTest::acceptsSmallResponse()
{
    QTcpServer server;
    const QUrl url = serve(server, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    QVERIFY(url.isValid());
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    BoundedReply limit(reply, 16);
    QSignalSpy finished(reply, &QNetworkReply::finished);
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!limit.exceeded());
    QCOMPARE(reply->readAll(), QByteArray("hello"));
    delete reply;
}

void BoundedReplyTest::rejectsDeclaredOversize()
{
    QTcpServer server;
    const QUrl url = serve(server, "HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n");
    QVERIFY(url.isValid());
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    BoundedReply limit(reply, 16);
    QTRY_VERIFY(limit.exceeded());
    delete reply;
}

void BoundedReplyTest::rejectsChunkedOversize()
{
    QTcpServer server;
    const QByteArray body(2048, 'x');
    const QByteArray response = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n800\r\n" +
                                body + "\r\n0\r\n\r\n";
    const QUrl url = serve(server, response);
    QVERIFY(url.isValid());
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    BoundedReply limit(reply, 128);
    QTRY_VERIFY(limit.exceeded());
    QVERIFY(reply->bytesAvailable() <= 129);
    delete reply;
}

QTEST_GUILESS_MAIN(BoundedReplyTest)

#include "tst_boundedreply.moc"
