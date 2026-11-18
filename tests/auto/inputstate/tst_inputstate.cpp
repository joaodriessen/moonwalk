#include <QtTest>
#include <limits>
#include "../../../app/streaming/input/forwardedmousebuttons.h"
#include "../../../app/streaming/input/wheelaccumulator.h"
#include "../../../app/backend/audiolayoutpolicy.h"

class InputStateTest : public QObject {
    Q_OBJECT
private slots:
    void captureLossBalancesOnlyForwardedButtons() {
        ForwardedMouseButtons buttons;
        QList<int> releases;
        buttons.pressed(1); buttons.pressed(3); buttons.pressed(5);
        buttons.releaseAll([&](int b) { releases.append(b); });
        QCOMPARE(releases, QList<int>({1, 3, 5}));
        QVERIFY(!buttons.released(1)); // A late physical up must not emit another release.
        buttons.releaseAll([&](int b) { releases.append(b); });
        QCOMPARE(releases.size(), 3);
        QVERIFY(!buttons.released(2)); // Never release an emulated/unforwarded press.
        buttons.pressed(3); // Swapped physical left uses its effective host button.
        QVERIFY(buttons.released(3));
        QVERIFY(!buttons.released(3));
    }
    void smoothScrollRetainsBothAxesAndDirection() {
        WheelAccumulator vertical, horizontal;
        int v = 0, h = 0;
        for (int i = 0; i < 100; ++i) {
            v += vertical.take(.004f); h += horizontal.take(-.004f);
        }
        QCOMPARE(v, 48); QCOMPARE(h, -48);
        WheelAccumulator alternating;
        int total = 0;
        for (int i = 0; i < 100; ++i) {
            total += alternating.take(.004f); total += alternating.take(-.004f);
        }
        QCOMPARE(total, 0);
    }
    void scrollRejectsInvalidValuesAndBoundsOutliers() {
        WheelAccumulator wheel;
        QCOMPARE(wheel.take(std::numeric_limits<float>::quiet_NaN()), short(0));
        QCOMPARE(wheel.take(std::numeric_limits<float>::infinity()), short(0));
        QCOMPARE(wheel.take(std::numeric_limits<float>::max()), std::numeric_limits<short>::max());
        QCOMPARE(wheel.take(-std::numeric_limits<float>::max()), std::numeric_limits<short>::min());
        QCOMPARE(wheel.take(1), short(120));
    }
    void hostAudioLimitDoesNotRewriteRequestedPreferences() {
        QCOMPARE(AudioLayoutPolicy::effectiveChannels(8, 2), 2);
        QCOMPARE(AudioLayoutPolicy::effectiveChannels(6, 2), 2);
        QCOMPARE(AudioLayoutPolicy::effectiveChannels(8, 6), 6);
        QCOMPARE(AudioLayoutPolicy::effectiveChannels(6, 8), 6);
        QCOMPARE(AudioLayoutPolicy::effectiveChannels(8, 0), 8);
        QCOMPARE(AudioLayoutPolicy::parseChannelLimit("2"), 2);
        QCOMPARE(AudioLayoutPolicy::parseChannelLimit("8"), 8);
        for (auto text : {"", "bad", "-2", "4", "2junk"})
            QCOMPARE(AudioLayoutPolicy::parseChannelLimit(text), 0);
    }
};
QTEST_GUILESS_MAIN(InputStateTest)
#include "tst_inputstate.moc"
