#include "backend/nvcomputer.h"
#include "streaming/filetransfer.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

namespace FileTransferTestHooks {
extern std::function<void()> beforeCommit;
}

static QByteArray json(QJsonObject o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
static QByteArray sha(QByteArray b) { return QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex(); }
class FileTransferTest : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        FileTransport::reset();
        FileTransferTestHooks::beforeCommit = { };
    }
    void queuedFilesContinueAfterFailureAndNeverReplaceCollisions()
    {
        QTemporaryDir dir;
        QVariantList urls;
        for (const auto& name : {"one.txt", "two.txt", "three.txt"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(name);
            urls.append(QUrl::fromLocalFile(f.fileName()));
        }
        QStringList uploads;
        QByteArray current;
        FileTransport::respond = [&](QString args, QByteArray bytes) {
            QUrlQuery q(args); const auto op = q.queryItemValue("op");
            if (op == "list") return json({{"entries", QJsonArray{QJsonObject{{"name","two.txt"}}}}});
            if (op == "beginUpload") {
                const auto name = q.queryItemValue("name"); uploads.append(name);
                if (name == "one.txt") throw std::runtime_error("Destination unavailable");
                if (q.queryItemValue("overwrite") != "0") throw std::runtime_error("Unsafe overwrite");
                current.clear(); return json({{"id","T"},{"chunkBytes",1048576}});
            }
            if (op == "chunk") { current += bytes; return QByteArray("{}"); }
            return json({{"sha256",QString::fromLatin1(sha(current))}});
        };
        NvComputer host; FileTransfer transfer(&host, true);
        QVERIFY(QMetaObject::invokeMethod(&transfer, "sendFiles", Q_ARG(QVariantList, urls), Q_ARG(QString, "Downloads"), Q_ARG(QString, "")));
        QTRY_COMPARE_WITH_TIMEOUT(transfer.property("collisionName").toString(), QString("two.txt"), 2000);
        QCOMPARE(uploads, QStringList{"one.txt"});
        QVERIFY(transfer.property("queueActive").toBool());
        QVERIFY(QMetaObject::invokeMethod(&transfer, "resolveCollision", Q_ARG(QString, "keepBoth")));
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.property("queueActive").toBool(), 2000);
        QCOMPARE(uploads, QStringList({"one.txt", "two (2).txt", "three.txt"}));
        const auto entries = transfer.property("queueEntries").toList();
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].toMap().value("status").toString(), QString("Failed"));
        QCOMPARE(entries[1].toMap().value("status").toString(), QString("Copied"));
        QCOMPARE(entries[2].toMap().value("status").toString(), QString("Copied"));
    }
    void cancelQueueStopsActiveAndPendingFiles()
    {
        QTemporaryDir dir; QVariantList urls;
        for (const auto& name : {"one", "two", "three"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("contents"); urls.append(QUrl::fromLocalFile(f.fileName()));
        }
        FileTransport::holdChunk = true;
        std::atomic<int> uploads{0};
        FileTransport::respond = [&](QString args, QByteArray) {
            const auto op = QUrlQuery(args).queryItemValue("op");
            if (op == "list") return json({{"entries",QJsonArray{}}});
            if (op == "beginUpload") { ++uploads; return json({{"id","T"},{"chunkBytes",1048576}}); }
            return QByteArray("{}");
        };
        NvComputer host; FileTransfer transfer(&host, true);
        QVERIFY(QMetaObject::invokeMethod(&transfer,"sendFiles",Q_ARG(QVariantList,urls),Q_ARG(QString,"Downloads"),Q_ARG(QString,"")));
        QTRY_VERIFY_WITH_TIMEOUT(FileTransport::chunkStarted.load(),2000);
        transfer.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(),2000);
        QVERIFY(!transfer.property("queueActive").toBool());
        QCOMPARE(uploads.load(),1); QCOMPARE(FileTransport::cancels.load(),1);
        for (const auto& entry : transfer.property("queueEntries").toList()) QCOMPARE(entry.toMap().value("status").toString(),QString("Canceled"));
    }
    void canceledCollisionCannotResumePendingCopy()
    {
        QTemporaryDir dir; QFile file(dir.filePath("existing")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("original"); file.close();
        FileTransport::respond = [](QString args, QByteArray) {
            if (QUrlQuery(args).queryItemValue("op") == "list") return json({{"entries",QJsonArray{QJsonObject{{"name","existing"}}}}});
            throw std::runtime_error("Unexpected upload");
        };
        NvComputer host; FileTransfer transfer(&host,true);
        QVariantList urls{QUrl::fromLocalFile(file.fileName())};
        QVERIFY(QMetaObject::invokeMethod(&transfer,"sendFiles",Q_ARG(QVariantList,urls),Q_ARG(QString,"Downloads"),Q_ARG(QString,"")));
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.property("collisionName").toString().isEmpty() && !transfer.busy(),2000);
        transfer.cancel();
        QVERIFY(QMetaObject::invokeMethod(&transfer,"resolveCollision",Q_ARG(QString,"keepBoth")));
        QVERIFY(!transfer.property("queueActive").toBool());
        QCOMPARE(FileTransport::requests.load(),1);
        QCOMPARE(transfer.property("queueEntries").toList().first().toMap().value("status").toString(),QString("Canceled"));
    }
    void unsupportedPeerEndsQueueWithoutCopying()
    {
        QTemporaryDir dir; QVariantList urls;
        for (const auto& name : {"one","two"}) { QFile file(dir.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("source"); urls.append(QUrl::fromLocalFile(file.fileName())); }
        FileTransport::supported = false;
        NvComputer host; FileTransfer transfer(&host,true);
        QVERIFY(QMetaObject::invokeMethod(&transfer,"sendFiles",Q_ARG(QVariantList,urls),Q_ARG(QString,"Downloads"),Q_ARG(QString,"")));
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.property("queueActive").toBool(),2000);
        QVERIFY(!transfer.supported()); QCOMPARE(FileTransport::requests.load(),0);
        for (const auto& entry : transfer.property("queueEntries").toList()) QCOMPARE(entry.toMap().value("status").toString(),QString("Failed"));
    }
    void oversizedOrFolderBatchDoesNotStartPartialCopy()
    {
        QTemporaryDir dir; NvComputer host; FileTransfer transfer(&host,true);
        QVariantList urls;
        for (int i=0;i<33;++i) urls.append(QUrl::fromLocalFile(dir.path()));
        QVERIFY(QMetaObject::invokeMethod(&transfer,"sendFiles",Q_ARG(QVariantList,urls),Q_ARG(QString,"Downloads"),Q_ARG(QString,"")));
        QVERIFY(!transfer.busy()); QVERIFY(!transfer.error().isEmpty()); QCOMPARE(FileTransport::requests.load(),0);
        urls = {QUrl::fromLocalFile(dir.path())};
        QVERIFY(QMetaObject::invokeMethod(&transfer,"sendFiles",Q_ARG(QVariantList,urls),Q_ARG(QString,"Downloads"),Q_ARG(QString,"")));
        QVERIFY(!transfer.busy()); QVERIFY(!transfer.error().isEmpty()); QCOMPARE(FileTransport::requests.load(),0);
    }
    void downloadPublishesOnlyCompleteChecksumVerifiedFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray data = "complete unicode data \xe2\x98\x83";
        FileTransport::respond = [&](QString args, QByteArray) {QUrlQuery q(args);auto op=q.queryItemValue("op");if(op=="beginDownload")return json({{"id","T"},{"size",QString::number(data.size())},{"chunkBytes",1048576}});if(op=="read")return data;if(op=="finish")return json({{"sha256",QString::fromLatin1(sha(data))}});return QByteArray("{}"); };
        NvComputer host;
        FileTransfer transfer(&host, true);
        const auto path = dir.filePath("received.txt");
        transfer.receiveFile("Downloads", "source", QUrl::fromLocalFile(path));
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY2(transfer.error().isEmpty(), qPrintable(transfer.error()));
        QFile output(path);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), data);
        QCOMPARE(transfer.progress(), qint64(data.size()));
        QCOMPARE(transfer.property("completedLocalFile").toUrl(), QUrl::fromLocalFile(path));
    }
    void checksumFailurePreservesOriginalAndRemovesOwnedTemp()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("keep.txt");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("original");
        }
        FileTransport::respond = [](QString args, QByteArray) {auto op=QUrlQuery(args).queryItemValue("op");if(op=="beginDownload")return json({{"id","T"},{"size","3"},{"chunkBytes",1048576}});if(op=="read")return QByteArray("new");return json({{"sha256","incorrect"}}); };
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.receiveFile("Downloads", "source", QUrl::fromLocalFile(path), true);
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(!transfer.error().isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("original"));
        QVERIFY(transfer.property("completedLocalFile").toUrl().isEmpty());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList({ "keep.txt" }));
    }
    void canceledUploadStopsWorkerAndCancelsHostTransaction()
    {
        QTemporaryDir dir;
        auto path = dir.filePath("source");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray(1024 * 1024 + 7, 'x'));
        }
        FileTransport::holdChunk = true;
        FileTransport::respond = [](QString args, QByteArray) {if(QUrlQuery(args).queryItemValue("op")=="beginUpload")return json({{"id","T"},{"chunkBytes",1048576}});return QByteArray("{}"); };
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.sendFile(QUrl::fromLocalFile(path), "Downloads", "", "copy");
        QTRY_VERIFY_WITH_TIMEOUT(FileTransport::chunkStarted.load(), 2000);
        transfer.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(!transfer.error().isEmpty());
        QCOMPARE(FileTransport::cancels.load(), 1);
        QCOMPARE(QFileInfo(path).size(), qint64(1024 * 1024 + 7));
    }
    void uploadUsesNegotiatedChunksAndVerifiesFinalChecksum()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("source");
        const QByteArray bytes(1024 * 1024 + 7, 'x');
        {
            QFile source(path);
            QVERIFY(source.open(QIODevice::WriteOnly));
            QCOMPARE(source.write(bytes), qint64(bytes.size()));
        }
        QByteArray received;
        bool valid = true;
        int chunks = 0;
        FileTransport::respond = [&](QString args, QByteArray data) {
            QUrlQuery q(args);
            auto op = q.queryItemValue("op");
            if (op == "beginUpload")
                return json({ { "id", "T" }, { "chunkBytes", 524288 } });
            if (op == "chunk") {
                valid = valid && data.size() <= 524288 && q.queryItemValue("offset").toLongLong() == received.size();
                received += data;
                ++chunks;
                return QByteArray("{}");
            }
            return json({ { "sha256", QString::fromLatin1(sha(received)) } });
        };
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.sendFile(QUrl::fromLocalFile(path), "Downloads", "", "copy");
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY2(transfer.error().isEmpty(), qPrintable(transfer.error()));
        QVERIFY(valid);
        QCOMPARE(chunks, 3);
        QCOMPARE(received, bytes);
        QCOMPARE(transfer.progress(), qint64(bytes.size()));
    }
    void qmlLocalPathsPreserveSpecialNamesAndFileContents_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("encodedUrl");
        for (const auto& name : { QStringLiteral("notes#draft.txt"), QStringLiteral("notes?draft.txt"), QStringLiteral("notes%23draft.txt"), QString::fromUtf8("snow \xe2\x98\x83.txt") }) {
            QTest::newRow(qPrintable(name + "-typed")) << name << false;
            QTest::newRow(qPrintable(name + "-picker")) << name << true;
        }
    }
    void qmlLocalPathsPreserveSpecialNamesAndFileContents()
    {
        QFETCH(QString, name);
        QFETCH(bool, encodedUrl);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(name);
        const QByteArray bytes = "the selected file, not its prefix";
        for (const auto& entry : { qMakePair(dir.filePath("notes"), QByteArray("WRONG prefix file")), qMakePair(path, bytes) }) {
            QFile file(entry.first);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(entry.second), qint64(entry.second.size()));
        }
        QByteArray received;
        FileTransport::respond = [&](QString args, QByteArray data) {
            const auto op = QUrlQuery(args).queryItemValue("op");
            if (op == "beginUpload") return json({ { "id", "T" }, { "chunkBytes", 1048576 } });
            if (op == "beginDownload") return json({ { "id", "T" }, { "size", QString::number(bytes.size()) }, { "chunkBytes", 1048576 } });
            if (op == "read") return bytes;
            if (op == "chunk") { received += data; return QByteArray("{}"); }
            return json({ { "sha256", QString::fromLatin1(sha(received)) } });
        };
        NvComputer host;
        FileTransfer transfer(&host, true);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml 2.12
            QtObject {
                property var transfer
                property url selectedUrl
                function send(path) {
                    selectedUrl = transfer.localFileUrl(path)
                    transfer.sendFile(selectedUrl, "Downloads", "", "copy", false)
                }
                function receive(path) {
                    selectedUrl = transfer.localFileUrl(path)
                    transfer.receiveFile("Downloads", "copy", selectedUrl, true)
                }
            }
        )", QUrl());
        QScopedPointer<QObject> bridge(component.create());
        QVERIFY2(bridge, qPrintable(component.errorString()));
        QVERIFY(bridge->setProperty("transfer", QVariant::fromValue(&transfer)));
        const auto input = encodedUrl ? QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded) : path;
        QVERIFY(QMetaObject::invokeMethod(bridge.data(), "send", Q_ARG(QVariant, input)));
        QCOMPARE(bridge->property("selectedUrl").toUrl().toLocalFile(), path);
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY2(transfer.error().isEmpty(), qPrintable(transfer.error()));
        QCOMPARE(received, bytes);
        const auto target = dir.filePath("received-" + name);
        const auto prefix = dir.filePath("received-notes");
        {
            QFile file(prefix);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("keep prefix destination");
        }
        const auto destination = encodedUrl ? QUrl::fromLocalFile(target).toString(QUrl::FullyEncoded) : target;
        QVERIFY(QMetaObject::invokeMethod(bridge.data(), "receive", Q_ARG(QVariant, destination)));
        QCOMPARE(bridge->property("selectedUrl").toUrl().toLocalFile(), target);
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY2(transfer.error().isEmpty(), qPrintable(transfer.error()));
        QFile output(target);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), bytes);
        QFile preserved(prefix);
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), QByteArray("keep prefix destination"));
    }
    void overwritePreservesDestinationChangedDuringDownload()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("keep");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("original");
        }
        FileTransport::respond = [path](QString args, QByteArray) {
            auto op = QUrlQuery(args).queryItemValue("op");
            if (op == "beginDownload")
                return json({ { "id", "T" }, { "size", "3" }, { "chunkBytes", 1048576 } });
            if (op == "read") {
                QFile edited(path);
                if (edited.open(QIODevice::WriteOnly))
                    edited.write("newer edits");
                return QByteArray("new");
            }
            return json({ { "sha256", QString::fromLatin1(sha("new")) } });
        };
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.receiveFile("Downloads", "source", QUrl::fromLocalFile(path), true);
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(!transfer.error().isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("newer edits"));
    }
    void overwriteRechecksDestinationAfterSaveFileCopyBeforeCommit()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("keep");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("original");
        }
        FileTransport::respond = [](QString args, QByteArray) {auto op=QUrlQuery(args).queryItemValue("op");if(op=="beginDownload")return json({{"id","T"},{"size","3"},{"chunkBytes",1048576}});if(op=="read")return QByteArray("new");return json({{"sha256",QString::fromLatin1(sha("new"))}}); };
        bool injected = false;
        FileTransferTestHooks::beforeCommit = [&] {injected=true;QFile file(path);if(file.open(QIODevice::WriteOnly))file.write("late edits"); };
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.receiveFile("Downloads", "source", QUrl::fromLocalFile(path), true);
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(injected);
        QVERIFY(!transfer.error().isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("late edits"));
        FileTransferTestHooks::beforeCommit = { };
    }
    void noOverwriteAndUnsupportedPeerMakeNoTransferRequest()
    {
        QTemporaryDir dir;
        auto path = dir.filePath("keep");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("original");
        }
        NvComputer host;
        FileTransfer transfer(&host, true);
        transfer.receiveFile("Downloads", "source", QUrl::fromLocalFile(path));
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(!transfer.error().isEmpty());
        QCOMPARE(FileTransport::requests.load(), 0);
        FileTransport::supported = false;
        transfer.listRemote();
        QTRY_VERIFY_WITH_TIMEOUT(!transfer.busy(), 2000);
        QVERIFY(!transfer.error().isEmpty());
        QCOMPARE(FileTransport::requests.load(), 0);
        QVERIFY(!transfer.supported());
    }
};
QTEST_GUILESS_MAIN(FileTransferTest)
#include "tst_filetransfer.moc"
