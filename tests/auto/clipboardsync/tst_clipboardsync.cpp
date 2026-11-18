#include <QtTest>
#include "streaming/clipboardsync.h"
#include "SDL_compat.h"

namespace {
struct PulledValue { QByteArray content; quintptr generation = 0; int code = 0; };
bool takePulledValue(PulledValue &value)
{
    if (value.code != 0) return true;
    SDL_Event event {};
    if (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT, SDL_USEREVENT) <= 0) return false;
    value.generation = reinterpret_cast<quintptr>(event.user.data2);
    value.code = event.user.code;
    if (event.user.code == SDL_CODE_CLIPBOARD_IMAGE_APPLY) {
        auto *png = static_cast<QByteArray *>(event.user.data1);
        value.content = *png;
        delete png;
    } else {
        value.content = QByteArray(static_cast<char *>(event.user.data1));
        SDL_free(event.user.data1);
    }
    return true;
}
}

class ClipboardWorkerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QVERIFY(SDL_Init(SDL_INIT_EVENTS)==0);}
    void cleanupTestCase(){SDL_Quit();}
    void init() { ClipboardTransport::reset(); ClipboardSync::discardQueuedApplyEvents(); }
    void legacyTextUsesBoundedPollingAndSuppressesEcho() {
        ClipboardTransport::remoteBody = QString::fromUtf8("Windows café 漢 😀").toUtf8();
        ClipboardTransport::remoteType = "text/plain";
        ClipboardTransport::remoteStatus = 200;
        ClipboardTransport::repeatResponses = true;
        ClipboardSync sync(nullptr, false, true);
        sync.setFocused(true); sync.start();
        PulledValue value;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(value), 1000);
        QCOMPARE(value.content, ClipboardTransport::remoteBody);
        QCOMPARE(ClipboardTransport::queries().first(), QString("type=text"));
        sync.enqueueLocalChange(value.content);
        QTest::qWait(650);
        QCOMPARE(ClipboardTransport::posts.load(), 0);
        QVERIFY(ClipboardTransport::gets.load() <= 3);
        QElapsedTimer timer; timer.start(); sync.stopAndWait();
        QVERIFY(timer.elapsed() < 300);
    }
    void skylightStillRejectsMissingRevisionHeader() {
        ClipboardTransport::remoteBody = "ambiguous";
        ClipboardTransport::remoteStatus = 200;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() >= 1, 1000);
        QTest::qWait(50);
        PulledValue value;
        QVERIFY(!takePulledValue(value));
        sync.stopAndWait();
    }
    void legacyStreamRegistrationRaceDoesNotDisableClipboard() {
        ClipboardTransport::remoteBody = "registered";
        ClipboardTransport::remoteStatus = 403;
        ClipboardSync sync(nullptr, false, true); sync.setFocused(true); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::gets.load(), 1, 1000);
        QTest::qWait(50);
        ClipboardTransport::remoteStatus = 200;
        PulledValue value;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(value), 3500);
        QCOMPARE(value.content, QByteArray("registered"));
        sync.stopAndWait();
    }
    void clipboardReadAndWritePermissionsAreIndependent() {
        ClipboardSync sync(nullptr, false, true);
        sync.setPermissions(false, true);
        sync.setFocused(true); sync.enqueueLocalChange("allowed outgoing"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 1000);
        QCOMPARE(ClipboardTransport::gets.load(), 0);
        sync.setPermissions(false, false);
        sync.enqueueLocalChange("denied outgoing");
        QTest::qWait(100);
        QCOMPARE(ClipboardTransport::posts.load(), 1);
        sync.stopAndWait();
    }
    void directionChangeInvalidatesQueuedOutgoingAndOldApplies() {
        ClipboardSync sync(nullptr);sync.setFocused(true);sync.enqueueLocalChange("old");
        const auto epoch=sync.focusGeneration();sync.setDirection(0);
        QVERIFY(!sync.canApplyRemote(epoch));sync.start();QTest::qWait(100);
        QCOMPARE(ClipboardTransport::gets.load(),0);QCOMPARE(ClipboardTransport::posts.load(),0);
        sync.setDirection(2);sync.enqueueLocalChange("new");
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(),1,1000);
        QCOMPARE(ClipboardTransport::gets.load(),0);QCOMPARE(ClipboardTransport::sent().last(),QByteArray("new"));sync.stopAndWait();
    }
    void unfocusedEdgeApplyRemainsEligibleUntilNewEpochOrDirection()
    {
        ClipboardSync sync(nullptr);
        sync.setFocused(true);
        sync.setFocused(false);
        const auto edge=sync.focusGeneration();
        QVERIFY(sync.canApplyRemote(edge));
        sync.setDirection(2);
        QVERIFY(!sync.canApplyRemote(edge));
        sync.setDirection(1);
        QVERIFY(sync.canApplyRemote(sync.focusGeneration()));
        QVERIFY(!sync.canApplyRemote(edge));
    }
    void imageTransferUsesBinaryPayloadAndSuppressesEcho() {
        const auto png=QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNgYGD4DwABBAEAX+XDSwAAAABJRU5ErkJggg==");
        QVERIFY(ClipboardSync::validPng(png));
        QVERIFY(!ClipboardSync::validPng(png.left(png.size()-1)));
        auto corrupt=png;corrupt[corrupt.size()-5]=char(corrupt[corrupt.size()-5]^1);
        QVERIFY(!ClipboardSync::validPng(corrupt));
        QVERIFY(!ClipboardSync::validPng(png+"trailing"));
        ClipboardTransport::remoteBody=png;ClipboardTransport::remoteType="image/png";ClipboardTransport::remoteHeader="7";ClipboardTransport::remoteStatus=200;
        ClipboardSync sync(nullptr,true);sync.setFocused(true);sync.start();
        SDL_Event event={};bool got=false;QElapsedTimer timer;timer.start();
        while(timer.elapsed()<1000 && !got){got=SDL_PeepEvents(&event,1,SDL_GETEVENT,SDL_USEREVENT,SDL_USEREVENT)>0;if(!got)QTest::qWait(5);}
        QVERIFY(got);QCOMPARE(event.user.code,SDL_CODE_CLIPBOARD_IMAGE_APPLY);
        auto payload=static_cast<QByteArray*>(event.user.data1);QCOMPARE(*payload,png);delete payload;
        QVERIFY(sync.canApplyRemote(reinterpret_cast<quintptr>(event.user.data2)));
        sync.enqueueLocalImage(png);QTest::qWait(100);QCOMPARE(ClipboardTransport::posts.load(),0);
        sync.stopAndWait();
    }
    void unsupportedOrOversizeImagePreservesQueue() {
        ClipboardSync unsupported(nullptr,false);QSignalSpy warnings(&unsupported,&ClipboardSync::warning);
        unsupported.enqueueLocalImage("not png");QCOMPARE(warnings.count(),1);
        ClipboardSync supported(nullptr,true);QSignalSpy invalid(&supported,&ClipboardSync::warning);
        supported.enqueueLocalImage(QByteArray(4*1024*1024+1,'x'));QCOMPARE(invalid.count(),1);
    }
    void failedNativePublicationRetriesTheSameChangeWithBackoff_data() {
        QTest::addColumn<bool>("image");
        QTest::newRow("text") << false;
        QTest::newRow("png") << true;
    }
    void failedNativePublicationRetriesTheSameChangeWithBackoff() {
        QFETCH(bool, image);
        const auto bytes = image ? QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNgYGD4DwABBAEAX+XDSwAAAABJRU5ErkJggg==") : QByteArray("not yet published");
        ClipboardTransport::remoteBody = bytes;
        ClipboardTransport::remoteType = image ? "image/png" : "text/plain";
        ClipboardTransport::remoteHeader = "7";
        ClipboardTransport::remoteStatus = 200;
        ClipboardSync sync(nullptr, image); sync.setFocused(true); sync.start();
        PulledValue first;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(first), 1000);
        QCOMPARE(first.content, bytes);
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() >= 2, 1000);
        QCOMPARE(ClipboardTransport::queries().last(), image ? QString("type=auto&since=7") : QString("type=text&since=7"));
        const auto requestsBeforeFailure = ClipboardTransport::queries().size();
        ClipboardTransport::remoteStatus = 200; // Same host value, no new copy/change count.
        QElapsedTimer timer; timer.start();
        sync.reportRemoteApplyFailure(first.generation, first.content, image);
        QTest::qWait(150);
        PulledValue retry;
        QVERIFY(!takePulledValue(retry));
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(retry), 3500);
        QVERIFY(timer.elapsed() >= 1800);
        QCOMPARE(retry.content, bytes);
        QCOMPARE(retry.generation, first.generation);
        QCOMPARE(retry.code, image ? SDL_CODE_CLIPBOARD_IMAGE_APPLY : SDL_CODE_CLIPBOARD_APPLY);
        const auto requests = ClipboardTransport::queries();
        QVERIFY(requests.mid(requestsBeforeFailure).contains(image ? QString("type=auto&since=0") : QString("type=text&since=0")));
        // Successful publication needs no failure report and retains echo suppression.
        if (image) sync.enqueueLocalImage(bytes); else sync.enqueueLocalChange(bytes);
        QTest::qWait(100);
        QCOMPARE(ClipboardTransport::posts.load(), 0);
        sync.stopAndWait();
    }
    void staleFailureAfterNewLocalCopyCannotRewindOrDelayTheCopy() {
        ClipboardTransport::remoteBody = "old remote"; ClipboardTransport::remoteHeader = "7"; ClipboardTransport::remoteStatus = 200;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        PulledValue old;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(old), 1000);
        sync.enqueueLocalChange("fresh local");
        sync.reportRemoteApplyFailure(old.generation, old.content);
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 500);
        QCOMPARE(ClipboardTransport::sent().last(), QByteArray("fresh local"));
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::queries().last(), QString("type=text&since=7"), 1000);
        sync.stopAndWait();
        const auto gets = ClipboardTransport::gets.load();
        sync.reportRemoteApplyFailure(sync.focusGeneration(), old.content);
        QTest::qWait(50);
        QCOMPARE(ClipboardTransport::gets.load(), gets);
    }
    void identicalHostRecopyDoesNotAcknowledgeAFailedNativePublication() {
        ClipboardTransport::remoteBody = "same remote"; ClipboardTransport::remoteHeader = "7"; ClipboardTransport::remoteStatus = 200;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        PulledValue first;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(first), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() >= 2, 1000);
        ClipboardTransport::remoteHeader = "8"; ClipboardTransport::remoteStatus = 200;
        sync.requestFocusEdgePull();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::queries().last(), QString("type=text&since=8"), 1000);
        PulledValue duplicate;
        QVERIFY(!takePulledValue(duplicate));
        ClipboardTransport::remoteStatus = 200;
        sync.reportRemoteApplyFailure(first.generation, first.content);
        PulledValue retried;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(retried), 3500);
        QCOMPARE(retried.content, first.content);
        QCOMPARE(retried.generation, first.generation);
        QVERIFY(ClipboardTransport::queries().count("type=text&since=0") >= 2);
        sync.stopAndWait();
    }
    void acceptedFailureFollowedByAnIdenticalLocalCopyDoesNotLoseTheCopy() {
        ClipboardTransport::remoteBody = "same bytes"; ClipboardTransport::remoteHeader = "7"; ClipboardTransport::remoteStatus = 200;
        ClipboardTransport::gateGet = true;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        PulledValue old;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(old), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() >= 2, 1000);
        sync.reportRemoteApplyFailure(old.generation, old.content);
        sync.enqueueLocalChange(old.content);
        ClipboardTransport::allowArm = true;
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 500);
        QCOMPARE(ClipboardTransport::sent().last(), old.content);
        QCOMPARE(ClipboardTransport::queries().last(), QString("type=text&since=7"));
        sync.stopAndWait();
    }
    void failureForOlderQueuedValueCannotUndoNewerApply() {
        ClipboardTransport::remoteBody = "first"; ClipboardTransport::remoteHeader = "7"; ClipboardTransport::remoteStatus = 200;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        PulledValue old;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(old), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() >= 2, 1000);
        // Publish a newer host response without changing the focus epoch.
        ClipboardTransport::remoteBody = "second"; ClipboardTransport::remoteHeader = "8"; ClipboardTransport::remoteStatus = 200;
        sync.requestFocusEdgePull();
        PulledValue newer;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(newer), 1000);
        QCOMPARE(newer.generation, old.generation);
        QCOMPARE(newer.content, QByteArray("second"));
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::queries().last() == "type=text&since=8", 1000);
        const auto gets = ClipboardTransport::gets.load();
        sync.reportRemoteApplyFailure(old.generation, old.content);
        QTest::qWait(2200);
        QCOMPARE(ClipboardTransport::gets.load(), gets);
        QCOMPARE(ClipboardTransport::queries().last(), QString("type=text&since=8"));
        // A real failure of the latest value retries from its previous cursor,
        // not from zero or the failed value's already-acknowledged change count.
        const auto requestsBeforeFailure = ClipboardTransport::queries().size();
        ClipboardTransport::remoteStatus = 200;
        sync.reportRemoteApplyFailure(newer.generation, newer.content);
        PulledValue retried;
        QTRY_VERIFY_WITH_TIMEOUT(takePulledValue(retried), 3500);
        QCOMPARE(retried.content, newer.content);
        QVERIFY(ClipboardTransport::queries().mid(requestsBeforeFailure).contains("type=text&since=7"));
        sync.enqueueLocalChange(newer.content);
        QTest::qWait(100);
        QCOMPARE(ClipboardTransport::posts.load(), 0);
        sync.stopAndWait();
    }
    void localCopyInterruptsHeldRead() {
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() > 0, 1000);
        QElapsedTimer elapsed; elapsed.start();
        const auto oldResponseEpoch = sync.focusGeneration();
        sync.enqueueLocalChange("local");
        QVERIFY(sync.focusGeneration() > oldResponseEpoch);
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 500);
        QVERIFY(elapsed.elapsed() < 500);
        QCOMPARE(ClipboardTransport::sent(), QList<QByteArray>({"local"}));
        sync.stopAndWait();
    }
    void localCopyInReadArmGapIsNotLost() {
        ClipboardTransport::gateGet = true;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.start();
        QTRY_VERIFY_WITH_TIMEOUT(ClipboardTransport::gets.load() > 0, 1000);
        sync.enqueueLocalChange("arm-gap");
        ClipboardTransport::allowArm = true;
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 500);
        sync.stopAndWait();
    }
    void failedUploadRetriesOnlyNewestCopyWithBackoff() {
        ClipboardTransport::failPosts = 1;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.enqueueLocalChange("old"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 1000);
        sync.enqueueLocalChange("new");
        QTest::qWait(150);
        QCOMPARE(ClipboardTransport::posts.load(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 2, 3500);
        QCOMPARE(ClipboardTransport::sent(), QList<QByteArray>({"old", "new"}));
        sync.stopAndWait();
    }
    void focusLossDoesNotReplayAnOldFailedCopy() {
        ClipboardTransport::failPosts = 1;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.enqueueLocalChange("old"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 1000);
        sync.setFocused(false); sync.setFocused(true);
        QTest::qWait(2200);
        QCOMPARE(ClipboardTransport::posts.load(), 1);
        sync.enqueueLocalChange("fresh");
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 2, 1000);
        QCOMPARE(ClipboardTransport::sent().last(), QByteArray("fresh"));
        sync.stopAndWait();
    }
    void successfulOldUploadCannotClearNewerQueuedCopy() {
        ClipboardSync sync(nullptr); sync.setFocused(true);
        ClipboardTransport::onPost = [&] {
            if (ClipboardTransport::posts.load() == 1) sync.enqueueLocalChange("newer");
        };
        sync.enqueueLocalChange("first"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 2, 1000);
        QCOMPARE(ClipboardTransport::sent(), QList<QByteArray>({"first", "newer"}));
        sync.stopAndWait();
    }
    void unsupportedEndpointDoesNotRetryForever() {
        ClipboardTransport::httpError = 403;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.enqueueLocalChange("copy"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 1000);
        sync.enqueueLocalChange("another"); QTest::qWait(100);
        QCOMPARE(ClipboardTransport::posts.load(), 1);
        sync.stopAndWait();
    }
    void shutdownInterruptsAnUpload() {
        ClipboardTransport::holdPost = true;
        ClipboardSync sync(nullptr); sync.setFocused(true); sync.enqueueLocalChange("copy"); sync.start();
        QTRY_COMPARE_WITH_TIMEOUT(ClipboardTransport::posts.load(), 1, 1000);
        QElapsedTimer elapsed; elapsed.start(); sync.stopAndWait();
        QVERIFY(elapsed.elapsed() < 500);
    }
};
QTEST_GUILESS_MAIN(ClipboardWorkerTest)
#include "tst_clipboardsync.moc"
