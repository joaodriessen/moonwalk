#include "streaming/clientclipboardimage.h"
#include <QKeyEvent>
#include <QtTest>

namespace FakeClipboard {
using namespace NativeClipboard::Detail;
inline DataCallback callback = nullptr;
inline CleanupCallback release = nullptr;
inline void* userdata = nullptr;
inline int failure = 0, clears = 0;
bool SDLCALL clear()
{
    ++clears;
    auto cleanup = release;
    auto data = userdata;
    callback = nullptr;
    release = nullptr;
    userdata = nullptr;
    if (cleanup)
        cleanup(data);
    return true;
}
bool SDLCALL set(DataCallback provide, CleanupCallback cleanup, void* data, const char* const*, size_t)
{
    clear();
    callback = provide;
    release = cleanup;
    userdata = data;
    if (failure == 1)
        clear();
    return failure == 0;
}
Functions api()
{
    Functions f;
    f.set = set;
    f.clear = clear;
    return f;
}
}

class ForeignClipboardMime final : public QMimeData {
    Q_OBJECT
public:
    mutable bool queried = false;
    QStringList formats() const override
    {
        queried = true;
        return { QStringLiteral("text/plain") };
    }
};

class ClipboardImageTest : public QObject {
    Q_OBJECT
private slots:
    void waylandLocalFallbackRejectsEmptySentinelWithoutQueryingForeignOffers()
    {
        QMimeData empty;
        QVERIFY(!NativeClipboard::Detail::localWaylandMime(&empty));
        empty.setText(QString());
        QVERIFY(NativeClipboard::Detail::localWaylandMime(&empty));
        ForeignClipboardMime foreign;
        QVERIFY(!NativeClipboard::Detail::localWaylandMime(&foreign));
        QVERIFY(!foreign.queried);
    }
    void pngRoundTripKeepsBytesDimensionsPixelsAndMime()
    {
        QImage original(31, 19, QImage::Format_ARGB32);
        original.fill(QColor(15, 40, 90, 127));
        original.setPixelColor(7, 8, QColor(255, 10, 20, 255));
        const auto bytes = ClientClipboardImage::encode(original);
        QVERIFY(!bytes.isEmpty());
        auto mime = ClientClipboardImage::decoded(bytes);
        QVERIFY(mime);
        QCOMPARE(mime->data("image/png"), bytes);
        const QImage restored = qvariant_cast<QImage>(mime->imageData());
        QCOMPARE(restored.size(), original.size());
        QCOMPARE(restored.pixelColor(7, 8), original.pixelColor(7, 8));
        QCOMPARE(restored.pixelColor(0, 0), original.pixelColor(0, 0));
        QCOMPARE(PngClipboard::hash(mime->data("image/png")), PngClipboard::hash(bytes));
    }
    void waylandPublicationOffersOnlyPngWhileDecodedApiKeepsLocalImageData()
    {
        QImage original(4, 3, QImage::Format_ARGB32);
        original.fill(QColor(31, 77, 123, 128));
        const auto png = ClientClipboardImage::encode(original);
        auto published = NativeClipboard::Detail::pngMime(png, true);
        QVERIFY(published);
        QCOMPARE(published->formats(), QStringList { QStringLiteral("image/png") });
        QCOMPARE(published->data("image/png"), png);
        QCOMPARE(QImage::fromData(published->data("image/png")).pixelColor(0, 0), original.pixelColor(0, 0));
        auto local = ClientClipboardImage::decoded(png);
        QVERIFY(local->hasImage());
        QCOMPARE(qvariant_cast<QImage>(local->imageData()).pixelColor(0, 0), original.pixelColor(0, 0));
    }
    void init()
    {
        FakeClipboard::clear();
        FakeClipboard::failure = 0;
    }
    void cleanup()
    {
        NativeClipboard::Detail::cancelInputRetry();
        FakeClipboard::clear();
        NativeClipboard::Detail::releaseSignalLease();
        QGuiApplication::clipboard()->blockSignals(false);
        QVERIFY(NativeClipboard::Detail::ownedPayload() == nullptr);
    }
    void nativePublicationIgnoresOnlyThePreviousQtClipboardObject()
    {
        QVERIFY(NativeClipboard::publishText("old Qt copy"));
        const auto* old = NativeClipboard::qtOwnedMimeData();
        QVERIFY(old);
        QVERIFY(NativeClipboard::Detail::offer(FakeClipboard::api(), "new native copy", false));
        QVERIFY(!NativeClipboard::qtOwnedMimeData());
        QVERIFY(NativeClipboard::Detail::offer(FakeClipboard::api(), "second native copy before Qt cancellation", false));
        QVERIFY(!NativeClipboard::qtOwnedMimeData());
        QGuiApplication::clipboard()->setText("new Qt copy");
        QVERIFY(NativeClipboard::qtOwnedMimeData());
        QVERIFY(NativeClipboard::qtOwnedMimeData() != old);
        QCOMPARE(NativeClipboard::readText(), QByteArray("new Qt copy"));
    }
    void teardownRetryRequiresGenuineInputAndNeverOverwritesReplacement()
    {
        using namespace NativeClipboard::Detail;
        auto* clipboard = QGuiApplication::clipboard();
        clipboard->setText("handoff");
        auto* expected = NativeClipboard::qtOwnedMimeData();
        inputRetry() = new InputRetry("handoff", false, expected);
        QKeyEvent synthetic(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
        QCoreApplication::sendEvent(clipboard, &synthetic);
        QVERIFY(inputRetry());
        QCOMPARE(NativeClipboard::qtOwnedMimeData(), expected);
        inputRetry()->considerInput(QEvent::KeyRelease, true);
        QVERIFY(inputRetry());
        // Exercise the production decision after a real input marker; actual
        // spontaneous delivery is covered by the external compositor fixture.
        QSignalSpy changed(clipboard, &QClipboard::dataChanged);
        inputRetry()->considerInput(QEvent::KeyPress, true);
        QVERIFY(!inputRetry());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(NativeClipboard::readText(), QByteArray("handoff"));
        inputRetry() = new InputRetry("handoff", false, NativeClipboard::qtOwnedMimeData());
        clipboard->setText("replacement");
        QVERIFY(!inputRetry());
        QCOMPARE(NativeClipboard::readText(), QByteArray("replacement"));
        inputRetry() = new InputRetry("replacement", false, NativeClipboard::qtOwnedMimeData());
        QVERIFY(offer(FakeClipboard::api(), "new session native", false));
        QVERIFY(!inputRetry());
    }
    void teardownHandoffsKnownPayloadAndPreservesNewerQtCopy()
    {
        using namespace NativeClipboard::Detail;
        QVERIFY(NativeClipboard::publishText("prior Qt text"));
        holdClipboardSignals(QGuiApplication::clipboard());
        QVERIFY(offer(FakeClipboard::api(), "latest native text", false));
        QVERIFY(NativeClipboard::finishSession());
        QCOMPARE(NativeClipboard::readText(), QByteArray("latest native text"));
        QVERIFY(!QGuiApplication::clipboard()->signalsBlocked());
        FakeClipboard::clear();
        holdClipboardSignals(QGuiApplication::clipboard());
        QVERIFY(offer(FakeClipboard::api(), "retained native text", false));
        QGuiApplication::clipboard()->setText("newer Qt copy");
        QVERIFY(NativeClipboard::finishSession());
        QCOMPARE(NativeClipboard::readText(), QByteArray("newer Qt copy"));
        QVERIFY(!QGuiApplication::clipboard()->signalsBlocked());
    }
    void nativeOwnershipLeaseSurvivesReplacementAndRestoresPriorSignalState()
    {
        using namespace NativeClipboard::Detail;
        auto* clipboard = QGuiApplication::clipboard();
        for (bool originallyBlocked : { false, true }) {
            clipboard->blockSignals(originallyBlocked);
            holdClipboardSignals(clipboard);
            QVERIFY(clipboard->signalsBlocked());
            QVERIFY(offer(FakeClipboard::api(), "first", false));
            holdClipboardSignals(clipboard); // replacement must not nest blockers
            QVERIFY(offer(FakeClipboard::api(), "second", false));
            QVERIFY(clipboard->signalsBlocked());
            FakeClipboard::clear();
            QCOMPARE(clipboard->signalsBlocked(), originallyBlocked);
            QVERIFY(!signalLease().clipboard);
            holdClipboardSignals(clipboard);
            FakeClipboard::failure = 2;
            QVERIFY(!offer(FakeClipboard::api(), "failure", false));
            QCOMPARE(clipboard->signalsBlocked(), originallyBlocked);
            FakeClipboard::failure = 0;
        }
    }
    void adoptingKnownNativePayloadRestoresSignalsBeforeQtPublication()
    {
        using namespace NativeClipboard::Detail;
        auto* clipboard = QGuiApplication::clipboard();
        holdClipboardSignals(clipboard);
        QVERIFY(offer(FakeClipboard::api(), "native text", false));
        QSignalSpy changed(clipboard, &QClipboard::dataChanged);
        QVERIFY(NativeClipboard::publishText("Qt panel copy", true));
        QVERIFY(!clipboard->signalsBlocked());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(NativeClipboard::readText(), QByteArray("Qt panel copy"));
        FakeClipboard::clear();
        QCoreApplication::processEvents();
        QVERIFY(!clipboard->signalsBlocked());
        QCOMPARE(NativeClipboard::readText(), QByteArray("Qt panel copy"));
    }
    void immutableNativePayloadSurvivesReplacementAndUsesExactCallbackAbi()
    {
        auto api = FakeClipboard::api();
        QByteArray original("first text");
        QVERIFY(NativeClipboard::Detail::offer(api, original, false));
        original[0] = 'X';
        size_t length = 0;
        const auto* data = FakeClipboard::callback(FakeClipboard::userdata, "text/plain;charset=utf-8", &length);
        QCOMPARE(QByteArray(static_cast<const char*>(data), qsizetype(length)), QByteArray("first text"));
        QVERIFY(!FakeClipboard::callback(FakeClipboard::userdata, nullptr, &length));
        QCOMPARE(length, size_t(0));
        QVERIFY(!FakeClipboard::callback(FakeClipboard::userdata, "image/png", &length));
        QVERIFY(NativeClipboard::Detail::offer(api, QByteArray("png bytes"), true));
        data = FakeClipboard::callback(FakeClipboard::userdata, "image/png", &length);
        QCOMPARE(QByteArray(static_cast<const char*>(data), qsizetype(length)), QByteArray("png bytes"));
        FakeClipboard::clear();
        QVERIFY(NativeClipboard::Detail::ownedPayload() == nullptr);
    }
    void nativeFailureCleansBothAlreadyReleasedAndRetainedUserdata()
    {
        for (int failure : { 1, 2 }) {
            FakeClipboard::failure = failure;
            QVERIFY(!NativeClipboard::Detail::offer(FakeClipboard::api(), QByteArray("payload"), true));
            QVERIFY(!FakeClipboard::userdata);
            QVERIFY(NativeClipboard::Detail::ownedPayload() == nullptr);
        }
    }
    void offscreenPublicationKeepsQtOwnershipAndRejectsInvalidPngWithoutMutation()
    {
        QVERIFY(NativeClipboard::supportsPng());
        QVERIFY(NativeClipboard::publishText("keep me"));
        QCOMPARE(NativeClipboard::readText(), QByteArray("keep me"));
        QVERIFY(!ClientClipboardImage::publish("invalid PNG"));
        QCOMPARE(NativeClipboard::readText(), QByteArray("keep me"));
        QImage image(13, 7, QImage::Format_ARGB32);
        image.fill(Qt::green);
        auto png = ClientClipboardImage::encode(image);
        QVERIFY(ClientClipboardImage::publish(png));
        QCOMPARE(ClientClipboardImage::read().png, png);
        QVERIFY(NativeClipboard::prepareForQtControls());
        QVERIFY(NativeClipboard::publishText("panel copy", true));
        QCOMPARE(NativeClipboard::readText(), QByteArray("panel copy"));
        QVERIFY(!ClientClipboardImage::read().image);
    }
    void textLimitReportsTooLargeAndCountsUtf8BeforeAllocation()
    {
        bool tooLarge = true;
        QVERIFY(NativeClipboard::publishText("short text"));
        QCOMPARE(NativeClipboard::readText(&tooLarge), QByteArray("short text"));
        QVERIFY(!tooLarge);
        QGuiApplication::clipboard()->setText(QString(400000, QChar(0x4e00)));
        QVERIFY(NativeClipboard::readText(&tooLarge).isEmpty());
        QVERIFY(tooLarge);
        QCOMPARE(NativeClipboard::Detail::textTooLarge(QString::fromUtf8("hello \xf0\x9f\x8c\x8d")), false);
        QGuiApplication::clipboard()->setText(QString(1024 * 1024 + 1, QLatin1Char('x')));
        QVERIFY(NativeClipboard::readText(&tooLarge).isEmpty());
        QVERIFY(tooLarge);
    }
    void wrongThreadCallsRefuseBeforeTouchingEitherClipboard()
    {
        bool published = true, supported = true;
        QByteArray text = "initial";
        auto* thread = QThread::create([&] {published=NativeClipboard::publishText("wrong thread");supported=NativeClipboard::supportsPng();text=NativeClipboard::readText(); });
        thread->start();
        QVERIFY(thread->wait(2000));
        delete thread;
        QVERIFY(!published);
        QVERIFY(!supported);
        QVERIFY(text.isEmpty());
    }
    void truncatedCorruptAndOversizedImagesAreRejectedBeforeApply()
    {
        QImage original(8, 8, QImage::Format_ARGB32);
        original.fill(Qt::blue);
        const auto bytes = ClientClipboardImage::encode(original);
        QVERIFY(!bytes.isEmpty());
        QVERIFY(!ClientClipboardImage::decoded(bytes.left(bytes.size() - 12)));
        auto corrupt = bytes;
        corrupt[corrupt.size() / 2] ^= 0x20;
        QVERIFY(!ClientClipboardImage::decoded(corrupt));
        QVERIFY(!ClientClipboardImage::decoded(QByteArray(PngClipboard::maxBytes + 1, 'x')));
        QVERIFY(ClientClipboardImage::encode(QImage(4097, 1, QImage::Format_ARGB32)).isEmpty());
    }
};
QTEST_MAIN(ClipboardImageTest)
#include "tst_clipboardimage.moc"
