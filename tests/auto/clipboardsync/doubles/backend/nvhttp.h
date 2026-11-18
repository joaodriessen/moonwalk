#pragma once
// Controlled HTTP seam; the worker, queue, focus generations, cancellation calls,
// retry scheduling and loop guard are the unchanged production implementation.
#include <QByteArray>
#include <QMutex>
#include <QList>
#include <QNetworkReply>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <functional>
#include <stdexcept>

class NvComputer;
class GfeHttpResponseException : public std::runtime_error {
public:
    explicit GfeHttpResponseException(int code) : std::runtime_error("HTTP failure"), m_Code(code) {}
    int getStatusCode() const { return m_Code; }
private: int m_Code;
};
class QtNetworkReplyException : public std::runtime_error {
public:
    explicit QtNetworkReplyException(QNetworkReply::NetworkError error)
        : std::runtime_error("controlled network failure"), m_Error(error) {}
    QNetworkReply::NetworkError getError() const { return m_Error; }
private: QNetworkReply::NetworkError m_Error;
};
struct ClipboardTransport {
    inline static std::atomic<int> gets{0}, posts{0}, failPosts{0};
    inline static std::atomic<bool> gateGet{false}, allowArm{false}, holdPost{false};
    inline static std::atomic<bool> repeatResponses{false};
    inline static std::atomic<int> httpError{0}, remoteStatus{0};
    inline static QByteArray remoteBody,remoteType,remoteHeader;
    inline static QMutex mutex;
    inline static QList<QByteArray> bodies;
    inline static QList<QString> requests;
    inline static std::function<void()> onPost;
    static void reset() {
        gets = 0; posts = 0; failPosts = 0; gateGet = false; allowArm = false;
        repeatResponses = false;
        holdPost = false; httpError = 0; remoteStatus=0;remoteBody.clear();remoteType.clear();remoteHeader.clear();bodies.clear();requests.clear(); onPost = {};
    }
    static QList<QByteArray> sent() { QMutexLocker lock(&mutex); return bodies; }
    static QList<QString> queries() { QMutexLocker lock(&mutex); return requests; }
};
class NvHTTP {
public:
    enum NvLogLevel { NVLL_NONE, NVLL_ERROR, NVLL_VERBOSE };
    struct RawResponse { int httpStatusCode = 204; QByteArray body, headerValue, contentType; };
    struct CancelToken {
        std::atomic<bool> cancelled{false}, interrupted{false};
        void cancel() { cancelled = true; }
        void interrupt() { interrupted = true; }
        void abortIfArmed() { interrupted = true; }
    };
    explicit NvHTTP(NvComputer*) {}
    QUrl m_BaseUrlHttps;
    void setCancelToken(CancelToken* token) { m_PushToken = token; }
    RawResponse openConnectionRaw(QUrl, QString, QString arguments, int, QByteArray, NvLogLevel, CancelToken* token, qint64=1024*1024) {
        { QMutexLocker lock(&ClipboardTransport::mutex); ClipboardTransport::requests.append(arguments); }
        ++ClipboardTransport::gets;
        const auto status=ClipboardTransport::repeatResponses ? ClipboardTransport::remoteStatus.load() : ClipboardTransport::remoteStatus.exchange(0);
        if(status)return {status,ClipboardTransport::remoteBody,ClipboardTransport::remoteHeader,ClipboardTransport::remoteType};
        while (ClipboardTransport::gateGet && !ClipboardTransport::allowArm && !token->cancelled)
            QThread::msleep(1);
        // A normal host can hold a GET for25seconds. The test holds it until
        // interruption; no real clipboard or socket is involved.
        while (!token->cancelled && !token->interrupted.exchange(false)) QThread::msleep(1);
        throw QtNetworkReplyException(QNetworkReply::OperationCanceledError);
    }
    void openConnectionPost(QUrl, QString, QString, QByteArray body, QString, int, NvLogLevel) {
        {
            QMutexLocker lock(&ClipboardTransport::mutex);
            ClipboardTransport::bodies.append(body);
        }
        const int attempt = ++ClipboardTransport::posts;
        if (ClipboardTransport::onPost) ClipboardTransport::onPost();
        while (ClipboardTransport::holdPost && !m_PushToken->cancelled && !m_PushToken->interrupted.exchange(false))
            QThread::msleep(1);
        if (m_PushToken->cancelled) throw QtNetworkReplyException(QNetworkReply::OperationCanceledError);
        if (ClipboardTransport::httpError) throw GfeHttpResponseException(ClipboardTransport::httpError);
        if (attempt <= ClipboardTransport::failPosts) throw QtNetworkReplyException(QNetworkReply::TimeoutError);
    }
private:
    CancelToken* m_PushToken = nullptr;
};
