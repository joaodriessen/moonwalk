#pragma once
#include <QByteArray>
#include <QNetworkReply>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <atomic>
#include <functional>
#include <stdexcept>
class NvComputer;
struct FileTransport {
    inline static std::function<QByteArray(QString, QByteArray)> respond;
    inline static std::atomic<bool> supported { true }, holdChunk { false }, chunkStarted { false };
    inline static std::atomic<int> cancels { 0 }, requests { 0 };
    static void reset()
    {
        supported = true;
        holdChunk = false;
        chunkStarted = false;
        cancels = 0;
        requests = 0;
        respond = { };
    }
};
class NvHTTP {
public:
    enum NvLogLevel { NVLL_NONE,
        NVLL_ERROR,
        NVLL_VERBOSE };
    struct CancelToken {
        std::atomic<bool> flag { false };
        void cancel() { flag = true; }
        bool isCancelled() const { return flag; }
    };
    struct RawResponse {
        int httpStatusCode = 200;
        QByteArray body, headerValue, contentType;
    };
    explicit NvHTTP(NvComputer*) { }
    QUrl m_BaseUrlHttps;
    void setCancelToken(CancelToken* token) { m_Token = token; }
    QString getServerInfo(NvLogLevel, bool) { return FileTransport::supported ? "1" : "0"; }
    static QString getXmlString(QString xml, QString) { return xml; }
    QString openConnectionPost(QUrl, QString, QString arguments, QByteArray body, QString, int, NvLogLevel)
    {
        auto op = QUrlQuery(arguments).queryItemValue("op");
        ++FileTransport::requests;
        if (op == "cancel") {
            ++FileTransport::cancels;
            return "{}";
        }
        if (op == "chunk") {
            FileTransport::chunkStarted = true;
            while (FileTransport::holdChunk && m_Token && !m_Token->isCancelled())
                QThread::msleep(1);
        }
        if (m_Token && m_Token->isCancelled())
            throw std::runtime_error("Canceled");
        return QString::fromUtf8(FileTransport::respond(arguments, body));
    }
    RawResponse openConnectionRaw(QUrl, QString, QString arguments, int, QByteArray, NvLogLevel, CancelToken* token, qint64)
    {
        ++FileTransport::requests;
        if (token && token->isCancelled())
            throw std::runtime_error("Canceled");
        return { 200, FileTransport::respond(arguments, { }), { }, "application/octet-stream" };
    }

private:
    CancelToken* m_Token = nullptr;
};
