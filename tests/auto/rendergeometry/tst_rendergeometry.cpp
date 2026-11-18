#include <QtTest>
#include "streaming/video/rendergeometry.h"
#include "streaming/viewscalepolicy.h"

class RenderGeometryTest : public QObject {
    Q_OBJECT
private slots:
    void actualPanKeepsRenderedPixelsAndAbsoluteInputAligned() {
        const QSize video(2880, 1800);
        const auto pixels = ViewScalePolicy::destination(video, QRect(0, 0, 1600, 1000), true, 1, QPoint(160, 80));
        const auto points = ViewScalePolicy::destination(video, QRect(0, 0, 800, 500), true, 2, QPoint(80, 40));
        const SDL_Rect destination {pixels.x(), pixels.y(), pixels.width(), pixels.height()};
        const auto gl = RenderGeometry::lowerLeft(destination, 1000);
        const QPointF localPoint(400, 250);
        const QPointF outputPixel = localPoint * 2;
        const double inputX = (localPoint.x() - points.x()) * video.width() / points.width();
        const double inputY = (localPoint.y() - points.y()) * video.height() / points.height();
        // The GPU's origin is lower-left and its video texture's origin is top-left.
        QCOMPARE(inputX, outputPixel.x() - gl.x);
        QCOMPARE(inputY, video.height() - ((1000 - outputPixel.y()) - gl.y));
        const auto visible = RenderGeometry::clip({0, 0, video.width(), video.height()}, destination, {0, 0, 1600, 1000});
        QVERIFY(!visible.empty());
        QCOMPARE(visible.destination.x, 0);
        QCOMPARE(visible.destination.y, 0);
        QCOMPARE(inputX, visible.source.x() + outputPixel.x());
        QCOMPARE(inputY, visible.source.y() + outputPixel.y());
        const auto integer = RenderGeometry::integerSource(visible);
        QCOMPARE(integer.x, int(visible.source.x()));
        QCOMPARE(integer.y, int(visible.source.y()));
        QVERIFY(integer.x >= 0 && integer.y >= 0);
        QVERIFY(integer.x + integer.w <= video.width());
        QVERIFY(integer.y + integer.h <= video.height());
    }
    void changingPanInvalidatesSourceEvenWhenOutputRectangleIsUnchanged() {
        const SDL_Rect video {0, 0, 2880, 1800}, viewport {0, 0, 1600, 1000};
        const auto before = RenderGeometry::clip(video, {-640, -400, 2880, 1800}, viewport);
        const auto after = RenderGeometry::clip(video, {-640, -320, 2880, 1800}, viewport);
        QVERIFY(RenderGeometry::same(before.destination, after.destination));
        QVERIFY(before.source != after.source);
        QCOMPARE(after.source.y(), qreal(320));
    }
    void clippingPreservesScaledSourceOffsetsAndLetterboxMargins() {
        const auto clipped = RenderGeometry::clip({100, 200, 1600, 1000}, {-100, -50, 800, 500}, {0, 0, 600, 400});
        QCOMPARE(clipped.source, QRectF(300, 300, 1200, 800));
        QVERIFY(RenderGeometry::same(clipped.destination, {0, 0, 600, 400}));
        const auto fit = RenderGeometry::clip({0, 0, 1920, 1080}, {0, 50, 800, 450}, {0, 0, 800, 550});
        QCOMPARE(fit.source, QRectF(0, 0, 1920, 1080));
        QVERIFY(RenderGeometry::same(fit.destination, {0, 50, 800, 450}));
        const auto gl = RenderGeometry::lowerLeft(fit.destination, 550);
        QVERIFY(RenderGeometry::same(gl, fit.destination));
        QVERIFY(RenderGeometry::clip({0, 0, 10, 10}, {-20, -20, 10, 10}, {0, 0, 100, 100}).empty());
    }
};
QTEST_GUILESS_MAIN(RenderGeometryTest)
#include "tst_rendergeometry.moc"
