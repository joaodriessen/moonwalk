#include <QtTest>
#include <QFile>
#include <memory>
#include <limits>
#include "streaming/streamtoolbar.h"

#include "path.h"
#include "streaming/video/overlaymanager.h"

// Exercise the production renderer/font lifetime while replacing only asset lookup.
QByteArray Path::readDataFile(QString fileName)
{
    if (fileName != QStringLiteral("ModeSeven.ttf")) return {};
    QFile font(QFINDTESTDATA("../../../app/ModeSeven.ttf"));
    return font.open(QIODevice::ReadOnly) ? font.readAll() : QByteArray();
}

namespace {
class Renderer final : public Overlay::IOverlayRenderer {
public:
    void notifyOverlayUpdated(Overlay::OverlayType type) override {
        if (type == Overlay::OverlayStatusUpdate) ++notifications;
        if (type == Overlay::OverlayToolbar) ++toolbarNotifications;
    }
    int notifications = 0;
    int toolbarNotifications = 0;
};
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
Surface takeSurface(Overlay::OverlayManager& overlays)
{
    return Surface(overlays.getUpdatedOverlaySurface(Overlay::OverlayStatusUpdate), SDL_FreeSurface);
}
const char audio[] = "Audio output is unavailable. Moonwalk is trying to reconnect it.";
const char clipboard[] = "Type Clipboard: the clipboard has no text.";
const char composed[] = "Audio output is unavailable. Moonwalk is trying to reconnect it.\n"
                        "Slow connection to host. Reduce your bitrate.\n"
                        "Gamepad mouse mode active. Long press Start to deactivate.\n"
                        "Type Clipboard: text exceeds the 1 MiB limit. Nothing was typed.";
}

class OverlayManagerTest final : public QObject {
    Q_OBJECT
private slots:
    void warningSurfaceFitsWindow_data();
    void warningSurfaceFitsWindow();
    void resizeRebuildsVisibleSurfaceWithoutStatusChange();
    void hiddenOverlayDefersRenderingUntilEnabled();
    void toolbarHitRegionsMatchLogicalLayout_data();
    void toolbarHitRegionsMatchLogicalLayout();
    void toolbarRasterFollowsDpiPaletteAndState();
    void toolbarFontUsesLocalPixelsAcrossPaintDeviceDpis();
    void toolbarUsesLocalPointsAcrossSdlCoordinateConventions();
    void toolbarPillOpensIntoTheBarAndBack();
    void toolbarOwnershipTransfersOnceAndHideClearsPending();
    void statusUsesNativeTypographyAndPhysicalDpi();
};

void OverlayManagerTest::warningSurfaceFitsWindow_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<QByteArray>("message");
    for (int width : {868, 640, 400}) {
        for (const auto& item : {qMakePair("audio", audio), qMakePair("clipboard", clipboard),
                                 qMakePair("composed", composed)}) {
            const QByteArray name = QByteArray(item.first) + '-' + QByteArray::number(width);
            QTest::newRow(name.constData()) << width << QByteArray(item.second);
        }
    }
}

void OverlayManagerTest::warningSurfaceFitsWindow()
{
    QFETCH(int, width);
    QFETCH(QByteArray, message);
    Renderer renderer;
    Overlay::OverlayManager overlays;
    overlays.setOverlayRenderer(&renderer);
    overlays.setDisplayWidth(width);
    overlays.updateOverlayText(Overlay::OverlayStatusUpdate, message.constData());
    overlays.setOverlayState(Overlay::OverlayStatusUpdate, true);
    auto surface = takeSurface(overlays);
    QVERIFY2(surface != nullptr, TTF_GetError());
    QVERIFY2(surface->w <= width, qPrintable(QStringLiteral("Surface %1 exceeds window %2")
                                             .arg(surface->w).arg(width)));
    QVERIFY(surface->w > 0);
    QVERIFY(surface->h > 0);
    QCOMPARE(renderer.notifications, 1);
    // The renderer consumes each transferred surface exactly once.
    QVERIFY(takeSurface(overlays) == nullptr);
}

void OverlayManagerTest::resizeRebuildsVisibleSurfaceWithoutStatusChange()
{
    Renderer renderer;
    Overlay::OverlayManager overlays;
    overlays.setOverlayRenderer(&renderer);
    overlays.setDisplayWidth(1200);
    overlays.updateOverlayText(Overlay::OverlayStatusUpdate, composed);
    overlays.setOverlayState(Overlay::OverlayStatusUpdate, true);
    auto wide = takeSurface(overlays);
    QVERIFY(wide != nullptr);
    const int initialHeight = wide->h;
    const QByteArray initialText(overlays.getOverlayText(Overlay::OverlayStatusUpdate));

    overlays.setDisplayWidth(400);
    QCOMPARE(renderer.notifications, 2);
    auto narrow = takeSurface(overlays);
    QVERIFY(narrow != nullptr);
    QVERIFY(narrow->w <= 400);
    QVERIFY(narrow->h > initialHeight);
    QCOMPARE(QByteArray(overlays.getOverlayText(Overlay::OverlayStatusUpdate)), initialText);

    overlays.setDisplayWidth(400);
    QCOMPARE(renderer.notifications, 2); // Unchanged geometry doesn't rebuild every SDL tick.
    QVERIFY(takeSurface(overlays) == nullptr);
    overlays.setDisplayWidth(868);
    QCOMPARE(renderer.notifications, 3);
    auto restored = takeSurface(overlays);
    QVERIFY(restored != nullptr);
    QVERIFY(restored->w <= 868);
    QVERIFY(restored->h < narrow->h);
    // All three transferred surfaces remain valid until their owning renderer releases them.
    QVERIFY(wide->pixels != nullptr);
    QVERIFY(narrow->pixels != nullptr);
}

void OverlayManagerTest::hiddenOverlayDefersRenderingUntilEnabled()
{
    Renderer renderer;
    Overlay::OverlayManager overlays;
    overlays.setOverlayRenderer(&renderer);
    overlays.updateOverlayText(Overlay::OverlayStatusUpdate, clipboard);
    overlays.setDisplayWidth(400);
    QCOMPARE(renderer.notifications, 0);
    QVERIFY(takeSurface(overlays) == nullptr);
    overlays.setOverlayState(Overlay::OverlayStatusUpdate, true);
    auto surface = takeSurface(overlays);
    QVERIFY(surface != nullptr);
    QVERIFY(surface->w <= 400);
    overlays.setOverlayState(Overlay::OverlayStatusUpdate, false);
    QCOMPARE(renderer.notifications, 2);
    QVERIFY(takeSurface(overlays) == nullptr);
    // Consuming a surface does not make the manager retain or free the renderer's ownership.
    QVERIFY(surface->pixels != nullptr);
}

void OverlayManagerTest::statusUsesNativeTypographyAndPhysicalDpi()
{
    Renderer renderer;
    Overlay::OverlayManager overlays;
    overlays.setOverlayRenderer(&renderer);
    overlays.setDisplayWidth(520,1);
    overlays.updateOverlayText(Overlay::OverlayStatusUpdate,"Preparing the remote desktop… 日本語");
    overlays.setOverlayState(Overlay::OverlayStatusUpdate,true);
    auto one=takeSurface(overlays); QVERIFY(one);
    overlays.setDisplayWidth(520,2);
    auto two=takeSurface(overlays); QVERIFY(two);
    QCOMPARE(two->w,one->w*2); QCOMPARE(two->h,one->h*2);
    QVERIFY(two->w<=1040);
}

void OverlayManagerTest::toolbarHitRegionsMatchLogicalLayout_data()
{
    QTest::addColumn<int>("width");
    for (int width : {128, 320, 520, 759, 760, 1920})
        QTest::newRow(qPrintable(QString::number(width))) << width;
}

void OverlayManagerTest::toolbarHitRegionsMatchLogicalLayout()
{
    QFETCH(int, width);
    const auto buttons = StreamToolbar::layout(width);
    QCOMPARE(buttons.size(), width < 480 ? 3 : 6);
    const auto bounds = StreamToolbar::bounds(width);
    QVERIFY(QRect(0, 0, width, StreamToolbar::ImageHeight).contains(bounds));
    QCOMPARE(bounds.y(), StreamToolbar::BarTop);
    for (int i = 0; i < buttons.size(); ++i) {
        const auto& button = buttons[i];
        QVERIFY(button.rect.width() >= 18);
        QCOMPARE(button.rect.height(), 44);
        QCOMPARE(StreamToolbar::hitTest(width, button.rect.center()), button.action);
        QCOMPARE(StreamToolbar::hitTest(width, button.rect.topLeft()), button.action);
        QVERIFY(bounds.contains(button.rect));
        if (i) QVERIFY(!buttons[i - 1].rect.intersects(button.rect));
        QCOMPARE(StreamToolbar::hitTest(width, QPoint(button.rect.x(), button.rect.y() - 1)), StreamToolbar::None);
    }
    QCOMPARE(buttons.last().action, StreamToolbar::Disconnect);
    QCOMPARE(StreamToolbar::hitTest(width, QPoint(-1, 20)), StreamToolbar::None);
    QCOMPARE(StreamToolbar::hitTest(width, QPoint(width, 20)), StreamToolbar::None);
    QCOMPARE(StreamToolbar::hitTest(width, QPoint(width / 2, StreamToolbar::BarTop + StreamToolbar::BarHeight)), StreamToolbar::None);
    QVERIFY(StreamToolbar::layout(127).isEmpty());
    const auto grip=StreamToolbar::handleRect(width);
    QVERIFY(qAbs(grip.center().x() - width / 2) <= 1);
    QCOMPARE(grip.height(),12); QCOMPARE(grip.y(),0);
    QVERIFY(StreamToolbar::handleRect(111).isEmpty());
    QVERIFY(StreamToolbar::onHandle(width,QPoint(width/2,0)));
    QVERIFY(StreamToolbar::onHandle(width,QPoint(width/2,-40))); // Above a letterboxed picture.
    QVERIFY(!StreamToolbar::onHandle(width,QPoint(width/2,12))); // Mac menu labels remain reachable.
    if (width>=400) QVERIFY(!StreamToolbar::onHandle(width,QPoint(20,0))); // Nor the corners of the host's own bars.
    StreamToolbar::Look rest; rest.capsule=StreamToolbar::pillRect(width,0,false);
    const auto pill=StreamToolbar::hud(width,2,QGuiApplication::font(),QGuiApplication::palette(),rest);
    QCOMPARE(pill.height(),32); // Only as tall as the pill while it's one.
    QCOMPARE(pill.pixelColor(width,28).alpha(),0);
    QVERIFY(pill.pixelColor(width,16).alpha()>180); // The pill itself.
}

void OverlayManagerTest::toolbarRasterFollowsDpiPaletteAndState()
{
    QFont font = QGuiApplication::font();
    font.setPixelSize(18);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#193045"));
    palette.setColor(QPalette::WindowText, QColor("#f6f7f8"));
    palette.setColor(QPalette::Mid, QColor("#778899"));
    palette.setColor(QPalette::Highlight, QColor("#3584e4"));
    const StreamToolbar::State rest;
    const auto normal = StreamToolbar::image(900, 1, font, palette, rest);
    const auto highDpi = StreamToolbar::image(900, 2, font, palette, rest);
    QCOMPARE(normal.size(), QSize(900, StreamToolbar::ImageHeight));
    QCOMPARE(highDpi.size(), QSize(1800, StreamToolbar::ImageHeight * 2));
    QCOMPARE(highDpi.devicePixelRatio(), qreal(2));
    QCOMPARE(normal.pixelColor(0, 0).alpha(), 0);
    const auto bounds = StreamToolbar::bounds(900);
    QCOMPARE(normal.pixelColor(bounds.center().x(), bounds.top() + 3), palette.color(QPalette::Window));
    StreamToolbar::Look collapsed;
    collapsed.capsule = StreamToolbar::pillRect(900, 0, false);
    const auto darkHandle = StreamToolbar::hud(900, 2, font, palette, collapsed).pixelColor(900, 16);
    QPalette lightPalette(palette);
    lightPalette.setColor(QPalette::Window, QColor("#f6f6f8"));
    lightPalette.setColor(QPalette::WindowText, QColor("#202024"));
    const auto lightHandle = StreamToolbar::hud(900, 2, font, lightPalette, collapsed).pixelColor(900, 16);
    QVERIFY(darkHandle.lightnessF() < .5);
    QVERIFY(darkHandle.alpha() > 180);
    QVERIFY(lightHandle.lightnessF() > .9);
    QVERIFY(lightHandle.alpha() > 180);
    StreamToolbar::State toggled;
    toggled.muted = toggled.fullscreen = toggled.captured = toggled.actual = true;
    QVERIFY(normal != StreamToolbar::image(900, 1, font, palette, toggled));
    StreamToolbar::State hovering;
    hovering.hovered = StreamToolbar::Mute;
    const auto tooltip=StreamToolbar::image(900,1,font,palette,hovering);
    QCOMPARE(StreamToolbar::label(StreamToolbar::Mute,false,false,false,false),StreamToolbar::text("Mute"));
    QCOMPARE(StreamToolbar::label(StreamToolbar::Mute,true,false,false,false),StreamToolbar::text("Unmute"));
    QVERIFY(tooltip != normal);
    QVERIFY(tooltip.pixelColor(StreamToolbar::layout(900).first().rect.center().x(), bounds.bottom() + 20).alpha() > 200);
    // Live status is drawn only when there's room for it, and a slow link says so in words.
    QVERIFY(StreamToolbar::showsStatus(900));
    QVERIFY(!StreamToolbar::showsStatus(759));
    StreamToolbar::Status status; status.host = QStringLiteral("Studio-PC"); status.rttMs = 8; status.fps = 60;
    QCOMPARE(StreamToolbar::statusText(status), StreamToolbar::text("%1 ms").arg(8) + QStringLiteral(" · ") + StreamToolbar::text("%1 fps").arg(60));
    status.poor = true;
    QCOMPARE(StreamToolbar::statusText(status), StreamToolbar::text("Slow connection"));
    QCOMPARE(StreamToolbar::statusText({}), StreamToolbar::text("Connected"));
    // Opening settles exactly on the finished picture, and the inset is painted in place.
    QCOMPARE(StreamToolbar::hud(900, 1, font, palette, StreamToolbar::settled(900), StreamToolbar::controls(900, 1, font, palette, rest)), normal);
    QCOMPARE(StreamToolbar::hud(900, 1, font, palette, StreamToolbar::settled(900), StreamToolbar::controls(900, 1, font, palette, rest), {}, 45),
             StreamToolbar::inset(normal, 45));
    auto pixels = Surface(StreamToolbar::surface(900, 2, font, palette, rest), SDL_FreeSurface);
    QVERIFY(pixels != nullptr);
    QCOMPARE(pixels->w, highDpi.width());
    QCOMPARE(pixels->h, highDpi.height());
    QCOMPARE(pixels->format->format, Uint32(SDL_PIXELFORMAT_ARGB8888));
    for (int y : {0, 24, 111})
        QCOMPARE(memcmp(static_cast<char*>(pixels->pixels) + y * pixels->pitch,
                        highDpi.constScanLine(y), highDpi.width() * 4), 0);
    QVERIFY(StreamToolbar::image(0, 1, font, palette, rest).isNull());
    QVERIFY(StreamToolbar::image(900, 0, font, palette, rest).isNull());
    QVERIFY(StreamToolbar::image(900, std::numeric_limits<qreal>::infinity(), font, palette, rest).isNull());
    QVERIFY(StreamToolbar::image(9000, 2, font, palette, rest).isNull());
}

void OverlayManagerTest::toolbarUsesLocalPointsAcrossSdlCoordinateConventions()
{
    const auto physical = StreamToolbar::metrics(2880, 2880, 2);
    const auto logical = StreamToolbar::metrics(1440, 2880, 2);
    QCOMPARE(physical.width, 1440);
    QCOMPARE(logical.width, physical.width);
    QCOMPARE(physical.pixelRatio, logical.pixelRatio);
    for (const auto& button : StreamToolbar::layout(physical.width)) {
        const auto point = button.rect.center();
        QCOMPARE(StreamToolbar::hitTest(physical.width, physical.point(point.x()*2,point.y()*2)),button.action);
        QCOMPARE(StreamToolbar::hitTest(logical.width, logical.point(point.x(),point.y())),button.action);
    }
    // A picture padded by bars: its top edge is the toolbar's top edge, because
    // the pointer may be held inside it.
    const auto letterboxed = StreamToolbar::metrics(2880, 2880, 2, 90);
    QCOMPARE(letterboxed.topInset, 45);
    QCOMPARE(letterboxed.point(1440, 90), QPoint(720, 0));
    QVERIFY(StreamToolbar::onHandle(letterboxed.width, letterboxed.point(1440, 90)));
    QFont insetFont; insetFont.setPixelSize(14);
    const auto rest = StreamToolbar::image(900, 2, insetFont, QGuiApplication::palette(), StreamToolbar::State{});
    const auto moved = StreamToolbar::inset(rest, 45);
    QCOMPARE(moved.height(), rest.height() + 90);
    QCOMPARE(moved.pixelColor(900, 90 + (StreamToolbar::BarTop + 3) * 2), rest.pixelColor(900, (StreamToolbar::BarTop + 3) * 2));
    QCOMPARE(moved.pixelColor(900, 10).alpha(), 0);
    QPalette native; native.setColor(QPalette::Window,Qt::white); native.setColor(QPalette::WindowText,Qt::black);
    const auto dark = StreamToolbar::palette(native,2);
    QVERIFY(dark.color(QPalette::Window).lightnessF() < .3);
    QVERIFY(dark.color(QPalette::WindowText).lightnessF() > .9);
    const auto light = StreamToolbar::palette(dark,1);
    QVERIFY(light.color(QPalette::Window).lightnessF() > .9);
    QVERIFY(light.color(QPalette::WindowText).lightnessF() < .3);
    QVERIFY(light.color(QPalette::Window).alphaF() > .9);
    QVERIFY(light.color(QPalette::Window).alphaF() < 1);
    const auto theme = StreamToolbar::palette();
    QCOMPARE(StreamToolbar::palette().cacheKey(), theme.cacheKey());
    QFont small; small.setPixelSize(10);
    QVERIFY(QFontInfo(StreamToolbar::font(small)).pixelSize() >= 14);
    QFont large; large.setPixelSize(24);
    QCOMPARE(QFontInfo(StreamToolbar::font(large)).pixelSize(),24);
    const auto image = StreamToolbar::image(physical.width,physical.pixelRatio,
        StreamToolbar::font(small),theme,StreamToolbar::State{});
    QCOMPARE(image.size(),QSize(2880,StreamToolbar::ImageHeight*2));
}

void OverlayManagerTest::toolbarPillOpensIntoTheBarAndBack()
{
    const int width = 1200;
    QFont font; font.setPixelSize(14);
    const QRectF bar = StreamToolbar::bounds(width);
    const QRectF message = StreamToolbar::messageRect(width, font, QStringLiteral("Ctrl+Alt+Shift+O for stream controls"));
    const auto pill = [&](qreal emphasis) { return StreamToolbar::pillRect(width, emphasis, false); };
    StreamToolbar::Motion motion;
    const auto run = [&](StreamToolbar::Mode mode, qreal emphasis, int frames) {
        bool moving = true;
        for (int i = 0; i < frames && moving; ++i)
            moving = motion.advance(mode, pill(emphasis), message, bar, emphasis, 1.0 / 60, false);
        return moving;
    };
    // Starts as the resting pill, which grows and brightens under the pointer.
    QVERIFY(!run(StreamToolbar::Pill, 0, 1));
    QCOMPARE(motion.look().capsule, pill(0));
    QVERIFY(!run(StreamToolbar::Pill, 1, 120));
    QCOMPARE(motion.look().capsule, pill(1));
    QCOMPARE(motion.look().bright, 1.0);
    QVERIFY(pill(1).width() > pill(0).width() && pill(1).height() > pill(0).height());
    QVERIFY(StreamToolbar::pillRect(width, 1, true).width() < pill(1).width()); // Pressed squeezes.

    // Tapped: the same capsule grows into the bar; the controls wait until it's nearly there.
    qreal widest = 0;
    bool controlsEarly = false;
    for (int i = 0; i < 120; ++i) {
        motion.advance(StreamToolbar::Bar, pill(0), message, bar, 0, 1.0 / 60, false);
        const auto look = motion.look();
        widest = qMax(widest, look.capsule.width());
        controlsEarly |= look.controls > 0 && look.capsule.width() < bar.width() * .85;
    }
    QVERIFY(!controlsEarly);
    QVERIFY(widest > bar.width()); // A little spring in it,
    QVERIFY(widest < bar.width() * 1.03); // but only a little.
    QCOMPARE(motion.look(), StreamToolbar::settled(width)); // And it comes to rest exactly.

    // Closing: the controls leave first, then the capsule shrinks without bouncing.
    bool shrankEarly = false;
    qreal narrowest = bar.width();
    for (int i = 0; i < 120; ++i) {
        motion.advance(StreamToolbar::Pill, pill(0), message, bar, 0, 1.0 / 60, false);
        const auto look = motion.look();
        shrankEarly |= look.controls > .5 && look.capsule.width() < bar.width() * .95;
        narrowest = qMin(narrowest, look.capsule.width());
    }
    QVERIFY(!shrankEarly);
    QVERIFY(narrowest >= pill(0).width() - .05);
    QCOMPARE(motion.look().capsule, pill(0));
    QCOMPARE(motion.look().glass, 0.0);

    // A change of mind half-way turns back without a jump.
    run(StreamToolbar::Bar, 0, 8);
    const auto before = motion.look().capsule;
    motion.advance(StreamToolbar::Pill, pill(0), message, bar, 0, 1.0 / 60, false);
    QVERIFY(qAbs(motion.look().capsule.width() - before.width()) < bar.width() * .1);
    QVERIFY(!run(StreamToolbar::Pill, 0, 240));
    QCOMPARE(motion.look().capsule, pill(0));

    // The keyboard hint is the same capsule with words in it; reduced motion jumps straight there.
    QVERIFY(motion.advance(StreamToolbar::Message, pill(0), message, bar, 0, 1.0 / 60, true));
    QCOMPARE(motion.look().capsule, message);
    QCOMPARE(motion.look().words, 1.0);
}

void OverlayManagerTest::toolbarOwnershipTransfersOnceAndHideClearsPending()
{
    Renderer renderer;
    Surface transferred(nullptr, SDL_FreeSurface);
    {
        Overlay::OverlayManager overlays;
        // Publishing before renderer attachment must still transfer ownership.
        auto first = SDL_CreateRGBSurfaceWithFormat(0, 32, 16, 32, SDL_PIXELFORMAT_ARGB8888);
        QVERIFY(first != nullptr);
        overlays.updateToolbar(first);
        QVERIFY(overlays.isOverlayEnabled(Overlay::OverlayToolbar));
        overlays.setOverlayRenderer(&renderer);
        transferred.reset(overlays.getUpdatedOverlaySurface(Overlay::OverlayToolbar));
        QCOMPARE(transferred.get(), first);
        QVERIFY(overlays.getUpdatedOverlaySurface(Overlay::OverlayToolbar) == nullptr);
        auto pending = SDL_CreateRGBSurfaceWithFormat(0, 64, 16, 32, SDL_PIXELFORMAT_ARGB8888);
        QVERIFY(pending != nullptr);
        overlays.updateToolbar(pending);
        QCOMPARE(renderer.toolbarNotifications, 1);
        overlays.updateToolbar(nullptr);
        QCOMPARE(renderer.toolbarNotifications, 2);
        QVERIFY(!overlays.isOverlayEnabled(Overlay::OverlayToolbar));
        QVERIFY(overlays.getUpdatedOverlaySurface(Overlay::OverlayToolbar) == nullptr);
        overlays.updateToolbar(nullptr);
        QCOMPARE(renderer.toolbarNotifications, 2);
    }
    // Manager teardown must never free a surface already owned by a renderer.
    QVERIFY(transferred->pixels != nullptr);
    QCOMPARE(SDL_FillRect(transferred.get(), nullptr, 0xff123456), 0);
}

QTEST_MAIN(OverlayManagerTest)
#include "tst_overlaymanager.moc"

void OverlayManagerTest::toolbarFontUsesLocalPixelsAcrossPaintDeviceDpis()
{
    QFont native; native.setPointSizeF(12);
    const auto font = StreamToolbar::font(native);
    QImage first(100,100,QImage::Format_ARGB32), second(100,100,QImage::Format_ARGB32);
    second.setDotsPerMeterX(first.dotsPerMeterX()*2);
    second.setDotsPerMeterY(first.dotsPerMeterY()*2);
    // The HUD already applies display scaling through its image DPR. Layout
    // metrics and actual painting must agree even when device DPI differs.
    QCOMPARE(QFontMetricsF(font,&first).horizontalAdvance("Disconnect"),
             QFontMetricsF(font,&second).horizontalAdvance("Disconnect"));
}
