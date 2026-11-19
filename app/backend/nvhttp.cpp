#include "nvcomputer.h"
#include "boundedreply.h"
#include <Limelight.h>

#include <QDebug>
#include <QUuid>
#include <QtNetwork/QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QXmlStreamReader>
#include <QSslKey>
#include <QImageReader>
#include <QtEndian>
#include <QNetworkProxy>
#include <QMetaObject>
#include <QMutexLocker>

#define FAST_FAIL_TIMEOUT_MS 2000
#define REQUEST_TIMEOUT_MS 5000
#define LAUNCH_TIMEOUT_MS 120000
#define RESUME_TIMEOUT_MS 30000
#define QUIT_TIMEOUT_MS 30000

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define XML_NAME_EQUALS(x, y) ((x) == (y))
#else
#define XML_NAME_EQUALS(x, y) ((x) == (u##y))
#endif

static QString requestLocation(const QUrl& url)
{
    // Pairing and launch arguments carry certificates and session keys.
    return url.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

static QString networkErrorDetail(const QNetworkReply& reply)
{
    const auto url = reply.url();
    auto detail = reply.errorString();
    const auto location = requestLocation(url);
    detail.replace(url.toString(QUrl::FullyEncoded), location);
    detail.replace(url.toString(), location);
    return detail;
}

// Stock hosts may report a successful stream that uses an existing display
// instead of their requested virtual display. This is observation only: never
// turn a warning into a cancel, retry, or a different launch request.
static QString hostDisplayWarning(const QString& response)
{
    if (response.size() > 16384) return {};
    QXmlStreamReader xml(response);
    if (!xml.readNextStartElement() || xml.name() != QStringLiteral("root") ||
        !xml.namespaceUri().isEmpty() ||
        xml.attributes().value(QStringLiteral("status_code")) != QStringLiteral("200")) return {};

    bool started = false, warningSeen = false, duplicateWarning = false;
    QString code;
    while (xml.readNextStartElement()) {
        const auto name = xml.name().toString();
        if (!xml.namespaceUri().isEmpty()) xml.skipCurrentElement();
        else if (name == QStringLiteral("clientWarning")) {
            duplicateWarning |= warningSeen;
            warningSeen = true;
            code = xml.readElementText();
        }
        else if (name == QStringLiteral("gamesession") || name == QStringLiteral("resume")) {
            started |= xml.readElementText() == QStringLiteral("1");
        }
        else xml.skipCurrentElement();
    }
    while (!xml.atEnd()) xml.readNext();
    if (xml.hasError() || duplicateWarning || !started ||
        code != QStringLiteral("virtual_display_disabled_by_client")) return {};

    // Use local plain text, not untrusted host markup. Unknown optional warning
    // codes and legacy responses leave the connection unchanged.
    return QCoreApplication::translate("Session",
        "The host reports that this client's settings disabled its virtual display. "
        "Streaming continues with the existing display. Choose Automatic or Use host setting under Host display to use the host's display policy.");
}

NvHTTP::NvHTTP(NvAddress address, uint16_t httpsPort, QSslCertificate serverCert, bool useTrueUid, QNetworkAccessManager* nam) :
    m_Nam(nam ? nam : new QNetworkAccessManager(this)),
    m_ServerCert(serverCert),
    m_UseTrueUid(useTrueUid)
{
    m_BaseUrlHttp.setScheme("http");
    m_BaseUrlHttps.setScheme("https");

    setAddress(address);
    setHttpsPort(httpsPort);

    // Never use a proxy server
    QNetworkProxy noProxy(QNetworkProxy::NoProxy);
    m_Nam->setProxy(noProxy);
}

NvHTTP::NvHTTP(NvComputer* computer, QNetworkAccessManager* nam) :
    NvHTTP(computer->activeAddress, computer->activeHttpsPort, computer->serverCert, !computer->isNvidiaServerSoftware, nam)
{
}

void NvHTTP::setServerCert(QSslCertificate serverCert)
{
    m_ServerCert = serverCert;
}

void NvHTTP::setAddress(NvAddress address)
{
    Q_ASSERT(!address.isNull());

    m_Address = address;

    m_BaseUrlHttp.setHost(address.address());
    m_BaseUrlHttps.setHost(address.address());

    m_BaseUrlHttp.setPort(address.port());
}

void NvHTTP::setHttpsPort(uint16_t port)
{
    m_BaseUrlHttps.setPort(port);
}

void NvHTTP::setTrueUid(bool useTrueUid)
{
    m_UseTrueUid = useTrueUid;
}

NvAddress NvHTTP::address()
{
    return m_Address;
}

QSslCertificate NvHTTP::serverCert()
{
    return m_ServerCert;
}

uint16_t NvHTTP::httpPort()
{
    return m_BaseUrlHttp.port();
}

uint16_t NvHTTP::httpsPort()
{
    return m_BaseUrlHttps.port();
}

QVector<int>
NvHTTP::parseQuad(QString quad)
{
    QVector<int> ret;

    // Return an empty vector for old GFE versions
    // that were missing GfeVersion.
    if (quad.isEmpty()) {
        return ret;
    }

    QStringList parts = quad.split(".");
    ret.reserve(parts.length());
    for (int i = 0; i < parts.length(); i++)
    {
        ret.append(parts.at(i).toInt());
    }

    return ret;
}

int
NvHTTP::getCurrentGame(QString serverInfo)
{
    // GFE 2.8 started keeping currentgame set to the last game played. As a result, it no longer
    // has the semantics that its name would indicate. To contain the effects of this change as much
    // as possible, we'll force the current game to zero if the server isn't in a streaming session.
    QString serverState = getXmlString(serverInfo, "state");
    if (serverState.endsWith("_SERVER_BUSY"))
    {
        return getXmlString(serverInfo, "currentgame").toInt();
    }
    else
    {
        return 0;
    }
}

QString
NvHTTP::getServerInfo(NvLogLevel logLevel, bool fastFail)
{
    QString serverInfo;

    // Check if we have a pinned cert and HTTPS port for this host yet
    if (!m_ServerCert.isNull() && httpsPort() != 0)
    {
        try
        {
            // Always try HTTPS first, since it properly reports
            // pairing status (and a few other attributes).
            serverInfo = openConnectionToString(m_BaseUrlHttps,
                                                "serverinfo",
                                                nullptr,
                                                fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                                logLevel);
            // Throws if the request failed
            verifyResponseStatus(serverInfo);
        }
        catch (const GfeHttpResponseException& e)
        {
            if (e.getStatusCode() == 401)
            {
                // Certificate validation error, fallback to HTTP
                serverInfo = openConnectionToString(m_BaseUrlHttp,
                                                    "serverinfo",
                                                    nullptr,
                                                    fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                                    logLevel);
                verifyResponseStatus(serverInfo);
            }
            else
            {
                // Rethrow real errors
                throw e;
            }
        }
    }
    else
    {
        // Only use HTTP prior to pairing or fetching HTTPS port
        serverInfo = openConnectionToString(m_BaseUrlHttp,
                                            "serverinfo",
                                            nullptr,
                                            fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                            logLevel);
        verifyResponseStatus(serverInfo);

        // Populate the HTTPS port
        uint16_t httpsPort = getXmlString(serverInfo, "HttpsPort").toUShort();
        if (httpsPort == 0) {
            httpsPort = DEFAULT_HTTPS_PORT;
        }
        setHttpsPort(httpsPort);

        // If we just needed to determine the HTTPS port, we'll try again over
        // HTTPS now that we have the port number
        if (!m_ServerCert.isNull()) {
            return getServerInfo(logLevel, fastFail);
        }
    }

    return serverInfo;
}

QString NvHTTP::getAuthenticatedServerInfo(NvLogLevel logLevel, bool fastFail)
{
    if (m_ServerCert.isNull() || httpsPort() == 0)
        throw GfeHttpResponseException(401,"The host's paired identity is not available");
    // Public discovery can report global idle while this client's role is paused.
    // Never use that view as confirmation before/after a target operation.
    m_Nam->clearConnectionCache();
    const QString response = openConnectionToString(m_BaseUrlHttps,"serverinfo",nullptr,
        fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,logLevel);
    verifyResponseStatus(response);
    if (getXmlString(response,"PairStatus") != QStringLiteral("1"))
        throw GfeHttpResponseException(401,"The host did not confirm the paired client's identity");
    return response;
}

void
NvHTTP::startApp(QString verb,
                 bool isGfe,
                 int appId,
                 PSTREAM_CONFIGURATION streamConfig,
                 bool sops,
                 bool localAudio,
                 int gamepadMask,
                 bool persistGameControllersOnDisconnect,
                 bool clientSideCursor,
                 QString& rtspSessionUrl,
                 QString desktopScale,
                 int adaptiveBitrate,
                 bool allowResumeLaunchFallback,
                 int virtualDisplay,
                 QSize desktopSize,
                 QString appUuid,
                 QString* hostWarning)
{
    if (hostWarning) hostWarning->clear();
    int riKeyId;

    memcpy(&riKeyId, streamConfig->remoteInputAesIv, sizeof(riKeyId));
    riKeyId = qFromBigEndian(riKeyId);

    // Sunshine's launch mode configures the desktop; RTSP independently requests
    // the encoded viewport. Keep desktop geometry stable during Automatic trials
    // on hosts that cannot acknowledge a corresponding desktop-scale change.
    const QSize mode = !isGfe && desktopSize.isValid() ? desktopSize
        : QSize(streamConfig->width, streamConfig->height);

    QString arguments =
                                   "appid="+QString::number(appId)+
                                   "&mode="+QString::number(mode.width())+"x"+
                                   QString::number(mode.height())+"x"+
                                   // Using an FPS value over 60 causes SOPS to default to 720p60,
                                   // so force it to 0 to ensure the correct resolution is set. We
                                   // used to use 60 here but that locked the frame rate to 60 FPS
                                   // on GFE 3.20.3. We don't need this hack for Sunshine.
                                   QString::number((streamConfig->fps > 60 && isGfe) ? 0 : streamConfig->fps)+
                                   "&additionalStates=1&sops="+QString::number(sops ? 1 : 0)+
                                   "&rikey="+QByteArray(streamConfig->remoteInputAesKey, sizeof(streamConfig->remoteInputAesKey)).toHex()+
                                   "&rikeyid="+QString::number(riKeyId)+
                                   ((streamConfig->supportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) ?
                                       "&hdrMode=1&clientHdrCapVersion=0&clientHdrCapSupportedFlagsInUint32=0&clientHdrCapMetaDataId=NV_STATIC_METADATA_TYPE_1&clientHdrCapDisplayData=0x0x0x0x0x0x0x0x0x0x0" :
                                        // SDR, said outright to Sunshine-family hosts.
                                        (isGfe ? "" : "&hdrMode=0"))+
                                   // The stream's fps is the display's refresh. Asking for VRR makes
                                   // Vibepollo drive its virtual display at 1000 Hz, which costs the
                                   // host for frames nobody sees.
                                   "&clientVrrRequested=0"+
                                   "&localAudioPlayMode="+QString::number(localAudio ? 1 : 0)+
                                   "&surroundAudioInfo="+QString::number(SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(streamConfig->audioConfiguration))+
                                   "&remoteControllersBitmap="+QString::number(gamepadMask)+
                                   "&gcmap="+QString::number(gamepadMask)+
                                   "&gcpersist="+QString::number(persistGameControllersOnDisconnect ? 1 : 0)+
                                   // clientSideCursor: cursor-coordination launch param (matches
                                   // the name/semantics of the closed-unmerged moonlight-qt PR
                                   // #1785 / Apollo PR #1336 prior art). ALWAYS sent, to every
                                   // host, both values -- unknown query params are ignored by
                                   // stock Sunshine/GFE/Apollo, and always-sending is what lets a
                                   // coordinating host (Skylight) suppress its own composited
                                   // cursor in both directions (on and off) without a
                                   // software-detection heuristic on the host side.
                                   // Value semantics: reflects what THIS client will actually draw,
                                   // not just the raw "Instant cursor" preference -- localCursor
                                   // only has any visible effect once absoluteMouseMode is also on
                                   // (see SdlInputHandler::setCaptureActive(),
                                   // app/streaming/input/input.cpp:384: the local cursor is only
                                   // shown when `m_AbsoluteMouseMode` is true; in relative-mouse
                                   // mode the OS cursor is simply grabbed regardless of
                                   // localCursor). Telling the host to hide its cursor while the
                                   // client draws nothing would recreate the no-cursor bug this
                                   // feature exists to fix, so the caller passes
                                   // localCursor && absoluteMouseMode, not localCursor alone.
                                   "&clientSideCursor="+QString::number(clientSideCursor ? 1 : 0)+
                                   LiGetLaunchUrlQueryParameters();

    if (desktopScale == "auto" || desktopScale == "1" || desktopScale == "2")
        arguments += "&desktopScale=" + desktopScale;
    if (adaptiveBitrate == 0 || adaptiveBitrate == 1)
        arguments += "&adaptiveBitrate=" + QString::number(adaptiveBitrate);
    if (virtualDisplay == 0 || virtualDisplay == 1)
        arguments += "&virtualDisplay=" + QString::number(virtualDisplay);

    if (!isGfe && !QUuid(appUuid).isNull())
        arguments += "&appuuid=" + QString::fromLatin1(QUrl::toPercentEncoding(appUuid));

    bool identityRetried = false;
    auto requestStart = [&](const QString& requestVerb) {
        QString response = openConnectionToString(m_BaseUrlHttps, requestVerb,
                                                   arguments, LAUNCH_TIMEOUT_MS);
        try {
            verifyResponseStatus(response);
        }
        catch (const GfeHttpResponseException& error) {
            // Retry this specific missing-peer-identity rejection once,
            // using the same certificate, pinned server and requested verb.
            if (identityRetried || error.getStatusCode() != 403 ||
                    QString::fromUtf8(error.getStatusMessage()).trimmed() != QStringLiteral("A paired TLS client identity is required")) throw;
            identityRetried = true;
            m_Nam->clearConnectionCache();
            qInfo() << "Retrying host request with fresh paired TLS authentication";
            response = openConnectionToString(m_BaseUrlHttps,requestVerb,arguments,LAUNCH_TIMEOUT_MS);
            verifyResponseStatus(response);
        }
        return response;
    };

    QString response;
    try {
        response = requestStart(verb);
    }
    catch (const GfeHttpResponseException& error) {
        // The host may quit an app just after the previous client disconnects,
        // leaving the polled currentGameId stale for an immediate reconnect.
        if (!allowResumeLaunchFallback || verb != "resume" || error.getStatusCode() != 503 ||
            QString::fromUtf8(error.getStatusMessage()) != QStringLiteral("No running app to resume")) {
            throw;
        }

        const QString serverInfo = getAuthenticatedServerInfo(NVLL_ERROR);
        // An absent/unknown state is not evidence that launching is safe. Never
        // replace a newly active app or blindly turn other resume errors into launch.
        if (!getXmlString(serverInfo, "state").endsWith("_SERVER_FREE")) {
            throw;
        }

        qInfo() << "Host app ended before resume; launching requested app after fresh idle state";
        // Exactly one retry, preserving app/options and the same cancellation
        // token. openConnection checks a latched cancellation before sending.
        response = requestStart("launch");
    }

    rtspSessionUrl = getXmlString(response, "sessionUrl0");
    if (hostWarning) *hostWarning = hostDisplayWarning(response);
}

void
NvHTTP::quitApp()
{
    QString response =
            openConnectionToString(m_BaseUrlHttps,
                                   "cancel",
                                   nullptr,
                                   QUIT_TIMEOUT_MS);

    // Throws if the request failed
    verifyResponseStatus(response);

    // Newer GFE versions will just return success even if quitting fails
    // if we're not the original requester.
    if (getCurrentGame(getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR)) != 0) {
        // Generate a synthetic GfeResponseException letting the caller know
        // that they can't kill someone else's stream.
        throw GfeHttpResponseException(599, "");
    }
}

QVector<NvDisplayMode>
NvHTTP::getDisplayModeList(QString serverInfo)
{
    QXmlStreamReader xmlReader(serverInfo);
    QVector<NvDisplayMode> modes;

    while (!xmlReader.atEnd()) {
        while (xmlReader.readNextStartElement()) {
            auto name = xmlReader.name();
            if (XML_NAME_EQUALS(name, "DisplayMode")) {
                modes.append(NvDisplayMode());
            }
            else if (!modes.isEmpty()) {
                if (XML_NAME_EQUALS(name, "Width")) {
                    modes.last().width = xmlReader.readElementText().toInt();
                }
                else if (XML_NAME_EQUALS(name, "Height")) {
                    modes.last().height = xmlReader.readElementText().toInt();
                }
                else if (XML_NAME_EQUALS(name, "RefreshRate")) {
                    modes.last().refreshRate = xmlReader.readElementText().toInt();
                }
            }
        }
    }

    return modes;
}

QVector<NvApp>
NvHTTP::getAppList()
{
    QString appxml = openConnectionToString(m_BaseUrlHttps,
                                            "applist",
                                            nullptr,
                                            REQUEST_TIMEOUT_MS,
                                            NvLogLevel::NVLL_ERROR);
    verifyResponseStatus(appxml);

    QXmlStreamReader xmlReader(appxml);
    QVector<NvApp> apps;
    // Read one token at a time. Older Qt versions can read past EndDocument
    // inside readNextStartElement() and report a false premature-end error.
    while (!xmlReader.atEnd()) {
        if (xmlReader.readNext() != QXmlStreamReader::StartElement) {
            continue;
        }
        auto name = xmlReader.name();
        if (XML_NAME_EQUALS(name, "App")) {
            // We must have a valid app before advancing to the next one
            if (!apps.isEmpty() && !apps.last().isInitialized()) {
                qWarning() << "Invalid applist XML";
                throw GfeHttpResponseException(502, "Invalid application catalogue");
            }
            apps.append(NvApp());
        }
        else if (!apps.isEmpty()) {
            if (XML_NAME_EQUALS(name, "AppTitle")) {
                // If an app has no name, Sunshine may send us <AppTitle/>,
                // which readElementText() returns as a null QString.
                // We want to treat this as an empty QString instead, so we
                // will explicitly convert it. An empty string will satisfy
                // NvApp's isInitialized() check.
                QString name = xmlReader.readElementText();
                if (name.isNull()) {
                    name = "";
                }
                apps.last().name = name;
            }
            else if (XML_NAME_EQUALS(name, "UUID")) {
                apps.last().uuid = xmlReader.readElementText();
            }
            else if (XML_NAME_EQUALS(name, "ID")) {
                apps.last().id = xmlReader.readElementText().toInt();
            }
            else if (XML_NAME_EQUALS(name, "IsHdrSupported")) {
                apps.last().hdrSupported = xmlReader.readElementText() == "1";
            }
            else if (XML_NAME_EQUALS(name, "IsAppCollectorGame")) {
                apps.last().isAppCollectorGame = xmlReader.readElementText() == "1";
            }
        }
    }

    if (xmlReader.hasError() || (!apps.isEmpty() && !apps.last().isInitialized()))
        throw GfeHttpResponseException(502, "Malformed application catalogue");
    return apps;
}

void
NvHTTP::verifyResponseStatus(QString xml)
{
    QXmlStreamReader xmlReader(xml);

    while (xmlReader.readNextStartElement())
    {
        if (XML_NAME_EQUALS(xmlReader.name(), "root"))
        {
            // Status code can be 0xFFFFFFFF in some rare cases on GFE 3.20.3, and
            // QString::toInt() will fail in that case, so use QString::toUInt()
            // and cast the result to an int instead.
            int statusCode = (int)xmlReader.attributes().value("status_code").toUInt();
            if (statusCode == 200)
            {
                // Successful
                return;
            }
            else
            {
                QString statusMessage = xmlReader.attributes().value("status_message").toString();
                if (statusCode != 401) {
                    // 401 is expected for unpaired PCs when we fetch serverinfo over HTTPS
                    qWarning() << "Host request failed with status" << statusCode;
                }
                if (statusCode == -1 && statusMessage == "Invalid") {
                    // Special case handling an audio capture error which GFE doesn't
                    // provide any useful status message for.
                    statusCode = 418;
                    statusMessage = tr("Missing audio capture device. Check that an audio capture device is available on the host.");
                }
                throw GfeHttpResponseException(statusCode, statusMessage);
            }
        }
    }

    throw GfeHttpResponseException(-1, "Malformed XML (missing root element)");
}

QImage
NvHTTP::getBoxArt(int appId)
{
    QNetworkReply* reply = openConnection(m_BaseUrlHttps,
                                          "appasset",
                                          "appid="+QString::number(appId)+
                                          "&AssetType=2&AssetIdx=0",
                                          REQUEST_TIMEOUT_MS,
                                          NvLogLevel::NVLL_VERBOSE);
    QImage image = QImageReader(reply).read();
    delete reply;

    return image;
}

QByteArray
NvHTTP::getXmlStringFromHex(QString xml,
                            QString tagName)
{
    return QByteArray::fromHex(getXmlString(xml, tagName).toUtf8());
}

QString
NvHTTP::getXmlString(QString xml,
                     QString tagName)
{
    QXmlStreamReader xmlReader(xml);

    while (!xmlReader.atEnd())
    {
        if (xmlReader.readNext() != QXmlStreamReader::StartElement)
        {
            continue;
        }

        if (xmlReader.name() == tagName)
        {
            return xmlReader.readElementText();
        }
    }

    return QString();
}

void NvHTTP::handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    bool ignoreErrors = true;

    if (m_ServerCert.isNull()) {
        // We should never make an HTTPS request without a cert
        Q_ASSERT(!m_ServerCert.isNull());
        return;
    }

    for (const QSslError& error : errors) {
        if (m_ServerCert != error.certificate()) {
            ignoreErrors = false;
            break;
        }
    }

    if (ignoreErrors) {
        reply->ignoreSslErrors(errors);
    }
}

QString
NvHTTP::openConnectionToString(QUrl baseUrl,
                               QString command,
                               QString arguments,
                               int timeoutMs,
                               NvLogLevel logLevel)
{
    QNetworkReply* reply = openConnection(baseUrl, command, arguments, timeoutMs, logLevel);
    QString ret;

    QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    ret = stream.readAll();
    delete reply;

    return ret;
}

QNetworkReply*
NvHTTP::openConnection(QUrl baseUrl,
                       QString command,
                       QString arguments,
                       int timeoutMs,
                       NvLogLevel logLevel)
{
    if (m_CancelToken && m_CancelToken->isCancelled()) {
        throw QtNetworkReplyException(QNetworkReply::OperationCanceledError, "Request cancelled");
    }

    // Port must be set
    Q_ASSERT(baseUrl.port(0) != 0);

    // Build a URL for the request
    QUrl url(baseUrl);
    url.setPath("/" + command);

    // Use a placeholder UID for GFE allow them to quit games for each other.
    url.setQuery("uniqueid=" + (m_UseTrueUid ? IdentityManager::get()->getUniqueId() : "0123456789ABCDEF") +
                 "&uuid=" + QUuid::createUuid().toRfc4122().toHex() +
                 ((arguments != nullptr) ? ("&" + arguments) : ""));

    QNetworkRequest request(url);
    // Hosts may attach paired identity to the TLS handshake only. Close
    // explicitly: idle-cache expiry can race the next resource request.
    request.setRawHeader("Connection", "close");

    // Add our client certificate
    request.setSslConfiguration(IdentityManager::get()->getSslConfig());

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Disable HTTP/2 (GFE 3.22 doesn't like it) and Qt 6 enables it by default
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif

    // Use fine-grained idle timeouts to avoid calling QNetworkAccessManager::clearAccessCache(),
    // which tears down the NAM's global thread each time. We must not keep persistent connections
    // or GFE will puke.
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, -1);
#elif QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
#endif

    auto sslErrorsConnection = connect(m_Nam, &QNetworkAccessManager::sslErrors, this, &NvHTTP::handleSslErrors);
    QNetworkReply* reply = m_Nam->get(request);

    // Cap buffering while the nested event loop runs, not after readAll().
    constexpr qint64 kMaxResponseBytes = 8 * 1024 * 1024;
    BoundedReply responseLimit(reply, kMaxResponseBytes);

    if (m_CancelToken) m_CancelToken->arm(reply);

    // Run the request with a timeout if requested
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    if (timeoutMs) {
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    }
    if (logLevel >= NvLogLevel::NVLL_VERBOSE) {
        qInfo() << "Executing request:" << requestLocation(url);
    }
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Abort the request if it timed out
    if (!reply->isFinished())
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << "Aborting timed out request for" << requestLocation(url);
        }
        reply->abort();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    // If we couldn't use fine-grained connection idle timeouts, kill them all now
    m_Nam->clearAccessCache();
#endif
    disconnect(sslErrorsConnection);
    if (m_CancelToken) m_CancelToken->disarm(reply);

    if (responseLimit.exceeded()) {
        delete reply;
        throw QtNetworkReplyException(QNetworkReply::ProtocolFailure,
                                      "Host response exceeded size limit");
    }

    // Handle error
    if (reply->error() != QNetworkReply::NoError)
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << command << "request failed with error:" << reply->error();
        }

        if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            // This will trigger falling back to HTTP for the serverinfo query
            // then pairing again to get the updated certificate.
            GfeHttpResponseException exception(401, "Server certificate mismatch");
            delete reply;
            throw exception;
        }
        else if (reply->error() == QNetworkReply::OperationCanceledError) {
            QtNetworkReplyException exception(QNetworkReply::TimeoutError, "Request timed out");
            delete reply;
            throw exception;
        }
        else {
            QtNetworkReplyException exception(reply->error(), networkErrorDetail(*reply));
            delete reply;
            throw exception;
        }
    }

    return reply;
}

QString
NvHTTP::openConnectionPost(QUrl baseUrl,
                           QString command,
                           QString arguments,
                           QByteArray body,
                           QString contentType,
                           int timeoutMs,
                           NvLogLevel logLevel)
{
    if (m_CancelToken && m_CancelToken->isCancelled()) {
        throw QtNetworkReplyException(QNetworkReply::OperationCanceledError, "Request cancelled");
    }
    // Port must be set
    Q_ASSERT(baseUrl.port(0) != 0);

    // Build a URL for the request. Identical scheme to openConnection() above.
    QUrl url(baseUrl);
    url.setPath("/" + command);

    // Use a placeholder UID for GFE allow them to quit games for each other.
    url.setQuery("uniqueid=" + (m_UseTrueUid ? IdentityManager::get()->getUniqueId() : "0123456789ABCDEF") +
                 "&uuid=" + QUuid::createUuid().toRfc4122().toHex() +
                 ((arguments != nullptr) ? ("&" + arguments) : ""));

    QNetworkRequest request(url);
    // Hosts may attach paired identity to the TLS handshake only. Close
    // explicitly: idle-cache expiry can race the next resource request.
    request.setRawHeader("Connection", "close");

    // Add our client certificate (identical to openConnection())
    request.setSslConfiguration(IdentityManager::get()->getSslConfig());

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Disable HTTP/2 (GFE 3.22 doesn't like it) and Qt 6 enables it by default
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    // Use fine-grained idle timeouts to avoid calling QNetworkAccessManager::clearAccessCache(),
    // which tears down the NAM's global thread each time. We must not keep persistent connections
    // or GFE will puke.
    request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
#endif

    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }

    // Same server-pin machinery as openConnection(): handleSslErrors() only ignores
    // errors when every offending certificate matches our pinned m_ServerCert.
    auto sslErrorsConnection = connect(m_Nam, &QNetworkAccessManager::sslErrors, this, &NvHTTP::handleSslErrors);
    QNetworkReply* reply = m_Nam->post(request, body);

    constexpr qint64 kMaxResponseBytes = 8 * 1024 * 1024;
    BoundedReply responseLimit(reply, kMaxResponseBytes);
    if (m_CancelToken) m_CancelToken->arm(reply);

    // Run the request with a timeout if requested
    QEventLoop loop;
    bool deadlineExpired = false;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    if (timeoutMs) {
        QTimer::singleShot(timeoutMs, &loop, [&] {
            deadlineExpired = true;
            loop.quit();
        });
    }
    if (logLevel >= NvLogLevel::NVLL_VERBOSE) {
        qInfo() << "Executing POST request:" << requestLocation(url);
    }
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Abort the request if it timed out
    if (!reply->isFinished())
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << "Aborting timed out POST request for" << requestLocation(url);
        }
        reply->abort();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    // If we couldn't use fine-grained connection idle timeouts, kill them all now
    m_Nam->clearAccessCache();
#endif
    disconnect(sslErrorsConnection);
    if (m_CancelToken) m_CancelToken->disarm(reply);

    if (responseLimit.exceeded()) {
        delete reply;
        throw QtNetworkReplyException(QNetworkReply::ProtocolFailure,
                                      "Host response exceeded size limit");
    }

    // Handle error
    if (reply->error() != QNetworkReply::NoError)
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << command << "POST request failed with error:" << reply->error();
        }

        const auto transportError = reply->error();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const bool httpError = (transportError >= QNetworkReply::ContentAccessDenied &&
                                transportError <= QNetworkReply::UnknownContentError) ||
                               (transportError >= QNetworkReply::InternalServerError &&
                                transportError <= QNetworkReply::UnknownServerError) ||
                               transportError == QNetworkReply::ProtocolInvalidOperationError;
        if (status.isValid() && status.toInt() >= 400 && httpError) {
            // Preserve endpoint status for disabled/unsupported versus retryable
            // errors. Qt can mask a truncated error body with its first error.
            const auto response = reply->readAll();
            bool haveLength = false;
            const auto length = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&haveLength);
            if (haveLength && length >= 0 && response.size() != length) {
                delete reply;
                throw QtNetworkReplyException(QNetworkReply::RemoteHostClosedError, "Incomplete HTTP error response");
            }
            GfeHttpResponseException exception(status.toInt(),
                reply->rawHeader("Content-Type").startsWith("text/plain") && !response.isEmpty()
                    ? QString::fromUtf8(response.left(512)) : networkErrorDetail(*reply));
            delete reply;
            throw exception;
        }
        if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            GfeHttpResponseException exception(401, "Server certificate mismatch");
            delete reply;
            throw exception;
        }
        else if (reply->error() == QNetworkReply::OperationCanceledError) {
            QtNetworkReplyException exception(deadlineExpired ? QNetworkReply::TimeoutError : QNetworkReply::OperationCanceledError,
                                              deadlineExpired ? "Request timed out" : "Request interrupted");
            delete reply;
            throw exception;
        }
        else {
            QtNetworkReplyException exception(reply->error(), networkErrorDetail(*reply));
            delete reply;
            throw exception;
        }
    }

    QString ret;
    QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    ret = stream.readAll();
    delete reply;

    return ret;
}

void NvHTTP::CancelToken::arm(QNetworkReply *reply)
{
    QMutexLocker locker(&m_Mutex);
    m_Reply = reply;
    if (m_Cancelled || m_InterruptPending) {
        m_InterruptPending = false;
        QMetaObject::invokeMethod(reply, "abort", Qt::QueuedConnection);
    }
}

void NvHTTP::CancelToken::disarm(QNetworkReply *reply)
{
    QMutexLocker locker(&m_Mutex);
    if (m_Reply == reply) {
        m_Reply = nullptr;
    }
}

void NvHTTP::CancelToken::abortIfArmed()
{
    QMutexLocker locker(&m_Mutex);
    if (m_Reply != nullptr) {
        // Queued, not direct: m_Reply lives on its own thread (the dedicated
        // clipboard thread), which may currently be blocked inside the nested
        // QEventLoop below. Qt's queued invokeMethod resolves the metaobject here
        // (synchronously, on THIS thread) but the actual abort() call is posted and
        // executes on the reply's thread - safe because we're still holding
        // m_Mutex, and disarm() (called from the reply's own thread before it is
        // deleted) must take the same lock before it can proceed.
        QMetaObject::invokeMethod(m_Reply, "abort", Qt::QueuedConnection);
    }
}

void NvHTTP::CancelToken::interrupt()
{
    QMutexLocker locker(&m_Mutex);
    m_InterruptPending = m_Reply == nullptr;
    if (m_Reply != nullptr) {
        QMetaObject::invokeMethod(m_Reply, "abort", Qt::QueuedConnection);
    }
}

void NvHTTP::CancelToken::cancel()
{
    QMutexLocker locker(&m_Mutex);
    m_Cancelled = true;
    if (m_Reply != nullptr) {
        QMetaObject::invokeMethod(m_Reply, "abort", Qt::QueuedConnection);
    }
}

bool NvHTTP::CancelToken::isCancelled() const
{
    QMutexLocker locker(&m_Mutex);
    return m_Cancelled;
}

NvHTTP::RawResponse
NvHTTP::openConnectionRaw(QUrl baseUrl,
                          QString command,
                          QString arguments,
                          int timeoutMs,
                          const QByteArray& headerNameToCapture,
                          NvLogLevel logLevel,
                          CancelToken* cancelToken,
                          qint64 maxResponseBytes)
{
    if (cancelToken && cancelToken->isCancelled()) {
        throw QtNetworkReplyException(QNetworkReply::OperationCanceledError, "Request cancelled");
    }

    // Port must be set
    Q_ASSERT(baseUrl.port(0) != 0);

    // Build a URL for the request. Identical scheme to openConnection() above.
    QUrl url(baseUrl);
    url.setPath("/" + command);

    // Use a placeholder UID for GFE allow them to quit games for each other.
    url.setQuery("uniqueid=" + (m_UseTrueUid ? IdentityManager::get()->getUniqueId() : "0123456789ABCDEF") +
                 "&uuid=" + QUuid::createUuid().toRfc4122().toHex() +
                 ((arguments != nullptr) ? ("&" + arguments) : ""));

    QNetworkRequest request(url);
    // Hosts may attach paired identity to the TLS handshake only. Close
    // explicitly: idle-cache expiry can race the next resource request.
    request.setRawHeader("Connection", "close");
    // Keep length validation in the same representation as readAll(). These
    // small clipboard/cursor endpoints do not need compressed transfer bodies.
    request.setRawHeader("Accept-Encoding", "identity");


    // Add our client certificate (identical to openConnection())
    request.setSslConfiguration(IdentityManager::get()->getSslConfig());

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Disable HTTP/2 (GFE 3.22 doesn't like it) and Qt 6 enables it by default
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    // Use fine-grained idle timeouts to avoid calling QNetworkAccessManager::clearAccessCache(),
    // which tears down the NAM's global thread each time. We must not keep persistent connections
    // or GFE will puke.
    request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
#endif

    auto sslErrorsConnection = connect(m_Nam, &QNetworkAccessManager::sslErrors, this, &NvHTTP::handleSslErrors);
    QNetworkReply* reply = m_Nam->get(request);

    Q_ASSERT(maxResponseBytes > 0);
    BoundedReply responseLimit(reply, maxResponseBytes);

    if (cancelToken != nullptr) {
        cancelToken->arm(reply);
    }

    // Run the request with a timeout if requested
    QEventLoop loop;
    bool deadlineExpired = false;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    if (timeoutMs) {
        QTimer::singleShot(timeoutMs, &loop, [&] {
            deadlineExpired = true;
            loop.quit();
        });
    }
    if (logLevel >= NvLogLevel::NVLL_VERBOSE) {
        qInfo() << "Executing long-poll request:" << requestLocation(url);
    }
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Abort the request if it timed out (or if a CancelToken abort raced us here and
    // hasn't been delivered yet - reply->abort() is safe to call more than once).
    if (!reply->isFinished())
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << "Aborting timed out request for" << requestLocation(url);
        }
        reply->abort();
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    // If we couldn't use fine-grained connection idle timeouts, kill them all now
    m_Nam->clearAccessCache();
#endif
    disconnect(sslErrorsConnection);

    // Disarm before we touch reply again (delete, on any exit path below) - this
    // must happen before deletion so a concurrent abortIfArmed() on another thread
    // can never be handed a dangling pointer (see CancelToken's doc comment).
    if (cancelToken != nullptr) {
        cancelToken->disarm(reply);
    }

    if (responseLimit.exceeded()) {
        disconnect(sslErrorsConnection);
        delete reply;
        throw QtNetworkReplyException(QNetworkReply::ProtocolFailure,
                                      "Host response exceeded size limit");
    }

    // Qt also retains HTTP status for a truncated, timed out or cancelled body.
    // Only content/server status errors describe complete HTTP responses.
    QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const auto error = reply->error();
    const bool httpError = (error >= QNetworkReply::ContentAccessDenied &&
                            error <= QNetworkReply::UnknownContentError) ||
                           (error >= QNetworkReply::InternalServerError &&
                            error <= QNetworkReply::UnknownServerError);
    if (statusAttr.isValid() && (error == QNetworkReply::NoError || httpError)) {
        RawResponse result;
        result.httpStatusCode = statusAttr.toInt();
        result.contentType = reply->rawHeader("Content-Type");
        const bool bodyless = result.httpStatusCode == 204 || result.httpStatusCode == 304;
        const auto encoding = reply->rawHeader("Content-Encoding").trimmed().toLower();
        // Qt can report success on EOF while still reading an incomplete
        // chunked body. Its public API exposes no chunk-terminator receipt.
        // Keep this raw endpoint seam to verifiable fixed-length framing.
        if (!bodyless && (!reply->rawHeader("Transfer-Encoding").trimmed().isEmpty() ||
                          (!encoding.isEmpty() && encoding != "identity"))) {
            delete reply;
            throw QtNetworkReplyException(QNetworkReply::ProtocolFailure,
                                          "Unsupported HTTP response framing or encoding");
        }
        result.body = reply->readAll();
        // Qt retains only its first error: an HTTP content error can mask a
        // subsequent early disconnect. Check known framing independently of it.
        // 204/304 never carry a body; 304 may declare the cached entity's size.
        bool haveLength = false;
        const qint64 declaredLength = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&haveLength);
        if (!bodyless && (!haveLength || declaredLength < 0 || result.body.size() != declaredLength)) {
            delete reply;
            throw QtNetworkReplyException(QNetworkReply::RemoteHostClosedError,
                                          "Incomplete HTTP response body");
        }
        if (!headerNameToCapture.isEmpty() && reply->hasRawHeader(headerNameToCapture)) {
            result.headerValue = reply->rawHeader(headerNameToCapture);
        }
        delete reply;
        return result;
    }

    // Incomplete response or transport-level failure. Mirror openConnection()'s
    // exception behavior exactly (including the special-cased SSL-mismatch/timeout
    // messages) so callers that already understand those two exception types don't
    // need a third code path.
    if (logLevel >= NvLogLevel::NVLL_ERROR) {
        qWarning() << command << "request failed with error:" << reply->error();
    }

    if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
        GfeHttpResponseException exception(401, "Server certificate mismatch");
        delete reply;
        throw exception;
    }
    else if (reply->error() == QNetworkReply::OperationCanceledError) {
        QtNetworkReplyException exception(deadlineExpired ? QNetworkReply::TimeoutError : QNetworkReply::OperationCanceledError,
                                          deadlineExpired ? "Request timed out" : "Request interrupted");
        delete reply;
        throw exception;
    }
    else {
        QtNetworkReplyException exception(reply->error(), networkErrorDetail(*reply));
        delete reply;
        throw exception;
    }
}

void NvHTTP::disconnectRemoteMonitor()
{
    const QString response = openConnectionToString(m_BaseUrlHttps, "launch",
        "appid=2147483502&appuuid=9a1c5a25-58fe-40e0-b9aa-7d3f00000002"
        "&rikey=00000000000000000000000000000000&rikeyid=0&localAudioPlayMode=0",
        LAUNCH_TIMEOUT_MS, NVLL_ERROR);
    try { verifyResponseStatus(response); }
    catch (const GfeHttpResponseException& error) {
        if (error.getStatusCode() != 410 ||
            QString::fromUtf8(error.getStatusMessage()) != QStringLiteral("Remote Monitor disconnected successfully.")) throw;
    }
    for (const auto& app : getAppList()) {
        if (QUuid(app.uuid) == QUuid(QStringLiteral("9a1c5a25-58fe-40e0-b9aa-7d3f00000002")))
            throw GfeHttpResponseException(503, "The host still reports a retained Remote Monitor");
    }
}

void NvHTTP::takeOverRetainedDesktop(const QString& hostUuid, const NvApp& expectedDesktop)
{
    // Called only after explicit user approval. Stock role controls do not have
    // an expected-target transaction, so never infer consent from a FREE state.
    struct Snapshot { QVector<NvApp> apps; NvApp target; NvApp running; NvApp control; };
    auto snapshot = [&]() {
        const auto info = getAuthenticatedServerInfo(NVLL_ERROR);
        if (getXmlString(info, "uniqueid") != hostUuid ||
            !getXmlString(info, "state").endsWith("_SERVER_FREE") || getCurrentGame(info) != 0 ||
            !HostCapabilities::parse(info).allows(HostCapabilities::Launch))
            throw GfeHttpResponseException(409, "The host session or permission changed. No further takeover request was sent.");
        Snapshot result;
        result.apps = getAppList();
        result.target = NvApp::desktopTarget(result.apps, false);
        result.running = NvApp::catalogueRole(result.apps, 7);
        result.control = NvApp::catalogueRole(result.apps, 4);
        if (!result.target.isInitialized() || QUuid(result.target.uuid).isNull() ||
            QUuid(result.target.uuid) != QUuid(expectedDesktop.uuid))
            throw GfeHttpResponseException(409, "The Desktop target changed. No further takeover request was sent.");
        if (result.running.isInitialized() && !result.control.isInitialized())
            throw GfeHttpResponseException(409, "The host does not offer a Desktop takeover control. End its retained stream from the original device.");
        if (result.running.isInitialized() && !NvApp::hasForeignDesktop(result.apps))
            throw GfeHttpResponseException(409, "The running application changed. No further takeover request was sent.");
        return result;
    };
    const auto before = snapshot();
    if (!before.running.isInitialized()) return; // It ended while the dialog was open.
    const QString prompt = QStringLiteral("This will close the active stream but leave Remote Monitor and Remote Input connected. Launch Terminate again within 60 seconds to confirm this was intentional.");
    const QString completed = QStringLiteral("Active stream terminated. Remote Monitor and Remote Input remain connected.");
    auto terminate = [&](const NvApp& control) {
        const QString response = openConnectionToString(m_BaseUrlHttps, "launch",
            "appid=" + QString::number(control.id) + "&appuuid=" + QString::fromLatin1(QUrl::toPercentEncoding(control.uuid)) +
            "&rikey=00000000000000000000000000000000&rikeyid=0&localAudioPlayMode=0",
            LAUNCH_TIMEOUT_MS, NVLL_ERROR);
        try { verifyResponseStatus(response); }
        catch (const GfeHttpResponseException& error) {
            const auto message = QString::fromUtf8(error.getStatusMessage());
            if (error.getStatusCode() == 410 && message == completed) return true;
            if (error.getStatusCode() == 410 && message == prompt) return false;
            throw;
        }
        throw GfeHttpResponseException(502, "The host did not confirm that its streaming application ended.");
    };
    if (!terminate(before.control)) {
        const auto confirmed = snapshot();
        if (!confirmed.running.isInitialized()) return;
        if (confirmed.running.id != before.running.id || confirmed.running.name != before.running.name ||
            confirmed.control.id != before.control.id || QUuid(confirmed.control.uuid) != QUuid(before.control.uuid))
            throw GfeHttpResponseException(409, "The retained session changed. Confirm the new session before taking it over.");
        if (!terminate(confirmed.control))
            throw GfeHttpResponseException(409, "The host requested a new confirmation. No further takeover request was sent.");
    }
    const auto after = snapshot();
    if (after.running.isInitialized())
        throw GfeHttpResponseException(409, "The host still reports a running session. No Desktop launch was attempted.");
}

void NvHTTP::verifyMainDesktopOwnership(const QString& hostUuid, const NvApp& expectedDesktop)
{
    const auto info = getAuthenticatedServerInfo(NVLL_ERROR);
    const int current = getCurrentGame(info);
    if (getXmlString(info, "uniqueid") != hostUuid ||
        !expectedDesktop.matchesRunningTarget(current, getXmlString(info, "currentgameuuid")))
        throw GfeHttpResponseException(409, "The host did not grant this device its own Desktop session. No shared display was opened.");
}
