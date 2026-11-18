#include "filetransfer.h"
#include "backend/nvcomputer.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>
#include <QUrlQuery>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef FILE_TRANSFER_TESTING
namespace FileTransferTestHooks {
std::function<void()> beforeCommit;
}
#endif

namespace {
constexpr qint64 chunkBytes = 1024 * 1024, maxFileBytes = 8ll * 1024 * 1024 * 1024;
QString query(std::initializer_list<QPair<QString, QString>> values)
{
    QUrlQuery q;
    for (const auto& v : values)
        q.addQueryItem(v.first, v.second);
    return q.query(QUrl::FullyEncoded);
}
void require(bool condition, const QString& message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}
void canceled(const std::shared_ptr<NvHTTP::CancelToken>& token) { require(!token->isCancelled(), QObject::tr("Transfer canceled; original files were preserved.")); }
QJsonObject object(const QByteArray& bytes)
{
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(bytes, &e);
    require(e.error == QJsonParseError::NoError && doc.isObject(), QObject::tr("Host returned an incomplete transfer response."));
    return doc.object();
}
QByteArray get(NvHTTP& http, const QString& arguments, const std::shared_ptr<NvHTTP::CancelToken>& token)
{
    auto raw = http.openConnectionRaw(http.m_BaseUrlHttps, "actions/files", arguments, 10000, { }, NvHTTP::NVLL_ERROR, token.get(), chunkBytes);
    require(raw.httpStatusCode == 200, QString::fromUtf8(raw.body.left(512)));
    return raw.body;
}
struct DestinationState {
    bool exists = false;
    qint64 size = 0;
    QDateTime modified;
    quint64 device = 0, inode = 0;
    bool operator==(const DestinationState& other) const { return exists == other.exists && size == other.size && modified == other.modified && device == other.device && inode == other.inode; }
};
DestinationState inspectDestination(const QString& path)
{
    QFileInfo info(path);
    DestinationState result;
    result.exists = info.exists();
    require(!info.isSymLink() && (!result.exists || info.isFile()), QObject::tr("Destination is no longer a regular file; original preserved."));
    if (!result.exists)
        return result;
    result.size = info.size();
    result.modified = info.lastModified();
#ifdef Q_OS_UNIX
    struct stat status { };
    require(::stat(QFile::encodeName(path).constData(), &status) == 0, QObject::tr("Destination could not be checked; original preserved."));
    result.device = status.st_dev;
    result.inode = status.st_ino;
#endif
    return result;
}
qint64 negotiatedChunkSize(const QJsonObject& begin)
{
    bool ok = false;
    auto size = begin.value("chunkBytes").toVariant().toLongLong(&ok);
    require(ok && size > 0 && size <= chunkBytes, QObject::tr("Host returned an unsupported transfer chunk size."));
    return size;
}
QJsonObject post(NvHTTP& http, const QString& arguments, const QByteArray& bytes = { }) { return object(http.openConnectionPost(http.m_BaseUrlHttps, "actions/files", arguments, bytes, "application/octet-stream", 10000, NvHTTP::NVLL_ERROR).toUtf8()); }
void cleanup(NvHTTP& http, const QString& id)
{
    if (id.isEmpty())
        return;
    http.setCancelToken(nullptr);
    try {
        http.openConnectionPost(http.m_BaseUrlHttps, "actions/files", query({ { "op", "cancel" }, { "id", id } }), { }, "application/octet-stream", 1000, NvHTTP::NVLL_ERROR);
    } catch (...) {
    }
}
}
FileTransfer::FileTransfer(NvComputer* computer, bool supported, QObject* parent)
    : QObject(parent)
    , m_Computer(computer)
    , m_HostOwner(computer ? computer->retain() : QSharedPointer<NvComputer> { })
    , m_Supported(supported)
{
}
FileTransfer::~FileTransfer()
{
    cancel();
    if (m_Thread) {
        m_Thread->wait();
        delete m_Thread;
    }
}
void FileTransfer::cancel()
{
    if (m_QueueActive) {
        m_QueueActive = false;
        m_CollisionName.clear();
        for (int i = m_QueueIndex; i < m_Queue.size(); ++i) {
            auto entry = m_Queue[i].toMap();
            entry["status"] = tr("Canceled");
            m_Queue[i] = entry;
        }
        emit changed();
    }
    if (m_Token)
        m_Token->cancel();
}
QUrl FileTransfer::localFileUrl(QString pathOrUrl) const
{
    // Picker/drop URLs are already escaped; typed paths are filesystem names.
    return pathOrUrl.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        ? QUrl(pathOrUrl) : QUrl::fromLocalFile(pathOrUrl);
}
void FileTransfer::chooseSendFile()
{
    if (m_Supported && !m_Busy)
        emit pickerRequested("send", m_Folder, m_Directory);
}
void FileTransfer::chooseReceiveDestination(QString folder, QString relativeFile)
{
    if (m_Supported && !m_Busy)
        emit pickerRequested("receive", folder, relativeFile);
}
void FileTransfer::fail(QString message)
{
    QPointer<FileTransfer> self(this);
    QMetaObject::invokeMethod(this, [self, message] {if(self){self->m_Error=message;emit self->changed();} }, Qt::QueuedConnection);
}
void FileTransfer::updateProgress(qint64 progress, qint64 total)
{
    QPointer<FileTransfer> self(this);
    QMetaObject::invokeMethod(this, [self, progress, total] {if(self){self->m_Progress=progress;self->m_Total=total;emit self->changed();} }, Qt::QueuedConnection);
}
void FileTransfer::start(std::function<void(NvHTTP&, const Token&)> job)
{
    if (m_Busy)
        return;
    if (!m_Supported || !m_Computer) {
        m_Error = tr("This host does not support file transfer.");
        emit changed();
        return;
    }
    if (m_Thread) {
        m_Thread->wait();
        delete m_Thread;
        m_Thread = nullptr;
    }
    m_Busy = true;
    m_Error.clear();
    m_Progress = 0;
    m_Total = 0;
    m_Token = std::make_shared<NvHTTP::CancelToken>();
    emit changed();
    auto token = m_Token;
    m_Thread = QThread::create([this, token, job] {
        try {
            NvHTTP http(m_Computer);
            http.setCancelToken(token.get());
            const auto info = http.getServerInfo(NvHTTP::NVLL_ERROR, true);
            if (NvHTTP::getXmlString(info, "FileTransferSupported") != "1") {
                QMetaObject::invokeMethod(this, [this] {m_Supported=false;emit changed(); }, Qt::QueuedConnection);
                throw std::runtime_error(tr("This host does not support file transfer.").toStdString());
            }
            canceled(token);
            job(http, token);
        } catch (const std::exception& e) {
            fail(QString::fromUtf8(e.what()));
        } catch (...) {
            fail(tr("File transfer failed; check the destination before retrying."));
        }
    });
    connect(m_Thread, &QThread::finished, this, [this] {
        m_Busy = false;
        m_Token.reset();
        if (m_QueueActive && m_CollisionName.isEmpty()) {
            setQueueStatus(m_Error.isEmpty() ? tr("Copied") : tr("Failed"), m_Error);
            ++m_QueueIndex;
            if (m_QueueIndex >= m_Queue.size()) m_QueueActive = false;
            else QMetaObject::invokeMethod(this, &FileTransfer::sendNext, Qt::QueuedConnection);
        }
        emit changed();
    });
    m_Thread->start();
}
void FileTransfer::listRemote(QString folder, QString relativeDirectory)
{
    if (m_Busy || m_QueueActive)
        return;
    m_Folder = folder;
    m_Directory = relativeDirectory;
    m_Entries.clear();
    emit changed();
    start([this, folder, relativeDirectory](NvHTTP& http, const Token& token) {
        auto result = object(get(http, query({ { "op", "list" }, { "folder", folder }, { "path", relativeDirectory } }), token));
        canceled(token);
        QVariantList entries;
        for (const auto& value : result.value("entries").toArray()) {
            auto e = value.toObject();
            entries.append(QVariantMap { { "name", e.value("name").toString() }, { "isDirectory", e.value("isDirectory").toVariant().toBool() }, { "size", e.value("size").toVariant().toLongLong() }, { "relativePath", e.value("relativePath").toString() } });
        }
        std::sort(entries.begin(), entries.end(), [](const QVariant& a, const QVariant& b) {
            const auto left = a.toMap(), right = b.toMap();
            if (left.value("isDirectory").toBool() != right.value("isDirectory").toBool())
                return left.value("isDirectory").toBool();
            return QString::localeAwareCompare(left.value("name").toString(), right.value("name").toString()) < 0;
        });
        QPointer<FileTransfer> self(this);
        QMetaObject::invokeMethod(this, [self, entries] {if(self){self->m_Entries=entries;emit self->changed();} }, Qt::QueuedConnection);
    });
}
void FileTransfer::sendFile(QUrl localFile, QString folder, QString relativeDirectory, QString name, bool overwrite)
{
    if (m_QueueActive) return;
    start([this, localFile, folder, relativeDirectory, name, overwrite](NvHTTP& http, const Token& token) {
        upload(http, token, localFile, folder, relativeDirectory, name, overwrite);
    });
}
void FileTransfer::upload(NvHTTP& http, const Token& token, QUrl localFile, QString folder, QString relativeDirectory, QString name, bool overwrite)
{
    QString id;
    try {
        require(localFile.isLocalFile(), tr("Choose a local regular file."));
        QFileInfo source(localFile.toLocalFile());
        require(source.isFile() && !source.isSymLink(), tr("Only regular files can be transferred."));
        QFile file(source.absoluteFilePath());
        require(file.open(QIODevice::ReadOnly), file.errorString());
        const auto size = file.size();
        require(size >= 0 && size <= maxFileBytes, tr("File exceeds the 8 GiB transfer limit."));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd()) {
            canceled(token);
            auto bytes = file.read(chunkBytes);
            require(!bytes.isEmpty() || file.error() == QFile::NoError, file.errorString());
            hash.addData(bytes);
        }
        const auto checksum = hash.result().toHex();
        require(file.seek(0), file.errorString());
        canceled(token);
        auto begin = post(http, query({ { "op", "beginUpload" }, { "folder", folder }, { "path", relativeDirectory }, { "name", name.isEmpty() ? source.fileName() : name }, { "size", QString::number(size) }, { "sha256", QString::fromLatin1(checksum) }, { "overwrite", overwrite ? "1" : "0" } }));
        id = begin.value("id").toString();
        require(!id.isEmpty(), tr("Host did not create a transfer."));
        const auto transferChunk = negotiatedChunkSize(begin);
        qint64 offset = 0;
        updateProgress(0, size);
        while (offset < size) {
            canceled(token);
            auto bytes = file.read(qMin(transferChunk, size - offset));
            require(!bytes.isEmpty(), tr("Source changed or could not be read."));
            post(http, query({ { "op", "chunk" }, { "id", id }, { "offset", QString::number(offset) } }), bytes);
            offset += bytes.size();
            updateProgress(offset, size);
        }
        require(file.size() == size && QFileInfo(file).lastModified() == source.lastModified(), tr("Source changed during transfer; the destination was preserved."));
        canceled(token);
        auto result = post(http, query({ { "op", "finish" }, { "id", id } }));
        require(result.value("sha256").toString().toLatin1() == checksum, tr("Final file checksum did not match."));
        id.clear();
    } catch (...) {
        cleanup(http, id);
        throw;
    }
}
void FileTransfer::setQueueStatus(QString status, QString message)
{
    if (m_QueueIndex >= m_Queue.size()) return;
    auto entry = m_Queue[m_QueueIndex].toMap();
    entry["status"] = status;
    entry["message"] = message;
    m_Queue[m_QueueIndex] = entry;
}
void FileTransfer::sendFiles(QVariantList localFiles, QString folder, QString relativeDirectory)
{
    if (m_Busy || m_QueueActive) return;
    m_Error.clear();
    if (localFiles.isEmpty() || localFiles.size() > 32) m_Error = tr("Choose between 1 and 32 regular files per copy.");
    QVariantList entries;
    for (const auto& value : localFiles) {
        if (!m_Error.isEmpty()) break;
        const auto url = value.toUrl();
        QFileInfo info(url.toLocalFile());
        if (!url.isLocalFile() || !info.isFile() || info.isSymLink() || info.size() > maxFileBytes) {
            m_Error = tr("Choose regular local files up to 8 GiB each; folders and links cannot be copied.");
            break;
        }
        entries.append(QVariantMap{{"url", url}, {"name", info.fileName()}, {"folder", folder}, {"directory", relativeDirectory},
                                   {"destination", folder + (relativeDirectory.isEmpty() ? QString() : "/" + relativeDirectory)}, {"status", tr("Waiting")}});
    }
    if (!m_Error.isEmpty()) { emit changed(); return; }
    if (!m_Supported || !m_Computer) { m_Error = tr("This host does not support file transfer."); emit changed(); return; }
    m_Queue = entries;
    m_QueueIndex = 0;
    m_QueueActive = true;
    m_CollisionName.clear();
    sendNext();
}
void FileTransfer::sendNext()
{
    if (!m_QueueActive || m_Busy) return;
    if (!m_Supported) {
        while (m_QueueIndex < m_Queue.size()) {
            setQueueStatus(tr("Failed"), tr("This host does not support file transfer."));
            ++m_QueueIndex;
        }
        m_QueueActive = false;
        emit changed();
        return;
    }
    const auto entry = m_Queue[m_QueueIndex].toMap();
    setQueueStatus(tr("Copying"));
    start([this, entry](NvHTTP& http, const Token& token) {
        const auto folder = entry.value("folder").toString();
        const auto directory = entry.value("directory").toString();
        auto name = entry.value("name").toString();
        const auto listing = object(get(http, query({{"op", "list"}, {"folder", folder}, {"path", directory}}), token));
        QSet<QString> names;
        for (const auto& value : listing.value("entries").toArray()) names.insert(value.toObject().value("name").toString().toCaseFolded());
        if (entry.value("keepBoth").toBool()) {
            const QFileInfo info(name);
            const auto suffix = info.completeSuffix();
            const auto stem = suffix.isEmpty() ? name : name.left(name.size() - suffix.size() - 1);
            for (int i = 2; names.contains(name.toCaseFolded()); ++i) {
                require(i <= 10000, tr("Could not find a free destination name; choose another name."));
                name = stem + QStringLiteral(" (%1)").arg(i) + (suffix.isEmpty() ? QString() : "." + suffix);
            }
        } else if (names.contains(name.toCaseFolded())) {
            canceled(token);
            QMetaObject::invokeMethod(this, [this, name, token] { if (m_QueueActive && !token->isCancelled()) { m_CollisionName = name; setQueueStatus(tr("Needs a name")); emit changed(); } }, Qt::QueuedConnection);
            return;
        }
        canceled(token);
        QMetaObject::invokeMethod(this, [this, name] { auto e = m_Queue[m_QueueIndex].toMap(); e["name"] = name; m_Queue[m_QueueIndex] = e; emit changed(); }, Qt::QueuedConnection);
        upload(http, token, entry.value("url").toUrl(), folder, directory, name, false);
    });
}
void FileTransfer::resolveCollision(QString action)
{
    if (!m_QueueActive || m_Busy || m_CollisionName.isEmpty()) return;
    m_CollisionName.clear();
    if (action == "keepBoth") {
        auto entry = m_Queue[m_QueueIndex].toMap(); entry["keepBoth"] = true; m_Queue[m_QueueIndex] = entry;
        sendNext();
    } else {
        setQueueStatus(tr("Skipped"));
        if (++m_QueueIndex >= m_Queue.size()) m_QueueActive = false;
        else sendNext();
    }
    emit changed();
}
void FileTransfer::revealReceivedFile()
{
    if (m_CompletedLocalFile.isLocalFile() && QFileInfo(m_CompletedLocalFile.toLocalFile()).isFile())
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_CompletedLocalFile.toLocalFile()).absolutePath()));
}
void FileTransfer::receiveFile(QString folder, QString relativeFile, QUrl localFile, bool overwrite)
{
    if (m_QueueActive || m_Busy) return;
    m_CompletedLocalFile = QUrl();
    start([this, folder, relativeFile, localFile, overwrite](NvHTTP& http, const Token& token) {
        QString id;
        try {
            require(localFile.isLocalFile(), tr("Choose a local destination file."));
            const auto target = localFile.toLocalFile();
            QFileInfo destination(target);
            require(!destination.exists() || (overwrite && destination.isFile() && !destination.isSymLink()), tr("Destination exists; choose another name or explicitly replace the file."));
            const auto original = inspectDestination(target);
            QTemporaryFile temp(QDir(destination.absolutePath()).filePath(".starlight-XXXXXX.part"));
            require(temp.open(), temp.errorString());
            auto begin = post(http, query({ { "op", "beginDownload" }, { "folder", folder }, { "path", relativeFile } }));
            id = begin.value("id").toString();
            bool ok = false;
            const auto size = begin.value("size").toVariant().toLongLong(&ok);
            require(!id.isEmpty() && ok && size >= 0 && size <= maxFileBytes, tr("Host returned an invalid file size."));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            const auto transferChunk = negotiatedChunkSize(begin);
            qint64 offset = 0;
            updateProgress(0, size);
            while (offset < size) {
                canceled(token);
                auto bytes = get(http, query({ { "op", "read" }, { "id", id }, { "offset", QString::number(offset) } }), token);
                require(bytes.size() == qMin(transferChunk, size - offset), tr("Host returned an incomplete file chunk."));
                require(temp.write(bytes) == bytes.size(), temp.errorString());
                hash.addData(bytes);
                offset += bytes.size();
                updateProgress(offset, size);
            }
            canceled(token);
            auto finish = post(http, query({ { "op", "finish" }, { "id", id } }));
            id.clear();
            require(finish.value("sha256").toString().toLatin1() == hash.result().toHex(), tr("File checksum failed; the original destination was preserved."));
            require(temp.flush(), temp.errorString());
            canceled(token);
            if (overwrite) {
                require(inspectDestination(target) == original, tr("Destination changed during transfer; newer contents were preserved."));
                require(temp.seek(0), temp.errorString());
                QSaveFile output(target);
                output.setDirectWriteFallback(false);
                require(output.open(QIODevice::WriteOnly), output.errorString());
                while (!temp.atEnd()) {
                    canceled(token);
                    auto bytes = temp.read(chunkBytes);
                    require(!bytes.isEmpty() && temp.error() == QFile::NoError, temp.errorString());
                    require(output.write(bytes) == bytes.size(), output.errorString());
                }
                canceled(token);
#ifdef FILE_TRANSFER_TESTING
                if (FileTransferTestHooks::beforeCommit)
                    FileTransferTestHooks::beforeCommit();
#endif
                require(inspectDestination(target) == original, tr("Destination changed while publishing; newer contents were preserved."));
                require(output.commit(), output.errorString());
            } else {
#ifdef Q_OS_UNIX
                require(::link(QFile::encodeName(temp.fileName()).constData(), QFile::encodeName(target).constData()) == 0, tr("Destination appeared or could not be published; existing files were preserved."));
#else
                require(QFile::rename(temp.fileName(), target), tr("Destination appeared or could not be published; existing files were preserved."));
#endif
            }
            QMetaObject::invokeMethod(this, [this, localFile] { m_CompletedLocalFile = localFile; emit changed(); }, Qt::QueuedConnection);
        } catch (...) {
            cleanup(http, id);
            throw;
        }
    });
}
