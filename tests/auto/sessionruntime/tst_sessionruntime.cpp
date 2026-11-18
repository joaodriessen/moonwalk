#include <QtTest>
#include <limits>
#include "streaming/presentationhealth.h"
#include "streaming/viewscalepolicy.h"
#include "streaming/sessionretrypolicy.h"
#include "streaming/desktopscalepolicy.h"
#include "streaming/cursorsizepolicy.h"
#include "streaming/windowgeometrypolicy.h"
#include "streaming/windowdecoration.h"
#include "streaming/input/localtextinputpolicy.h"

class SessionRuntimeTest : public QObject {
    Q_OBJECT
private slots:
    void cursorPreferencesUseTheActiveDesktopAndValidateFallbacks() {
        QCOMPARE(CursorSizePolicy::nominalSize("Hyprland",24,32),32);
        QCOMPARE(CursorSizePolicy::nominalSize("GNOME",24,32),24);
        QCOMPARE(CursorSizePolicy::nominalSize("Hyprland",24,0),24);
        QCOMPARE(CursorSizePolicy::nominalSize("Hyprland",200,4),24);
        QCOMPARE(CursorSizePolicy::nominalSize("KDE",0,48),48);
    }
    void colorCursorUsesWindowUnitsInsteadOfApplyingDisplayScaleTwice() {
        QCOMPARE(CursorSizePolicy::coordinateScale(2880,2880,2),2.0);
        QCOMPARE(CursorSizePolicy::coordinateScale(1440,2880,2),1.0);
        QCOMPARE(CursorSizePolicy::coordinateScale(1920,2880,1.5),1.0);
        QCOMPARE(CursorSizePolicy::coordinateScale(1440,2880,1.5),0.75);
    }
    void nativeResolutionRestoreLeavesRoomForWindowDecorations() {
        using WindowGeometryPolicy::initialWindowedSize;
        QCOMPARE(initialWindowedSize({2880,1800},{2880,1800}),QSize(2304,1440));
        QCOMPARE(initialWindowedSize({1440,900},{1440,900}),QSize(1152,720));
        QCOMPARE(initialWindowedSize({1280,720},{2880,1800}),QSize(1280,720));
        QCOMPARE(initialWindowedSize({1080,1920},{1920,1080}),QSize(486,864));
        QVERIFY(initialWindowedSize({},{1440,900}).isEmpty());
    }
    void onlyCaptionlessCompositorsGetAClientSideTitleBar() {
        using WindowDecoration::policyForDesktop;
        for (const char* desktop : {"Hyprland", "ubuntu:Hyprland", "hyprland", "river", "niri"}) {
            QVERIFY2(policyForDesktop(desktop).forceClientSide, desktop);
            QVERIFY2(policyForDesktop(desktop).preferClientSide, desktop);
        }
        // KWin draws the real thing, and libdecor picks itself on GNOME.
        for (const char* desktop : {"KDE", "GNOME", "ubuntu:GNOME", "COSMIC", "sway", ""}) {
            QVERIFY2(!policyForDesktop(desktop).forceClientSide, desktop);
            QVERIFY2(!policyForDesktop(desktop).preferClientSide, desktop);
        }
    }
    void cursorSizeMatchesVisibleLocalArtworkWithoutCountingPadding() {
        QImage remote(56,80,QImage::Format_ARGB32); remote.fill(Qt::transparent);
        for (int y=8;y<48;++y) for (int x=10;x<30;++x) remote.setPixelColor(x,y,Qt::black);
        // Same artwork with different transparent padding must have the same visible size.
        const auto padded=CursorSizePolicy::pixels(remote,2,20.5);
        QCOMPARE(padded,QSize(57,82));
        const auto cropped=remote.copy(CursorSizePolicy::visibleBounds(remote));
        QCOMPARE(CursorSizePolicy::pixels(cropped,2,20.5),QSize(21,41));
        QCOMPARE(CursorSizePolicy::pixels(cropped,1,20),QSize(10,20));
        QCOMPARE(CursorSizePolicy::pixels(cropped,1.5,20),QSize(15,30));
        QCOMPARE(CursorSizePolicy::pixels(cropped,2,40),QSize(40,80));
        QCOMPARE(CursorSizePolicy::pixels(cropped,std::numeric_limits<double>::quiet_NaN(),20),QSize(10,20));
        QImage hidden(20,20,QImage::Format_ARGB32); hidden.fill(Qt::transparent);
        QVERIFY(CursorSizePolicy::pixels(hidden,2,20).isEmpty());
        QVERIFY(CursorSizePolicy::pixels({},2,20).isEmpty());
        // Never exceed the host-cursor buffer bound, even for huge transparent canvases.
        QVERIFY(CursorSizePolicy::pixels(remote,8,128).height()<=256);
    }
    void aSingleFrameNeverReplacesAWorkingConfiguration() {
        PresentationHealth health;
        QCOMPARE(health.observe(100, 1, 100, false, false), quint32(0));
        QCOMPARE(health.observe(30100, 1, 100, false, false), quint32(0));
        QCOMPARE(health.observe(30200, 2, 30200, false, false), quint32(0));
        QCOMPARE(health.observe(61200, 3, 61200, false, false), quint32(0));
        QCOMPARE(health.observe(64000, 3, 61200, false, false), quint32(0));
    }
    void healthyStaticCaptureAndStoppedCaptureAreDifferent() {
        PresentationHealth health;
        for (quint32 now=100;now<=30100;now+=1000)
            QCOMPARE(health.observe(now, 2+now/1000, now, true, true), now-100);
        QCOMPARE(health.observe(30200, 1, 100, true, false), quint32(0));
        QCOMPARE(health.observe(30300, 34, 30300, true, true), quint32(0));
        QCOMPARE(health.observe(33300,34,30300,true,true),quint32(0));
    }
#if SDL_VERSION_ATLEAST(2, 0, 18)
    void actualPanUsesThePackagedPreciseOnlyWheelShape() {
        SDL_MouseWheelEvent event {};
        event.preciseX = -2; event.preciseY = -3;
        ViewScalePolicy::WheelPan wheel;
        QCOMPARE(wheel.take(event), QPoint(-160,-240));
        // The SDL2-compat poll result has x/y == 0, so those cannot supply pan.
        QCOMPARE(event.x,0); QCOMPARE(event.y,0);
    }
    void fractionalPanStaysAlignedWithRenderedPixelsAndDpr2Input() {
        SDL_MouseWheelEvent event {};
        event.preciseX = .25f; event.preciseY = -.5f;
        ViewScalePolicy::WheelPan wheel;
        const auto pan = ViewScalePolicy::pan(QSize(2880,1800),QSize(1600,1000),{},wheel.take(event));
        QCOMPARE(pan,QPoint(20,-40));
        const auto pixels = ViewScalePolicy::destination(QSize(2880,1800),QRect(0,0,1600,1000),true,1,pan);
        const auto points = ViewScalePolicy::destination(QSize(2880,1800),QRect(0,0,800,500),true,2,pan/2);
        QCOMPARE(pixels,QRect(-620,-440,2880,1800));
        QCOMPARE(points,QRect(-310,-220,1440,900));
        const QPointF local(400,250);
        QCOMPARE((local.x()-points.x())*2,local.x()*2-pixels.x());
        QCOMPARE((local.y()-points.y())*2,local.y()*2-pixels.y());
    }
    void tinyPanRetainsResidualAndFlippedEventsReverseBothAxes() {
        SDL_MouseWheelEvent event {};
        event.preciseX = .004f; event.preciseY = -.004f;
        ViewScalePolicy::WheelPan wheel;
        QPoint total;
        for(int i=0;i<100;++i) total += wheel.take(event);
        QCOMPARE(total,QPoint(32,-32));
        event.direction = SDL_MOUSEWHEEL_FLIPPED;
        for(int i=0;i<100;++i) total += wheel.take(event);
        QCOMPARE(total,QPoint());
        wheel.reset();
        QCOMPARE(wheel.take(event),QPoint());
        event.preciseX = 1; event.preciseY = -1;
        QCOMPARE(wheel.take(event),QPoint(-80,80));
    }
#endif
    void legacyWheelFallbackIsPerAxisAndPreciseValuesTakePrecedence() {
        SDL_MouseWheelEvent event {};
        event.x=2; event.y=-3;
        ViewScalePolicy::WheelPan wheel;
        QCOMPARE(wheel.take(event),QPoint(160,-240));
#if SDL_VERSION_ATLEAST(2, 0, 18)
        event.preciseX=.5f;
        QCOMPARE(wheel.take(event),QPoint(40,-240));
        event.direction=SDL_MOUSEWHEEL_FLIPPED;
        QCOMPARE(wheel.take(event),QPoint(-40,240));
#else
        event.direction=SDL_MOUSEWHEEL_FLIPPED;
        QCOMPARE(wheel.take(event),QPoint(-160,240));
#endif
    }
    void actualViewCannotAccumulateInvisibleOverscroll() {
        const QSize video(1001,701), view(800,500);
        auto pan=ViewScalePolicy::pan(video,view,{},QPoint(-8000,8000));
        QCOMPARE(pan,QPoint(-101,100));
        QCOMPARE(ViewScalePolicy::pan(video,view,pan,QPoint(1,-1)),QPoint(-100,99));
        QCOMPARE(ViewScalePolicy::destination(video,QRect(QPoint(),view),true,1,pan).left(),-201);
        QCOMPARE(ViewScalePolicy::pan(QSize(400,300),view,{},QPoint(10,10)),QPoint());
    }
    void actualPixelSizeSharesTheSameCenteredMappingAtHiDpi() {
        const auto pixels = ViewScalePolicy::destination(QSize(2880,1800), QRect(0,0,1600,1000), true, 1.0);
        const auto points = ViewScalePolicy::destination(QSize(2880,1800), QRect(0,0,800,500), true, 2.0);
        QCOMPARE(pixels, QRect(-640,-400,2880,1800));
        QCOMPARE(points, QRect(-320,-200,1440,900));
        QCOMPARE((400.0-points.x())/points.width(), 0.5);
        QCOMPARE(ViewScalePolicy::destination(QSize(2880,1800),QRect(0,0,800,500),false,2.0),QRect(0,0,800,500));
    }
    void desktopScaleFollowsLogicalVideoViewport_data() {
        QTest::addColumn<QString>("preference");
        QTest::addColumn<QSize>("video");
        QTest::addColumn<QSizeF>("viewport");
        QTest::addColumn<QString>("expected");
        QTest::newRow("4k-unscaled-fullscreen") << QString("auto") << QSize(3840,2160) << QSizeF(3840,2160) << QString("1");
        QTest::newRow("4k-retina-fullscreen") << QString("auto") << QSize(3840,2160) << QSizeF(1920,1080) << QString("2");
        QTest::newRow("native-retina-maximized") << QString("auto") << QSize(2880,1800) << QSizeF(1440,834) << QString("2");
        QTest::newRow("small-retina-window") << QString("auto") << QSize(1920,1200) << QSizeF(800,600) << QString("2");
        // Pillarboxing reduces the video width to 1920 logical pixels here.
        QTest::newRow("ultrawide-letterbox") << QString("auto") << QSize(3840,2160) << QSizeF(3840,1080) << QString("2");
        QTest::newRow("reduced-resolution-retina") << QString("auto") << QSize(1920,1200) << QSizeF(1440,834) << QString("1");
        QTest::newRow("explicit-1x") << QString("1") << QSize(3840,2160) << QSizeF(1920,1080) << QString("1");
        QTest::newRow("explicit-2x") << QString("2") << QSize(1920,1080) << QSizeF(1920,1080) << QString("2");
        QTest::newRow("unknown-viewport") << QString("auto") << QSize(3840,2160) << QSizeF() << QString("auto");
        QTest::newRow("invalid-video") << QString("auto") << QSize() << QSizeF(1920,1080) << QString("auto");
        QTest::newRow("negative-viewport") << QString("auto") << QSize(3840,2160) << QSizeF(1920,-1) << QString("auto");
        QTest::newRow("nonfinite-viewport") << QString("auto") << QSize(3840,2160) << QSizeF(std::numeric_limits<double>::infinity(),1080) << QString("auto");
        QTest::newRow("below-scale-midpoint") << QString("auto") << QSize(1920,1080) << QSizeF(1281,721) << QString("1");
        QTest::newRow("scale-midpoint-prefers-readable") << QString("auto") << QSize(1920,1080) << QSizeF(1280,720) << QString("2");
        QTest::newRow("odd-resolution") << QString("auto") << QSize(2881,1800) << QSizeF(1440,900) << QString("1");
    }
    void desktopScaleFollowsLogicalVideoViewport() {
        QFETCH(QString, preference);
        QFETCH(QSize, video);
        QFETCH(QSizeF, viewport);
        QFETCH(QString, expected);
        QCOMPARE(DesktopScalePolicy::forViewport(preference, video, viewport), expected);
    }
    void transientDisconnectsOnlyRetryEstablishedWork() {
        for (int error : {-1, ML_ERROR_NO_VIDEO_TRAFFIC, ML_ERROR_NO_VIDEO_FRAME, 104, 10054}) {
            QVERIFY(SessionRetryPolicy::retryTermination(error, true));
            QVERIFY(!SessionRetryPolicy::retryTermination(error, false));
        }
        for (int error : {ML_ERROR_GRACEFUL_TERMINATION, ML_ERROR_PROTECTED_CONTENT,
                          ML_ERROR_FRAME_CONVERSION, ML_ERROR_UNEXPECTED_EARLY_TERMINATION})
            QVERIFY(!SessionRetryPolicy::retryTermination(error, true));
        QVERIFY(SessionRetryPolicy::retryStage(STAGE_NAME_RESOLUTION));
        QVERIFY(SessionRetryPolicy::retryStage(STAGE_RTSP_HANDSHAKE));
        QVERIFY(!SessionRetryPolicy::retryStage(STAGE_PLATFORM_INIT));
        QVERIFY(!SessionRetryPolicy::retryStage(STAGE_AUDIO_STREAM_INIT));
    }
    void retryNeverLaunchesOrQuitsAnotherRemoteApp() {
        QVERIFY(SessionRetryPolicy::sameTarget(42, 42));
        QVERIFY(!SessionRetryPolicy::sameTarget(0, 42));
        QVERIFY(!SessionRetryPolicy::sameTarget(7, 42));
        QVERIFY(!SessionRetryPolicy::sameTarget(0, 0));
    }
    void textModeSendsCommittedTextInsteadOfPhysicalLetters() {
        LocalTextInputPolicy policy;
        policy.setEnabled(true);
        QVERIFY(policy.suppress(SDL_SCANCODE_A, SDLK_a, KMOD_NONE, true));
        QVERIFY(policy.suppress(SDL_SCANCODE_A, SDLK_a, KMOD_CTRL, false));
        QVERIFY(policy.suppress(SDL_SCANCODE_E, SDLK_e, KMOD_SHIFT, true));
        QVERIFY(policy.suppress(SDL_SCANCODE_E, SDLK_e, KMOD_NONE, false));
        QVERIFY(!policy.suppress(SDL_SCANCODE_LEFT, SDLK_LEFT, KMOD_NONE, true));
        QVERIFY(!policy.suppress(SDL_SCANCODE_LEFT, SDLK_LEFT, KMOD_NONE, false));
    }
    void shortcutsAndAltGrKeepTheirDistinctSemantics() {
        LocalTextInputPolicy policy;
        policy.setEnabled(true);
        QVERIFY(!policy.suppress(SDL_SCANCODE_C, SDLK_c, KMOD_CTRL, true));
        QVERIFY(!policy.suppress(SDL_SCANCODE_C, SDLK_c, KMOD_NONE, false));
        QVERIFY(!policy.suppress(SDL_SCANCODE_V, SDLK_v, KMOD_GUI, true));
        QVERIFY(policy.suppress(SDL_SCANCODE_Q, SDLK_q, KMOD_CTRL | KMOD_ALT | KMOD_MODE, true));
        // A suppressed down stays suppressed when mode/modifiers change before its up.
        policy.setEnabled(false);
        QVERIFY(policy.suppress(SDL_SCANCODE_Q, SDLK_q, KMOD_NONE, false));
        QVERIFY(!policy.suppress(SDL_SCANCODE_Q, SDLK_q, KMOD_NONE, true));
        policy.setEnabled(true);
        QVERIFY(policy.suppress(SDL_SCANCODE_B, SDLK_b, KMOD_NONE, true));
        policy.focusLost();
        QVERIFY(!policy.suppress(SDL_SCANCODE_B, SDLK_b, KMOD_CTRL, false));
    }
    void compositionEditingNeverEditsTheRemoteDocument() {
        LocalTextInputPolicy policy;
        policy.setEnabled(true);
        const SDL_Scancode keys[] = {SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_DELETE,
            SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
            SDL_SCANCODE_HOME, SDL_SCANCODE_END, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_TAB};
        for (auto key : keys) {
            policy.setComposing(true);
            QVERIFY(policy.suppress(key, SDL_GetKeyFromScancode(key), KMOD_NONE, true));
            // Commit/cancel can clear preedit before the matching up arrives.
            policy.setComposing(false);
            QVERIFY(policy.suppress(key, SDL_GetKeyFromScancode(key), KMOD_NONE, false));
            QVERIFY(!policy.suppress(key, SDL_GetKeyFromScancode(key), KMOD_NONE, true));
        }
        policy.setComposing(true);
        QVERIFY(!policy.suppress(SDL_SCANCODE_C, SDLK_c, KMOD_CTRL, true));
        policy.focusLost();
        QVERIFY(!policy.suppress(SDL_SCANCODE_LEFT, SDLK_LEFT, KMOD_NONE, true));
    }
};
QTEST_GUILESS_MAIN(SessionRuntimeTest)
#include "tst_sessionruntime.moc"
