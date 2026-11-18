#include <QtTest>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include "backend/nvhttp.h"

extern "C" const char* LiGetLaunchUrlQueryParameters(void) { return ""; }

class ScriptedReply final : public QNetworkReply {
public:
    ScriptedReply(const QNetworkRequest& request, QByteArray body, QObject* parent)
        : QNetworkReply(parent), m_Body(std::move(body)) {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this] { setFinished(true); emit readyRead(); emit finished(); });
    }
    void abort() override { setError(OperationCanceledError, "Cancelled"); setFinished(true); emit finished(); }
    qint64 bytesAvailable() const override { return m_Body.size() - m_Offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* output, qint64 maximum) override {
        const qint64 count = qMin(maximum, qint64(m_Body.size()) - m_Offset);
        if (!count) return -1;
        memcpy(output, m_Body.constData() + m_Offset, size_t(count)); m_Offset += count; return count;
    }
private:
    QByteArray m_Body;
    qint64 m_Offset = 0;
};

class ScriptedNetwork final : public QNetworkAccessManager {
public:
    QList<QByteArray> responses;
    QList<QUrl> requests;
    QList<QNetworkRequest> details;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        requests << request.url();
        details << request;
        return new ScriptedReply(request, responses.isEmpty() ? QByteArray("<root status_code=\"500\"/>") : responses.takeFirst(), this);
    }
};

class LaunchRequestTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_Settings;
    const QByteArray started = "<root status_code=\"200\"><sessionUrl0>rtsp://fixture/session</sessionUrl0></root>";
    const QByteArray gone = "<root status_code=\"503\" status_message=\"No running app to resume\"/>";
    const QByteArray pairedFree = "<root status_code=\"200\"><PairStatus>1</PairStatus><uniqueid>fixture-host</uniqueid><state>SUNSHINE_SERVER_FREE</state><currentgame>0</currentgame><Permission>67108864</Permission></root>";
    const QByteArray idleCatalogue = "<root status_code=\"200\"><App><AppTitle>Desktop</AppTitle><ID>101</ID><UUID>abcdefab-1234-4321-abcd-0123456789ab</UUID></App></root>";
    const QByteArray takeoverPrompt = "<root status_code=\"410\" status_message=\"This will close the active stream but leave Remote Monitor and Remote Input connected. Launch Terminate again within 60 seconds to confirm this was intentional.\"/>";
    const QByteArray takeoverDone = "<root status_code=\"410\" status_message=\"Active stream terminated. Remote Monitor and Remote Input remain connected.\"/>";
    QByteArray foreignCatalogue() const {
        QByteArray xml = idleCatalogue;
        xml.replace("</root>", "<App><AppTitle>     Desktop</AppTitle><ID>202</ID><UUID>9a1c5a25-58fe-40e0-b9aa-7d3f00000007</UUID></App><App><AppTitle>Terminate</AppTitle><ID>2147483514</ID><UUID>9a1c5a25-58fe-40e0-b9aa-7d3f00000004</UUID></App></root>");
        return xml;
    }
    NvApp desktopTarget() const {
        NvApp app; app.id=101; app.name="Desktop"; app.uuid="abcdefab-1234-4321-abcd-0123456789ab"; return app;
    }
    void request(NvHTTP& http, bool fallback, QString scale = QString(), int adaptive = -1, int virtualDisplay = -1) {
        STREAM_CONFIGURATION configuration {};
        configuration.width = 1920; configuration.height = 1080; configuration.fps = 60;
        configuration.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        QString url;
        http.startApp("resume", false, 42, &configuration, true, false, 0, false, false,
                      url, scale, adaptive, fallback, virtualDisplay);
        QCOMPARE(url, QString("rtsp://fixture/session"));
    }
private slots:
    void stockHostDisplayWarning_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::addColumn<bool>("warn");
        const QByteArray warning = "<clientWarning>virtual_display_disabled_by_client</clientWarning>";
        QTest::newRow("stock-launch") << QByteArray("<gamesession>1</gamesession>") + warning << true;
        QTest::newRow("stock-resume") << QByteArray("<resume>1</resume>") + warning << true;
        QTest::newRow("legacy-host") << QByteArray("<gamesession>1</gamesession>") << false;
        QTest::newRow("unknown-code") << QByteArray("<gamesession>1</gamesession><clientWarning>future_warning</clientWarning>") << false;
        QTest::newRow("not-started") << QByteArray("<gamesession>0</gamesession>") + warning << false;
        QTest::newRow("nested-code") << QByteArray("<gamesession>1</gamesession><extension>") + warning + "</extension>" << false;
        QTest::newRow("duplicate-code") << QByteArray("<gamesession>1</gamesession>") + warning + warning << false;
        QTest::newRow("namespaced-code") << QByteArray("<gamesession>1</gamesession><clientWarning xmlns=\"urn:other\">virtual_display_disabled_by_client</clientWarning>") << false;
        QTest::newRow("malformed-code") << QByteArray("<gamesession>1</gamesession><clientWarning>virtual_display_disabled_by_client</clientWarning><broken>") << false;
        QTest::newRow("oversized-response") << QByteArray("<gamesession>1</gamesession>") + warning + "<extension>" + QByteArray(17000,'x') + "</extension>" << false;
    }
    void stockHostDisplayWarning() {
        QFETCH(QByteArray, response);
        QFETCH(bool, warn);
        ScriptedNetwork network;
        network.responses << "<root status_code=\"200\"><sessionUrl0>rtsp://fixture/session</sessionUrl0>" + response + "</root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&network);
        STREAM_CONFIGURATION configuration {};
        configuration.width=1920; configuration.height=1080; configuration.fps=60;
        configuration.audioConfiguration=AUDIO_CONFIGURATION_STEREO;
        QString url, warning="stale warning";
        http.startApp("launch",false,42,&configuration,true,false,0,false,false,url,{},-1,false,-1,{}, {}, &warning);
        QCOMPARE(url,QString("rtsp://fixture/session"));
        QCOMPARE(!warning.isEmpty(),warn);
        if (warn) QVERIFY(warning.contains("existing display"));
        QCOMPARE(network.requests.size(),1); // Observation never triggers a retry or host mutation.
    }
    void approvedTakeoverUsesAdvertisedControlAndFreshConfirmation() {
        for (bool prompt : {false,true}) {
            ScriptedNetwork network;
            network.responses << pairedFree << foreignCatalogue() << (prompt ? takeoverPrompt : takeoverDone);
            if (prompt) network.responses << pairedFree << foreignCatalogue() << takeoverDone;
            network.responses << pairedFree << idleCatalogue;
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
            http.takeOverRetainedDesktop("fixture-host",desktopTarget());
            int controls=0;
            for (const auto& r : network.requests) {
                QVERIFY(r.path()!="/cancel");
                if (r.path()=="/launch") {
                    ++controls;
                    QCOMPARE(QUrlQuery(r).queryItemValue("appid"),QString("2147483514"));
                    QCOMPARE(QUrlQuery(r).queryItemValue("appuuid"),QString("9a1c5a25-58fe-40e0-b9aa-7d3f00000004"));
                }
            }
            QCOMPARE(controls,prompt?2:1);
            QVERIFY(network.responses.isEmpty());
        }
    }
    void takeoverDoesNothingWhenPriorDesktopAlreadyEnded() {
        ScriptedNetwork network; network.responses << pairedFree << idleCatalogue;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        http.takeOverRetainedDesktop("fixture-host",desktopTarget());
        QCOMPARE(network.requests.size(),2);
        for (const auto& r : network.requests) QVERIFY(r.path()!="/launch");
    }
    void takeoverRefusesChangedGameIdentityPermissionAndCancellation() {
        for (int scenario=0;scenario<5;++scenario) {
            ScriptedNetwork network;
            QByteArray info=pairedFree, apps=foreignCatalogue();
            if (scenario==0) apps.replace("     Desktop","     Running game");
            if (scenario==1) info.replace("fixture-host","different-host");
            if (scenario==2) info.replace("67108864","0");
            if (scenario==3) info.replace("SERVER_FREE","SERVER_BUSY");
            network.responses << info << apps;
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
            NvHTTP::CancelToken token;
            if (scenario==4) {token.cancel();http.setCancelToken(&token);}
            bool failed=false;
            try {http.takeOverRetainedDesktop("fixture-host",desktopTarget());} catch (...) {failed=true;}
            QVERIFY(failed);
            for (const auto& r : network.requests) QVERIFY(r.path()!="/launch" && r.path()!="/cancel");
        }
    }
    void takeoverStopsIfRetainedSessionChangesAfterPrompt() {
        ScriptedNetwork network;
        auto changed=foreignCatalogue();changed.replace("<ID>202</ID>","<ID>203</ID>");
        network.responses << pairedFree << foreignCatalogue() << takeoverPrompt << pairedFree << changed;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(http.takeOverRetainedDesktop("fixture-host",desktopTarget()),GfeHttpResponseException);
        QCOMPARE(network.requests.size(),5);
    }
    void takeoverNeverSendsAThirdConfirmationOrRelaunchesWithoutRelease() {
        for (bool repeatedPrompt : {false,true}) {
            ScriptedNetwork network;network.responses << pairedFree << foreignCatalogue();
            if (repeatedPrompt) network.responses << takeoverPrompt << pairedFree << foreignCatalogue() << takeoverPrompt;
            else network.responses << takeoverDone << pairedFree << foreignCatalogue();
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
            QVERIFY_EXCEPTION_THROWN(http.takeOverRetainedDesktop("fixture-host",desktopTarget()),GfeHttpResponseException);
            int controls=0;for(const auto& r:network.requests)if(r.path()=="/launch")++controls;
            QCOMPARE(controls,repeatedPrompt?2:1);
        }
    }
    void mainDesktopOwnershipMustBeBusyAndMatchHostAndAppUuid() {
        for(int scenario=0;scenario<6;++scenario) {
            ScriptedNetwork network;
            QByteArray info="<root status_code=\"200\"><PairStatus>1</PairStatus><uniqueid>fixture-host</uniqueid><state>SUNSHINE_SERVER_BUSY</state><currentgame>101</currentgame><currentgameuuid>abcdefab-1234-4321-abcd-0123456789ab</currentgameuuid></root>";
            if(scenario==1)info.replace("SERVER_BUSY","SERVER_FREE");
            if(scenario==2)info.replace("fixture-host","different-host");
            if(scenario==3)info.replace("abcdefab-1234","abcdefab-9999");
            if(scenario==4)info.replace("<currentgame>101","<currentgame>999"); // Canonical UUID survives an ID alias change.
            if(scenario==5)info.replace("<PairStatus>1","<PairStatus>0");
            network.responses << info;
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
            bool failed=false;try {http.verifyMainDesktopOwnership("fixture-host",desktopTarget());}catch(...){failed=true;}
            QCOMPARE(failed,scenario!=0 && scenario!=4);
            QCOMPARE(network.requests.size(),1);
        }
    }

    void pairedResourcesExplicitlyCloseTheirHttpConnection() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"200\"><PairStatus>1</PairStatus></root>"
                          << "<root status_code=\"200\"><App><AppTitle>Desktop</AppTitle><ID>101</ID></App></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        http.getAuthenticatedServerInfo(NvHTTP::NVLL_NONE);
        http.getAppList();
        QCOMPARE(network.details.size(),2);
        for (const auto& r : network.details) QCOMPARE(r.rawHeader("Connection"),QByteArray("close"));
    }

    void privateDesktopLifecycleLeavesRetainedGameAlone() {
        const QByteArray shared = "<root status_code=\"200\">"
            "<App><AppTitle>     Desktop</AppTitle><ID>202</ID><UUID>9a1c5a25-58fe-40e0-b9aa-7d3f00000007</UUID></App>"
            "<App><AppTitle>Desktop</AppTitle><ID>101</ID><UUID>abcdefab-1234-4321-abcd-0123456789ab</UUID></App>"
            "<App><AppTitle>    Remote Monitor</AppTitle><ID>2147483515</ID><UUID>9a1c5a25-58fe-40e0-b9aa-7d3f00000005</UUID></App></root>";
        ScriptedNetwork network;
        network.responses << shared << started
                          << "<root status_code=\"410\" status_message=\"Remote Monitor disconnected successfully.\"/>" << shared;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        const NvApp target = NvApp::desktopTarget(http.getAppList(), true);
        QVERIFY(target.isRemoteMonitor());
        STREAM_CONFIGURATION c {}; c.width=2880; c.height=1800; c.fps=60;
        QString url;
        http.startApp("launch",false,target.id,&c,true,false,0,false,false,url,{},-1,false,1,{},target.uuid);
        http.disconnectRemoteMonitor();
        QCOMPARE(network.requests.size(),4);
        const QUrlQuery launch(network.requests[1]);
        QCOMPARE(launch.queryItemValue("appid"),QString("2147483515"));
        QCOMPARE(launch.queryItemValue("mode"),QString("2880x1800x60"));
        QCOMPARE(launch.queryItemValue("virtualDisplay"),QString("1"));
        QCOMPARE(QUrlQuery(network.requests[2]).queryItemValue("appid"),QString("2147483502"));
        for (const auto& r : network.requests) QVERIFY(r.path() != "/cancel" && r.path() != "/resume");
        QVERIFY(network.responses.isEmpty());
    }
    void monitorCleanupCannotTreatTruncatedCatalogueAsReleased() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"410\" status_message=\"Remote Monitor disconnected successfully.\"/>"
                          << "<root status_code=\"200\"><App><AppTitle>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(http.disconnectRemoteMonitor(), GfeHttpResponseException);
    }

    void configuredAppUuidTravelsWithLaunchWithoutChangingGfe() {
        for (bool gfe : {false, true}) {
            ScriptedNetwork network; network.responses << started;
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&network);
            STREAM_CONFIGURATION c {}; c.width=2880; c.height=1800; c.fps=60;
            QString url;
            http.startApp("launch",gfe,101,&c,true,false,0,false,false,url,{},-1,false,-1,{},
                          "{ABCDEFAB-1234-4321-ABCD-0123456789AB}");
            const QUrlQuery query(network.requests[0]);
            QCOMPARE(query.hasQueryItem("appuuid"), !gfe);
            if (!gfe) QCOMPARE(query.queryItemValue("appuuid"),QString("{ABCDEFAB-1234-4321-ABCD-0123456789AB}"));
        }
    }
    void monitorDisconnectNeverCancelsGlobalGameAndVerifiesRelease() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"410\" status_message=\"Remote Monitor disconnected successfully.\"/>"
                          << "<root status_code=\"200\"><App><AppTitle>Desktop</AppTitle><ID>101</ID></App></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        http.disconnectRemoteMonitor();
        QCOMPARE(network.requests.size(),2);
        QCOMPARE(network.requests[0].path(),QString("/launch"));
        QCOMPARE(QUrlQuery(network.requests[0]).queryItemValue("appuuid"),QString("9a1c5a25-58fe-40e0-b9aa-7d3f00000002"));
        QCOMPARE(network.requests[1].path(),QString("/applist"));
    }
    void monitorDisconnectRejectsUnrelatedErrorsAndRetainedOwnership() {
        for (bool retained : {false,true}) {
            ScriptedNetwork network;
            network.responses << (retained ? QByteArray("<root status_code=\"410\" status_message=\"Remote Monitor disconnected successfully.\"/>")
                                          : QByteArray("<root status_code=\"410\" status_message=\"Confirm termination\"/>"));
            if (retained) network.responses << "<root status_code=\"200\"><App><AppTitle>Disconnect Monitor</AppTitle><ID>2147483502</ID><UUID>9a1c5a25-58fe-40e0-b9aa-7d3f00000002</UUID></App></root>";
            NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
            QVERIFY_EXCEPTION_THROWN(http.disconnectRemoteMonitor(),GfeHttpResponseException);
            for (const auto& request : network.requests) QVERIFY(request.path() != "/cancel");
        }
    }

    void resourceChecksNeverAcceptAnUnpairedGlobalIdleView() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"200\"><PairStatus>0</PairStatus><currentgame>0</currentgame></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(http.getAuthenticatedServerInfo(NvHTTP::NVLL_NONE),GfeHttpResponseException);
        QCOMPARE(network.requests.size(),1);
        QCOMPARE(network.requests[0].scheme(),QString("https"));
    }
    void authenticatedChecksDoNotFallBackToPublicDiscovery() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"401\"/>"
                          << "<root status_code=\"200\"><PairStatus>0</PairStatus><currentgame>0</currentgame></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(http.getAuthenticatedServerInfo(NvHTTP::NVLL_NONE),GfeHttpResponseException);
        QCOMPARE(network.requests.size(),1);
    }
    void pairedRequestsKeepClientAuthenticationOnNewConnections() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"200\"><PairStatus>1</PairStatus><state>SUNSHINE_SERVER_BUSY</state><currentgame>42</currentgame></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QCOMPARE(NvHTTP::getCurrentGame(http.getAuthenticatedServerInfo(NvHTTP::NVLL_NONE)),42);
        const auto ssl=network.details[0].sslConfiguration();
        QVERIFY(ssl.testSslOption(QSsl::SslOptionDisableSessionSharing));
        QVERIFY(ssl.testSslOption(QSsl::SslOptionDisableSessionTickets));
        QVERIFY(!ssl.localCertificate().isNull());
    }
    void cancelCannotBeConfirmedByAnUnpairedIdleView() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"200\"><cancel>1</cancel></root>"
                          << "<root status_code=\"200\"><PairStatus>0</PairStatus><state>SUNSHINE_SERVER_FREE</state><currentgame>0</currentgame></root>";
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(http.quitApp(),GfeHttpResponseException);
        QCOMPARE(network.requests.size(),2);
        QCOMPARE(network.requests[0].path(),QString("/cancel"));
        QCOMPARE(network.requests[1].scheme(),QString("https"));
    }
    void unpairedIdleCannotAuthorizeResumeFallback() {
        ScriptedNetwork network;
        network.responses << gone << "<root status_code=\"200\"><PairStatus>0</PairStatus><state>SUNSHINE_SERVER_FREE</state></root>" << started;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),true,&network);
        QVERIFY_EXCEPTION_THROWN(request(http,true),GfeHttpResponseException);
        QCOMPARE(network.requests.size(),2);
        QCOMPARE(network.requests[1].path(),QString("/serverinfo"));
    }
    void desktopGeometryCanStayFixedWhileTheEncodedPictureShrinks() {
        ScriptedNetwork network; network.responses << started;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&network);
        STREAM_CONFIGURATION configuration {};
        configuration.width = 2160; configuration.height = 1350; configuration.fps = 60;
        configuration.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        QString url;
        http.startApp("resume",false,42,&configuration,true,false,0,false,false,url,
                      {},-1,false,-1,QSize(2880,1800));
        QCOMPARE(QUrlQuery(network.requests[0]).queryItemValue("mode"),QString("2880x1800x60"));
        QCOMPARE(configuration.width,2160);
        QCOMPARE(configuration.height,1350);
    }
    void separateDesktopGeometryDoesNotChangeGfeLaunchSemantics() {
        ScriptedNetwork network; network.responses << started;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&network);
        STREAM_CONFIGURATION configuration {};
        configuration.width = 1920; configuration.height = 1080; configuration.fps = 120;
        configuration.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        QString url;
        http.startApp("launch",true,42,&configuration,true,false,0,false,false,url,
                      {},-1,false,-1,QSize(2880,1800));
        QCOMPARE(QUrlQuery(network.requests[0]).queryItemValue("mode"),QString("1920x1080x0"));
    }
    void transientMissingTlsIdentityRetriesTheSameVerbWithFreshAuthentication() {
        ScriptedNetwork network;
        network.responses << "<root status_code=\"403\" status_message=\"A paired TLS client identity is required\"/>" << started;
        NvHTTP http(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&network);
        request(http,false);
        QCOMPARE(network.requests.size(),2);
        QCOMPARE(network.requests[0].path(),QString("/resume"));
        QCOMPARE(network.requests[1].path(),QString("/resume"));
        const auto first = network.details[0].sslConfiguration();
        const auto second = network.details[1].sslConfiguration();
        QVERIFY(!first.localCertificate().isNull());
        QCOMPARE(second.localCertificate(),first.localCertificate());
        QCOMPARE(second.privateKey(),first.privateKey());
        QVERIFY(second.testSslOption(QSsl::SslOptionDisableSessionSharing));
        QVERIFY(second.testSslOption(QSsl::SslOptionDisableSessionTickets));
    }
    void deniedPermissionIsNotRetriedAndMissingIdentityHasOnlyOneRetry() {
        ScriptedNetwork denied;
        denied.responses << "<root status_code=\"403\" status_message=\"Permission denied\"/>";
        NvHTTP first(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&denied);
        QVERIFY_EXCEPTION_THROWN(request(first,false),GfeHttpResponseException);
        QCOMPARE(denied.requests.size(),1);
        ScriptedNetwork missing;
        missing.responses << "<root status_code=\"403\" status_message=\"A paired TLS client identity is required\"/>"
                          << "<root status_code=\"403\" status_message=\"A paired TLS client identity is required\"/>";
        NvHTTP second(NvAddress("127.0.0.1",47989),47984,IdentityManager::get()->getSslConfig().localCertificate(),false,&missing);
        QVERIFY_EXCEPTION_THROWN(request(second,false),GfeHttpResponseException);
        QCOMPARE(missing.requests.size(),2);
    }
    void initTestCase() {
        QVERIFY(m_Settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_Settings.path());
    }
    void optionalExtensionsPreserveOldPeers() {
        ScriptedNetwork network; network.responses << started << started << started;
        NvHTTP http(NvAddress("127.0.0.1", 47989), 47984, IdentityManager::get()->getSslConfig().localCertificate(), false, &network);
        request(http, false);
        QUrlQuery legacy(network.requests[0]);
        QVERIFY(!legacy.hasQueryItem("desktopScale")); QVERIFY(!legacy.hasQueryItem("adaptiveBitrate"));
        QVERIFY(!legacy.hasQueryItem("virtualDisplay"));
        request(http, false, "2", 1, 1);
        QUrlQuery extended(network.requests[1]);
        QCOMPARE(extended.queryItemValue("desktopScale"), QString("2"));
        QCOMPARE(extended.queryItemValue("adaptiveBitrate"), QString("1"));
        QCOMPARE(extended.queryItemValue("virtualDisplay"), QString("1"));
        request(http, false, {}, -1, 0);
        QCOMPARE(QUrlQuery(network.requests[2]).queryItemValue("virtualDisplay"), QString("0"));
    }
    // Asking for VRR makes Vibepollo run its virtual display at 1000 Hz; an SDR stream says so.
    void launchAsksForAFixedRateSdrMode() {
        ScriptedNetwork network; network.responses << started;
        NvHTTP http(NvAddress("127.0.0.1", 47989), 47984, IdentityManager::get()->getSslConfig().localCertificate(), false, &network);
        request(http, false);
        const QUrlQuery query(network.requests[0]);
        QCOMPARE(query.queryItemValue("mode"), QString("1920x1080x60"));
        QCOMPARE(query.queryItemValue("clientVrrRequested"), QString("0"));
        QCOMPARE(query.queryItemValue("hdrMode"), QString("0"));
    }
    void automaticResumeCannotTurnIntoLaunch() {
        ScriptedNetwork network; network.responses << gone << "<root status_code=\"200\"><PairStatus>1</PairStatus><state>SUNSHINE_SERVER_FREE</state></root>" << started;
        NvHTTP http(NvAddress("127.0.0.1", 47989), 47984, IdentityManager::get()->getSslConfig().localCertificate(), false, &network);
        QVERIFY_EXCEPTION_THROWN(request(http, false), GfeHttpResponseException);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests[0].path(), QString("/resume"));
    }
    void explicitLegacyResumeOnlyFallsBackAfterFreshIdle() {
        ScriptedNetwork network; network.responses << gone << "<root status_code=\"200\"><PairStatus>1</PairStatus><state>SUNSHINE_SERVER_FREE</state></root>" << started;
        NvHTTP http(NvAddress("127.0.0.1", 47989), 47984, IdentityManager::get()->getSslConfig().localCertificate(), false, &network);
        request(http, true);
        QCOMPARE(network.requests.size(), 3);
        QCOMPARE(network.requests[1].path(), QString("/serverinfo"));
        QCOMPARE(network.requests[2].path(), QString("/launch"));
    }
    void busyHostIsNeverReplacedAndCancellationSendsNothing() {
        ScriptedNetwork network; network.responses << gone << "<root status_code=\"200\"><PairStatus>1</PairStatus><state>SUNSHINE_SERVER_BUSY</state></root>";
        NvHTTP http(NvAddress("127.0.0.1", 47989), 47984, IdentityManager::get()->getSslConfig().localCertificate(), false, &network);
        QVERIFY_EXCEPTION_THROWN(request(http, true), GfeHttpResponseException);
        QCOMPARE(network.requests.size(), 2);
        NvHTTP::CancelToken cancellation; cancellation.cancel(); http.setCancelToken(&cancellation);
        QVERIFY_EXCEPTION_THROWN(request(http, false), QtNetworkReplyException);
        QCOMPARE(network.requests.size(), 2);
    }
};
QTEST_GUILESS_MAIN(LaunchRequestTest)
#include "tst_launchrequest.moc"
