#include <QtTest>

#include "../../../app/streaming/clipboardfocusguard.h"
#include "../../../app/streaming/input/clipboardtext.h"
#include "../../../app/streaming/streamstatus.h"

class ClipboardFocusGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesCopyMadeAfterFocusLoss();
    void preservesFocusedCopyBeforeItsNotification();
    void preservesCopyMadeBeforeFocusLossIsProcessed();
    void permitsRemoteCopyWhenLocalClipboardIsUnchanged();
    void focusRegainAdmitsFreshRemoteCopyButRejectsQueuedResponse();
    void typeClipboardPreservesUtf8AndNewlineSemantics();
    void typeClipboardBoundsLargeMultilineWork();
    void streamWarningsHaveIndependentLifetimes();
    void audioRecoveryDebouncesAndRetainsOtherWarnings();
    void clipboardFeedbackExpiresWithoutClearingPersistentWarnings();
};

void ClipboardFocusGuardTest::preservesFocusedCopyBeforeItsNotification()
{
    ClipboardFocusGuard guard("old image");
    guard.focusGained("old image", 7);
    QVERIFY(!guard.mayApply("new local image", 7));
    guard.focusedClipboardChanged("new local image");
    QVERIFY(guard.mayApply("new local image", 7));
    guard.applied("remote image");
    QVERIFY(guard.mayApply("remote image", 7));
}

void ClipboardFocusGuardTest::streamWarningsHaveIndependentLifetimes()
{
    StreamStatus state;
    state.setNetwork(StreamStatus::Network::Slow);
    state.setGamepadMouse(true);
    auto status = state.snapshot(0);
    QVERIFY(status.gamepadMouse);
    QCOMPARE(status.network, StreamStatus::Network::Slow);
    state.setNetwork(StreamStatus::Network::None);
    QVERIFY(state.snapshot(1).gamepadMouse);
    state.setNetwork(StreamStatus::Network::Poor);
    state.setGamepadMouse(false);
    status = state.snapshot(2);
    QVERIFY(!status.gamepadMouse);
    QCOMPARE(status.network, StreamStatus::Network::Poor);
}

void ClipboardFocusGuardTest::audioRecoveryDebouncesAndRetainsOtherWarnings()
{
    StreamStatus state;
    state.setNetwork(StreamStatus::Network::Poor);
    state.setAudioAvailable(false, 100);
    QVERIFY(!state.snapshot(599).audioUnavailable);
    state.setAudioAvailable(false, 600); // Failed retries must not restart the debounce.
    QVERIFY(state.snapshot(600).audioUnavailable);
    QVERIFY(!state.snapshot(600, true).audioUnavailable); // Focus-loss mute pauses recovery.
    QVERIFY(state.snapshot(600, true).network == StreamStatus::Network::Poor);
    QVERIFY(state.snapshot(600, false).audioUnavailable); // Unmuting retains the device failure.
    state.setAudioAvailable(true, 601);
    QVERIFY(!state.snapshot(601).audioUnavailable);
    QCOMPARE(state.snapshot(601).network, StreamStatus::Network::Poor);
    state.setAudioAvailable(false, UINT32_MAX - 100);
    QVERIFY(state.snapshot(400).audioUnavailable); // SDL tick wrap.
    state.setAudioAvailable(true, 401);
    state.setAudioAvailable(false, 500);
    state.setAudioAvailable(true, 600); // A brief device transition never flashes a warning.
    QVERIFY(!state.snapshot(2000).audioUnavailable);
}

void ClipboardFocusGuardTest::clipboardFeedbackExpiresWithoutClearingPersistentWarnings()
{
    StreamStatus state;
    state.setGamepadMouse(true);
    state.showClipboardNotice(StreamStatus::Notice::ClipboardEmpty, 100);
    QCOMPARE(state.snapshot(100).notice, StreamStatus::Notice::ClipboardEmpty);
    state.showClipboardNotice(StreamStatus::Notice::ClipboardTooLarge, 200);
    QCOMPARE(state.snapshot(200).notice, StreamStatus::Notice::ClipboardTooLarge);
    state.showClipboardNotice(StreamStatus::Notice::ClipboardQueueFailed, 300);
    QCOMPARE(state.snapshot(4299).notice, StreamStatus::Notice::ClipboardQueueFailed);
    QCOMPARE(state.snapshot(4300).notice, StreamStatus::Notice::None);
    QVERIFY(state.snapshot(4300).gamepadMouse);
    state.showClipboardNotice(StreamStatus::Notice::ClipboardQueueFailed, 5000);
    state.showClipboardNotice(StreamStatus::Notice::None, 5001); // Accepted retry supersedes rejection.
    QCOMPARE(state.snapshot(5001).notice, StreamStatus::Notice::None);
    QVERIFY(state.snapshot(5001).gamepadMouse);
}

void ClipboardFocusGuardTest::typeClipboardPreservesUtf8AndNewlineSemantics()
{
    QByteArray text = QString::fromUtf8("a\r\n\r\nb\rc\n你好🌙\r\n").toUtf8();
    const auto result = ClipboardText::prepare(text.data());
    QCOMPARE(result.status, ClipboardText::Status::Ready);
    QCOMPARE(QByteArray(text.constData()), QString::fromUtf8("a\n\nb\rc\n你好🌙\n").toUtf8());
    QCOMPARE(result.length, std::size_t(qstrlen(text.constData())));
    QCOMPARE(ClipboardText::prepare(nullptr).status, ClipboardText::Status::Empty);
}

void ClipboardFocusGuardTest::typeClipboardBoundsLargeMultilineWork()
{
    QByteArray text;
    text.reserve(ClipboardText::MaxBytes);
    for (std::size_t i = 0; i < ClipboardText::MaxBytes / 2; ++i) text.append("\r\n");
    const auto result = ClipboardText::prepare(text.data());
    QCOMPARE(result.status, ClipboardText::Status::Ready);
    QCOMPARE(result.length, ClipboardText::MaxBytes / 2);
    QCOMPARE(QByteArray(text.constData()), QByteArray(int(ClipboardText::MaxBytes / 2), '\n'));

    QByteArray tooLarge(int(ClipboardText::MaxBytes + 1), 'x');
    const auto original = tooLarge;
    QCOMPARE(ClipboardText::prepare(tooLarge.data()).status, ClipboardText::Status::TooLarge);
    QCOMPARE(tooLarge, original); // Never type a silently truncated document.
}

void ClipboardFocusGuardTest::preservesCopyMadeAfterFocusLoss()
{
    ClipboardFocusGuard guard("before");
    guard.focusLost("before");
    QVERIFY(!guard.mayApply("new local copy"));
    QVERIFY(!guard.mayApply("newer local copy"));
}

void ClipboardFocusGuardTest::preservesCopyMadeBeforeFocusLossIsProcessed()
{
    ClipboardFocusGuard guard("before");
    guard.focusLost("copy in another app");
    QVERIFY(!guard.mayApply("copy in another app"));
}

void ClipboardFocusGuardTest::permitsRemoteCopyWhenLocalClipboardIsUnchanged()
{
    ClipboardFocusGuard guard("before");
    guard.focusLost("before");
    QVERIFY(guard.mayApply("before"));
    guard.applied("remote copy");
    QVERIFY(guard.mayApply("remote copy"));
    QVERIFY(!guard.mayApply("later local copy"));
}

void ClipboardFocusGuardTest::focusRegainAdmitsFreshRemoteCopyButRejectsQueuedResponse()
{
    ClipboardFocusGuard guard("before");
    guard.focusLost("before");
    QVERIFY(!guard.mayApply("local copy", 1));
    guard.focusedClipboardChanged("local copy");
    guard.focusGained("local copy", 2);
    QVERIFY(!guard.mayApply("local copy", 1));
    QVERIFY(guard.mayApply("local copy", 2));
    guard.applied("fresh remote copy");
    QVERIFY(guard.mayApply("fresh remote copy", 2));
    QVERIFY(!guard.mayApply("fresh remote copy", 1));

    guard.focusLost("local copy before loss event");
    QVERIFY(!guard.mayApply("local copy before loss event", 2));
    guard.focusGained("local copy before loss event", 3);
    QVERIFY(guard.mayApply("local copy before loss event", 3));
    QVERIFY(!guard.mayApply("local copy before loss event", 2));
}

QTEST_GUILESS_MAIN(ClipboardFocusGuardTest)

#include "tst_clipboardguard.moc"
