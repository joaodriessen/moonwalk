#pragma once
#include "backend/nvhttp.h"
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <functional>
#include <memory>
class NvComputer;

class FileTransfer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(qint64 progress READ progress NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList remoteEntries READ remoteEntries NOTIFY changed)
    Q_PROPERTY(QString remoteFolder READ remoteFolder NOTIFY changed)
    Q_PROPERTY(QString remoteDirectory READ remoteDirectory NOTIFY changed)
    Q_PROPERTY(QVariantList queueEntries READ queueEntries NOTIFY changed)
    Q_PROPERTY(bool queueActive READ queueActive NOTIFY changed)
    Q_PROPERTY(QString collisionName READ collisionName NOTIFY changed)
    Q_PROPERTY(QUrl completedLocalFile READ completedLocalFile NOTIFY changed)
public:
    explicit FileTransfer(NvComputer* computer, bool supported = false, QObject* parent = nullptr);
    ~FileTransfer() override;
    bool supported() const { return m_Supported; }
    bool busy() const { return m_Busy; }
    qint64 progress() const { return m_Progress; }
    qint64 total() const { return m_Total; }
    QString error() const { return m_Error; }
    QVariantList remoteEntries() const { return m_Entries; }
    QString remoteFolder() const { return m_Folder; }
    QString remoteDirectory() const { return m_Directory; }
    QVariantList queueEntries() const { return m_Queue; }
    bool queueActive() const { return m_QueueActive; }
    QString collisionName() const { return m_CollisionName; }
    QUrl completedLocalFile() const { return m_CompletedLocalFile; }
    Q_INVOKABLE void sendFiles(QVariantList localFiles, QString folder = QStringLiteral("Downloads"), QString relativeDirectory = QString());
    Q_INVOKABLE void resolveCollision(QString action);
    Q_INVOKABLE void revealReceivedFile();
    Q_INVOKABLE QUrl localFileUrl(QString pathOrUrl) const;
    Q_INVOKABLE void listRemote(QString folder = QStringLiteral("Downloads"), QString relativeDirectory = QString());
    Q_INVOKABLE void sendFile(QUrl localFile, QString folder, QString relativeDirectory, QString name = QString(), bool overwrite = false);
    Q_INVOKABLE void receiveFile(QString folder, QString relativeFile, QUrl localFile, bool overwrite = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void chooseSendFile();
    Q_INVOKABLE void chooseReceiveDestination(QString folder, QString relativeFile);
signals:
    void changed();
    void pickerRequested(QString action, QString folder, QString relativeFile);

private:
    using Token = std::shared_ptr<NvHTTP::CancelToken>;
    void start(std::function<void(NvHTTP&, const Token&)> job);
    void sendNext();
    void upload(NvHTTP& http, const Token& token, QUrl localFile, QString folder, QString relativeDirectory, QString name, bool overwrite);
    void setQueueStatus(QString status, QString message = QString());
    void updateProgress(qint64 progress, qint64 total);
    void fail(QString message);
    NvComputer* m_Computer;
    QSharedPointer<NvComputer> m_HostOwner;
    QThread* m_Thread = nullptr;
    Token m_Token;
    bool m_Supported = false, m_Busy = false;
    qint64 m_Progress = 0, m_Total = 0;
    QString m_Error, m_Folder = QStringLiteral("Downloads"), m_Directory;
    QVariantList m_Entries, m_Queue;
    int m_QueueIndex = 0;
    bool m_QueueActive = false;
    QString m_CollisionName;
    QUrl m_CompletedLocalFile;
};
