#pragma once

#include "identitymanager.h"
#include "nvapp.h"
#include "nvaddress.h"

#include <Limelight.h>

#include <QUrl>
#include <QByteArray>
#include <QMutex>
#include <QSize>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class NvComputer;

class NvDisplayMode
{
public:
    bool operator==(const NvDisplayMode& other) const
    {
        return width == other.width &&
                height == other.height &&
                refreshRate == other.refreshRate;
    }

    int width;
    int height;
    int refreshRate;
};
Q_DECLARE_TYPEINFO(NvDisplayMode, Q_PRIMITIVE_TYPE);

class GfeHttpResponseException : public std::exception
{
public:
    GfeHttpResponseException(int statusCode, QString message) :
        m_StatusCode(statusCode),
        m_StatusMessage(message.toUtf8())
    {

    }

    const char* what() const throw()
    {
        return m_StatusMessage.constData();
    }

    const char* getStatusMessage() const
    {
        return m_StatusMessage.constData();
    }

    int getStatusCode() const
    {
        return m_StatusCode;
    }

    QString toQString() const
    {
        // Hosts sometimes refuse with a code and no words.
        const QString code = "Error " + QString::number(m_StatusCode);
        const QString message = QString::fromUtf8(m_StatusMessage).trimmed();
        return message.isEmpty() ? code : message + " (" + code + ")";
    }

private:
    int m_StatusCode;
    QByteArray m_StatusMessage;
};

class QtNetworkReplyException : public std::exception
{
public:
    QtNetworkReplyException(QNetworkReply::NetworkError error, QString errorText) :
        m_Error(error),
        m_ErrorText(errorText.toUtf8())
    {

    }

    const char* what() const throw()
    {
        return m_ErrorText.constData();
    }

    const char* getErrorText() const
    {
        return m_ErrorText.constData();
    }

    QNetworkReply::NetworkError getError() const
    {
        return m_Error;
    }

    QString toQString() const
    {
        const QString code = "Error " + QString::number(m_Error);
        const QString message = QString::fromUtf8(m_ErrorText).trimmed();
        return message.isEmpty() ? code : message + " (" + code + ")";
    }

private:
    QNetworkReply::NetworkError m_Error;
    QByteArray m_ErrorText;
};

class NvHTTP : public QObject
{
    Q_OBJECT

public:
    enum NvLogLevel {
        NVLL_NONE,
        NVLL_ERROR,
        NVLL_VERBOSE
    };

    // Result of openConnectionRaw() (below) - a status-preserving GET used by the
    // in-stream clipboard long-poll (app/streaming/clipboardsync.cpp). Populated only
    // when a real HTTP response was received, including 4xx/5xx statuses; a genuine
    // transport-level failure (including an incomplete body) throws instead - see
    // openConnectionRaw()'s doc comment.
    struct RawResponse {
        int httpStatusCode = 0;
        QByteArray body;
        // Value of the single caller-requested response header (raw bytes, no
        // charset decoding needed - it's always a small ASCII integer here), empty
        // if the header was absent.
        QByteArray headerValue;
        QByteArray contentType;
    };

    // Cooperative cross-thread cancellation for a single in-flight openConnectionRaw()
    // call. Not clipboard-specific by name, but exists to solve exactly one problem:
    // the clipboard long-poll's GET can be held by the host for up to ~25-30s, and
    // that wait must be abortable from another thread (e.g. session teardown)
    // without blocking that other thread and without racing the reply's deletion.
    //
    // Thread-safety contract: arm()/disarm() are called only from the thread that
    // owns the QNetworkReply (i.e. from inside openConnectionRaw(), on whichever
    // thread constructed the owning NvHTTP - the dedicated clipboard thread).
    // abortIfArmed() is called from ANY other thread wanting to cancel the current
    // wait. The mutex guarantees that if abortIfArmed() observes a non-null reply,
    // that reply is still alive and owned by its thread for at least as long as it
    // takes to resolve QNetworkReply's metaobject and post the queued abort() call -
    // openConnectionRaw() cannot reach disarm()+delete without taking the same lock.
    class CancelToken {
    public:
        void arm(QNetworkReply *reply);
        void disarm(QNetworkReply *reply);

        // Requests that the currently in-flight request (if any) abort as soon as
        // possible. Safe to call from any thread, including while no request is in
        // flight (no-op). Non-blocking: QNetworkReply::abort() is invoked via a
        // queued connection, so it actually executes on the reply's own thread (the
        // nested QEventLoop inside openConnectionRaw() will dispatch it), never here.
        void abortIfArmed();

        // Interrupt the current request, or the next one if it has not armed yet.
        // Unlike cancel(), this is consumed by one request and permits retries.
        void interrupt();

        // Permanently cancel this operation, including requests not armed yet.
        void cancel();
        bool isCancelled() const;

    private:
        mutable QMutex m_Mutex;
        bool m_Cancelled = false;
        bool m_InterruptPending = false;
        QNetworkReply *m_Reply = nullptr;
    };

    explicit NvHTTP(NvAddress address, uint16_t httpsPort, QSslCertificate serverCert, bool useTrueUid, QNetworkAccessManager* nam = nullptr);

    explicit NvHTTP(NvComputer* computer, QNetworkAccessManager* nam = nullptr);

    static
    int
    getCurrentGame(QString serverInfo);

    QString
    getServerInfo(NvLogLevel logLevel, bool fastFail = false);

    // A paired client's role view, required before/after changing a remote target.
    QString getAuthenticatedServerInfo(NvLogLevel logLevel, bool fastFail = false);

    static
    void
    verifyResponseStatus(QString xml);

    static
    QString
    getXmlString(QString xml,
                 QString tagName);

    static
    QByteArray
    getXmlStringFromHex(QString xml,
                        QString tagName);

    QString
    openConnectionToString(QUrl baseUrl,
                           QString command,
                           QString arguments,
                           int timeoutMs,
                           NvLogLevel logLevel = NvLogLevel::NVLL_VERBOSE);

    // POST counterpart to openConnectionToString()/openConnection(). The client is
    // otherwise GET-only; this is the sole POST capability, added for the in-stream
    // clipboard sync (app/streaming/clipboardsync.cpp). Mirrors openConnection()'s
    // cert-attach and server-pin (handleSslErrors) machinery verbatim, swapping
    // m_Nam->get() for m_Nam->post(). Returns the response body as a string (empty for
    // the clipboard host's 200-empty success case). Throws GfeHttpResponseException /
    // QtNetworkReplyException on failure, exactly like openConnectionToString().
    QString
    openConnectionPost(QUrl baseUrl,
                       QString command,
                       QString arguments,
                       QByteArray body,
                       QString contentType,
                       int timeoutMs,
                       NvLogLevel logLevel = NvLogLevel::NVLL_VERBOSE);

    // Status-preserving GET, added for the in-stream clipboard sync's host->client
    // long-poll (design doc Part 1.2/1.3; see clipboardsync.cpp). Unlike
    // openConnection()/openConnectionToString(), an HTTP-level response - including
    // 4xx/5xx statuses such as 404/403/400/503 and no-content 204 - is returned as
    // data in a RawResponse, NOT raised as an exception. Only a genuine transport-
    // level failure, including incomplete response bodies, timeout, cancellation,
    // connection refusal, DNS failure or TLS/cert mismatch, throws
    // GfeHttpResponseException/QtNetworkReplyException, exactly like openConnection().
    // This lets the caller cleanly distinguish "endpoint absent" (404/403) from
    // "no change yet" (204) from "host unreachable" (throws) without exception-driven
    // control flow for the merely-uninteresting statuses.
    //
    // Entity-bearing responses must declare finite Content-Length and identity
    // encoding. Qt can treat EOF in an incomplete chunked body as successful,
    // so this seam rejects transfer encodings or unframed entities. The host
    // clipboard/cursor endpoints emit Content-Length; 204/304 remain bodyless.
    //
    // `headerNameToCapture` (may be empty to skip): the single response header whose
    // value is copied into RawResponse::headerValue.
    //
    // `cancelToken` (may be nullptr): if provided, the in-flight QNetworkReply is
    // registered with it for the duration of the call so another thread can request
    // early cancellation via CancelToken::abortIfArmed() (used to make clipboard
    // thread shutdown not wait out the full long-poll hold - see
    // ClipboardSync::stopAndWait()). A cancelled request surfaces here as the same
    // transport-level timeout/abort path as a natural timeout.
    RawResponse
    openConnectionRaw(QUrl baseUrl,
                      QString command,
                      QString arguments,
                      int timeoutMs,
                      const QByteArray& headerNameToCapture,
                      NvLogLevel logLevel = NvLogLevel::NVLL_ERROR,
                      CancelToken* cancelToken = nullptr,
                      qint64 maxResponseBytes = 1024 * 1024);

    // Token must outlive this NvHTTP and its requests.
    void setCancelToken(CancelToken* token) { m_CancelToken = token; }

    void setServerCert(QSslCertificate serverCert);
    void setAddress(NvAddress address);
    void setHttpsPort(uint16_t port);
    void setTrueUid(bool useTrueUid);

    NvAddress address();

    QSslCertificate serverCert();

    uint16_t httpPort();

    uint16_t httpsPort();

    static
    QVector<int>
    parseQuad(QString quad);

    void
    quitApp();

    void disconnectRemoteMonitor();
    void takeOverRetainedDesktop(const QString& hostUuid, const NvApp& expectedDesktop);
    void verifyMainDesktopOwnership(const QString& hostUuid, const NvApp& expectedDesktop);

    void
    startApp(QString verb,
             bool isGfe,
             int appId,
             PSTREAM_CONFIGURATION streamConfig,
             bool sops,
             bool localAudio,
             int gamepadMask,
             bool persistGameControllersOnDisconnect,
             bool clientSideCursor,
             QString& rtspSessionUrl,
             QString desktopScale = QString(),
             int adaptiveBitrate = -1,
             bool allowResumeLaunchFallback = true,
             int virtualDisplay = -1,
             QSize desktopSize = QSize(),
             QString appUuid = QString(),
             QString* hostWarning = nullptr);

    QVector<NvApp>
    getAppList();

    QImage
    getBoxArt(int appId);

    static
    QVector<NvDisplayMode>
    getDisplayModeList(QString serverInfo);

    QUrl m_BaseUrlHttp;
    QUrl m_BaseUrlHttps;
private:
    void
    handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);

    QNetworkReply*
    openConnection(QUrl baseUrl,
                   QString command,
                   QString arguments,
                   int timeoutMs,
                   NvLogLevel logLevel);

    CancelToken* m_CancelToken = nullptr;
    NvAddress m_Address;
    QNetworkAccessManager* m_Nam;
    QSslCertificate m_ServerCert;
    bool m_UseTrueUid;
};
