#include <QtTest>
#include <QTemporaryDir>
#include "streaming/automaticpacing.h"
#include "streaming/hostlatencymeter.h"
#include "streaming/streamplan.h"
#include "streaming/streamrecommendation.h"
#include "streaming/video/ffmpeg-renderers/presentationmonitor.h"

namespace {

constexpr qint64 Now = 1793800000; // 4 Nov 2026

// This MacBook: a 2880×1800 60 Hz panel that decodes only H.264 in hardware, and a
// Vibepollo host that sends H.264 and HEVC.
StreamPlanInput macBook(const QString& host)
{
    StreamPlanInput input;
    input.nativeWidth = 2880;
    input.nativeHeight = 1800;
    input.displayHz = 60;
    input.h264 = true;
    input.serverCodecModeSupport = SCM_H264 | SCM_HEVC;
    input.hostName = host;
    input.now = Now;
    return input;
}

HostCapacity measured(int width, int height, int fps, double averageMs, double p95Ms)
{
    HostCapacity capacity;
    StreamPlanning::remember(capacity, width, height, fps, QStringLiteral("H.264"), averageMs, p95Ms, Now - 3600, qMin(double(fps), 1000 / averageMs), true);
    return capacity;
}

// A host whose encoder holds up to a fixed number of pixels a second and then falls
// behind, so the time per frame jumps rather than growing in step with the size.
struct CliffHost {
    double pixelsPerSecond;
    double loadAtLimit;
    double averageMs(const StreamPlan& plan) const
    {
        const double rate = double(plan.width) * plan.height * plan.fps;
        const double interval = 1000.0 / plan.fps;
        return rate > pixelsPerSecond ? 3 * interval : loadAtLimit * interval * rate / pixelsPerSecond;
    }
};

} // namespace

class StreamPlanTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_Settings;

private slots:
    void deferredRecommendationIsScopedExpiresAndPreservesItsBaseline() {
        StreamRecommendation offer {{2520,1576,60}, measured(2880,1800,60,40,55), Now, false};
        offer.baseline.performanceContext = "host/build/route/display";
        const auto rungs = StreamPlanning::ladder(2880,1800,60,false,2);
        QVERIFY(offer.valid(rungs, offer.baseline.performanceContext, "H.264", Now));
        QVERIFY(!offer.valid(rungs, "different route", "H.264", Now));
        QVERIFY(!offer.valid(rungs, offer.baseline.performanceContext, "HEVC", Now));
        QVERIFY(!offer.valid(rungs, offer.baseline.performanceContext, "H.264", Now-1));
        QVERIFY(!offer.valid(rungs, offer.baseline.performanceContext, "H.264", Now+StreamPlanning::OverloadMemorySeconds+1));
        QVERIFY(!offer.valid(StreamPlanning::ladder(1920,1080,60,false), offer.baseline.performanceContext, "H.264", Now));
        offer.save("deferred-test-host", offer.baseline.performanceContext, "H.264");
        // A newer rolling measurement must not replace the trial's baseline.
        measured(2160,1350,60,50,60).save("deferred-test-host");
        auto saved = StreamRecommendation::load("deferred-test-host", offer.baseline.performanceContext, "H.264");
        QCOMPARE(saved.target.width,2520);
        QCOMPARE(saved.baseline.width,2880);
        QCOMPARE(saved.baseline.cadenceFps(),offer.baseline.cadenceFps());
        QVERIFY(saved.valid(rungs, offer.baseline.performanceContext,"H.264",Now));
        QVERIFY(!StreamRecommendation::load("another-host", offer.baseline.performanceContext,"H.264").createdAt);
        QVERIFY(!StreamRecommendation::load("deferred-test-host", "another display","H.264").createdAt);
        StreamRecommendation::clear("deferred-test-host", offer.baseline.performanceContext,"H.264");
        QVERIFY(!StreamRecommendation::load("deferred-test-host", offer.baseline.performanceContext,"H.264").createdAt);
        QCOMPARE(HostCapacity::load("deferred-test-host").width,2160);
    }
    void deferredQualityKeepsThePictureAndRoundTripsItsTarget() {
        auto baseline = measured(2880,1800,60,40,55);
        baseline.performanceContext = "quality-context";
        StreamRecommendation quality {{2880,1800,60},baseline,Now,false,8000};
        const auto rungs = StreamPlanning::ladder(2880,1800,60,false,2);
        QVERIFY(quality.valid(rungs,"quality-context","H.264",Now));
        quality.save("quality-test-host","quality-context","H.264");
        const auto saved=StreamRecommendation::load("quality-test-host","quality-context","H.264");
        QCOMPARE(saved.target.width,2880);QCOMPARE(saved.bitrateKbps,8000);
        QVERIFY(saved.valid(rungs,"quality-context","H.264",Now));
        quality.bitrateKbps=0;QVERIFY(!quality.valid(rungs,"quality-context","H.264",Now));
        quality.bitrateKbps=1;QVERIFY(!quality.valid(rungs,"quality-context","H.264",Now));
        quality.bitrateKbps=8000;quality.target.width=2520;quality.target.height=1576;
        QVERIFY(!quality.valid(rungs,"quality-context","H.264",Now));
        StreamRecommendation::clear("quality-test-host","quality-context","H.264");
    }
    void failedTrialCanQueueItsPreviousPictureWithoutAnotherAutomaticTrial() {
        auto baseline = measured(2880,1800,60,40,55);
        baseline.performanceContext = "same-context";
        StreamRecommendation undo {{2880,1800,60},baseline,Now,true};
        const auto rungs = StreamPlanning::ladder(2880,1800,60,false,2);
        QVERIFY(undo.valid(rungs,"same-context","H.264",Now));
        undo.rollback=false;QVERIFY(!undo.valid(rungs,"same-context","H.264",Now));
        undo.rollback=true;undo.target.fps=30;
        QVERIFY(!undo.valid(rungs,"same-context","H.264",Now));
    }
    void capturePressureBeforeTheTimestampCanStillTryASmallerPicture() {
        HostLatencyMeter meter;
        for(int i=0;i<180;++i) meter.addFrame(1000000+quint64(i)*28571,150,20000,i+1);
        const auto evidence=StreamPlanning::adaptationEvidence(meter,2160,1350,60,false,0,false,true);
        QVERIFY(evidence.active);
        QVERIFY(evidence.slow);
        QVERIFY(evidence.canTrySize);
    }
    void identicalModesAndFrameRateChangesCannotSpendAReconnect() {
        StreamAdaptation episode;
        const auto baseline = measured(2880,1800,60,35,50);
        QVERIFY(!episode.request({2880,1800,60},{2880,1800,60},baseline,1000));
        QVERIFY(!episode.request({2520,1576,30},{2880,1800,60},baseline,1000));
        QCOMPARE(episode.transitions,0);
        QVERIFY(!episode.pending);
    }
    void unqualifiedRollbackIsExplicitAndCannotLoop() {
        StreamAdaptation first;
        auto baseline = measured(2520,1576,60,25.637,39.5);
        baseline.deliveredFps = 32.135;
        baseline.demandObserved = false;
        QVERIFY(first.request({2160,1350,60},{2520,1576,60},baseline,1000));
        auto successor = first;
        QVERIFY(successor.presented({2160,1350,60},6000));
        QCOMPARE(successor.interruptedMs,qint64(5000));
        QVERIFY(successor.restore({2160,1350,60},16000));
        auto restored = successor;
        QCOMPARE(restored.target.width,2520);
        QVERIFY(!restored.baseline.demandObserved);
        QVERIFY(restored.presented({2520,1576,60},22000));
        QVERIFY(restored.locked);
        QVERIFY(!restored.pending);
        QVERIFY(!restored.checkTrial);
        QVERIFY(!restored.restore({2520,1576,60},23000));
        QVERIFY(!restored.request({2160,1350,60},{2520,1576,60},baseline,24000));
    }
    void ignoredModeStopsTuningWithoutRepeatingTheRequest() {
        StreamAdaptation episode;
        QVERIFY(episode.request({2520,1576,60},{2880,1800,60},measured(2880,1800,60,35,50),1000));
        QVERIFY(!episode.presented({2880,1800,60},6000));
        QVERIFY(episode.locked);
        QVERIFY(!episode.checkTrial);
        QVERIFY(!episode.restore({2880,1800,60},16000));
    }
    void disruptionBudgetStillReservesOneUndo() {
        StreamAdaptation episode;
        const auto sample = measured(2520,1576,60,25,40);
        QVERIFY(episode.request({2160,1350,60},{2520,1576,60},sample,1000));
        QVERIFY(episode.presented({2160,1350,60},32000));
        episode.checkTrial = false;
        QVERIFY(!episode.request({1800,1126,60},{2160,1350,60},sample,42000));
        QVERIFY(episode.restore({2160,1350,60},42000));
        QVERIFY(episode.presented({2520,1576,60},48000));
        QVERIFY(!episode.restore({2160,1350,60},58000));
    }
    void qualityTrialsAreSharedAcrossModesAndRespectRebuildCost() {
        StreamAdaptation first;
        QVERIFY(first.mayTryQuality(HostCapacity::BitrateChange::Unknown));
        ++first.qualityTrials;
        auto nextMode = first;
        QVERIFY(!nextMode.mayTryQuality(HostCapacity::BitrateChange::Unknown));
        QVERIFY(!nextMode.mayTryQuality(HostCapacity::BitrateChange::Rebuild));
        QVERIFY(nextMode.mayTryQuality(HostCapacity::BitrateChange::Live));
        ++nextMode.qualityTrials;
        auto thirdMode = nextMode;
        QVERIFY(!thirdMode.mayTryQuality(HostCapacity::BitrateChange::Live));
    }
    void aLiveTrialSurvivesAnUnqualifiedSavedMeasurement() {
        auto input = macBook("Any host");
        input.displayScale = 2;
        input.capacity = measured(2520,1576,60,25.637,39.5);
        input.capacity.sourceFps = input.capacity.deliveredFps = 32.135;
        input.capacity.demandObserved = false; // Actual field record after queue pressure.
        input.trialWidth = 2160; input.trialHeight = 1350;
        const auto plan = planStream(input);
        QCOMPARE(QSize(plan.width,plan.height),QSize(2160,1350));
        QCOMPARE(plan.fps,60);
    }
    void rollbackDoesNotDependOnQualificationOfItsBaseline() {
        auto input = macBook("Any host");
        input.displayScale = 2;
        input.capacity = measured(2520,1576,60,25,39);
        input.capacity.demandObserved = false;
        input.trialWidth = 2520; input.trialHeight = 1576;
        QCOMPARE(planStream(input).width,2520);
        input.width = 1920; input.height = 1080; // Manual remains authoritative.
        QCOMPARE(planStream(input).width,1920);
    }
    void repeatedUntimedFramesAreNotMisclassifiedAsMissingVideo() {
        HostLatencyMeter meter;
        for (int i=0;i<400;++i) meter.addFrame(1000000+quint64(i)*16667,i%2 ? 0:400,20000,i+1);
        const auto observed=StreamPlanning::adaptationEvidence(meter,2880,1800,60,false,0,false,true);
        QVERIFY(qAbs(observed.deliveredFps-60)<.1);
        QVERIFY(!observed.slow);
        QCOMPARE(meter.averageMs(),40.0);
    }
    void deliveryLossGetsAQualityTrialEvenWhenTheHostProducesFullCadence() {
        HostLatencyMeter meter;
        for(int i=0;i<120;++i) meter.addFrame(1000000+quint64(i)*66667,100,20000,i*4+1);
        const auto observed=StreamPlanning::adaptationEvidence(meter,2880,1800,60,false,0,false,false);
        QVERIFY(observed.slow);
        QVERIFY(!observed.canTrySize); // First reduce bytes; host pipeline is fast.
    }
    void legacyHostSlowActiveVideoCanOptimizeWithoutCaptureExtension() {
        HostLatencyMeter meter;
        for (int i=0;i<120;++i) meter.addFrame(1000000+quint64(i)*66667,420,20000,i+1);
        const auto observed=StreamPlanning::adaptationEvidence(meter,2880,1800,60,false,0,false,true);
        QVERIFY(observed.active);
        QVERIFY(observed.slow);
        QVERIFY(observed.canTrySize);
    }
    void highDelayAtFullCadenceAndSparseIdleVideoDoNotAdapt() {
        HostLatencyMeter full,idle;
        for(int i=0;i<400;++i) full.addFrame(1000000+quint64(i)*16667,400,20000,i+1);
        for(int i=0;i<100;++i) idle.addFrame(1000000+quint64(i)*1000000,400,200,i+1);
        QVERIFY(!StreamPlanning::adaptationEvidence(full,2880,1800,60,false,0,false,true).slow);
        QVERIFY(!StreamPlanning::adaptationEvidence(idle,2880,1800,60,false,0,false,true).slow);
    }
    void slowCaptureWithHostPressureCanOptimizeButStaleCaptureCannot() {
        HostLatencyMeter meter;
        for (int i = 0; i < 120; ++i) meter.addFrame(1000000 + quint64(i) * 66667, 420, 20000, i + 1);
        QVERIFY(StreamPlanning::adaptationEvidence(meter,2880,1800,60,true,15,true,true).canTrySize);
        QVERIFY(!StreamPlanning::adaptationEvidence(meter,2880,1800,60,true,15,false,true).active);
        QVERIFY(!StreamPlanning::adaptationEvidence(meter,2880,1800,60,true,0,true,true).active);
    }
    void severePressureTriesOnlyTheNextScaleStep() {
        auto input = macBook("Generic host");
        input.displayScale = 2;
        input.capacity = measured(2880, 1800, 60, 42, 80);
        input.capacity.sourceFps = 15;
        input.capacity.deliveredFps = 15;
        input.capacity.demandObserved = true;
        const auto plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2520, 1576));
        QCOMPARE(plan.fps, 60);
    }
    void unscaledDisplaysAlsoTrySmallResolutionSteps() {
        auto input = macBook("Generic host");
        input.nativeWidth = 1920;
        input.nativeHeight = 1080;
        input.displayScale = 1;
        input.capacity = measured(1920, 1080, 60, 42, 80);
        const auto plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(1680, 946));
        QCOMPARE(plan.fps, 60);
    }
    void ineffectiveSecondCutRestoresThePreviousWorkingMode() {
        auto input = macBook("Generic host");
        input.displayScale = 2;
        input.capacity = measured(2520, 1576, 60, 30, 45);
        input.capacity.reductionRejectedUntil = Now + 86400;
        const auto plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2520, 1576));
        QCOMPARE(plan.fps, 60);
    }
    void automaticNeverTradesAwayDisplayCadenceOrLogicalDesktop() {
        const auto rungs = StreamPlanning::ladder(2880, 1800, 60, true, 2);
        for (const auto& rung : rungs) {
            QCOMPARE(rung.fps, 60);
            QVERIFY(rung.width >= 1440);
            QVERIFY(rung.height >= 900);
        }
    }
    void initTestCase()
    {
        QVERIFY(m_Settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_Settings.path());
    }

    void ineffectiveReductionReturnsToNativeAndHasABoundedCooldown()
    {
        HostCapacity before;
        before.sourceFps = 40; before.hostLatencyAverageMs = 30;
        QVERIFY(!StreamPlanning::reductionHelped(before, 40, 29));
        QVERIFY(StreamPlanning::reductionHelped(before, 55, 29));
        QVERIFY(StreamPlanning::reductionHelped(before, 40, 20));
        auto in = macBook("Mini PC");
        StreamPlanning::remember(in.capacity, 2880, 1800, 60, "H.264", 30, 40, in.now, 40, true);
        in.capacity.reductionRejectedUntil = in.now + 3600;
        QCOMPARE(planStream(in).width, 2880);
        in.now += 3601;
        QVERIFY(planStream(in).width < 2880);
    }

    void unmeasuredHostGetsTheDisplay()
    {
        const StreamPlan plan = planStream(macBook("Mini PC"));
        QCOMPARE(QSize(plan.width, plan.height), QSize(2880, 1800));
        QCOMPARE(plan.fps, 60);
        QVERIFY(!plan.limitedByHost);
        QVERIFY(plan.limitReason.isEmpty());
    }

    void sourceCadenceDoesNotBlameTheHostForDecoderSkips()
    {
        HostLatencyMeter meter;
        // The decoder samples every other frame of a 60fps source. Its own
        // throughput is not evidence that the host only produces 30fps.
        for (int n = 0; n <= 300; n += 2)
            meter.addFrame(quint64(n) * 16667, 400, 20000, quint32(n));
        QVERIFY(qAbs(meter.sourceFps() - 60) < 0.1);
        QVERIFY(meter.demandObserved(2880,1800));
        HostLatencyMeter idle;
        for (int n = 0; n <= 100; ++n)
            idle.addFrame(quint64(n) * 100000, 400, 100, quint32(n));
        QVERIFY(!idle.demandObserved(2880,1800));
    }

    void pipelineLatencyIsNotThroughput()
    {
        auto input = macBook("Pipelined host");
        input.capacity = measured(2880, 1800, 60, 40, 50);
        input.capacity.sourceFps = input.capacity.deliveredFps = 60;
        input.capacity.demandObserved = true;
        QCOMPARE(planStream(input).width, 2880);
        QVERIFY(!planStream(input).limitedByHost);
    }

    void unsupportedScaleStepsNeverChangeTheLogicalDesktop()
    {
        auto input = macBook("Mac");
        input.displayScale = 2;
        input.integerDesktopScale = true;
        input.capacity = measured(2880, 1800, 60, 40, 50);
        input.capacity.sourceFps = 25;
        input.capacity.demandObserved = true;
        const auto plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(1440, 900));
        QCOMPARE(plan.fps, 60);
    }

    void unqualifiedOldSamplesCannotReduceNativeResolution()
    {
        auto input = macBook("Host");
        input.capacity = measured(2880, 1800, 60, 40, 50);
        input.capacity.sourceFps = 25;
        input.capacity.demandObserved = true;
        input.capacity.measuredAt = Now - 8 * 86400;
        QCOMPARE(planStream(input).width, 2880);
        input.capacity.measuredAt = Now;
        input.capacity.codec = "HEVC";
        QCOMPARE(planStream(input).width, 2880);
        input.capacity.codec = "H.264";
        input.capacity.demandObserved = false;
        QCOMPARE(planStream(input).width, 2880);
        input.capacity.demandObserved = true;
        input.capacity.performanceContext = "host build A";
        input.performanceContext = "host build B";
        QCOMPARE(planStream(input).width, 2880);
        input.performanceContext = "host build A";
        QVERIFY(planStream(input).width < 2880);
    }

    // Synthetic serial encoder: declared source cadence falls below the requested FPS.
    void slowHostStepsDownOneSizeAndKeepsTheFrameRate()
    {
        StreamPlanInput input = macBook("Mini PC");
        input.capacity = measured(2880, 1800, 60, 38, 70);
        const StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2520, 1576));
        QCOMPARE(plan.fps, 60);
        QCOMPARE(plan.codec, QString("H.264"));
        QVERIFY(plan.limitedByHost);
        QVERIFY2(plan.limitReason.contains("Mini PC") && plan.limitReason.contains("2520×1576"), qPrintable(plan.limitReason));
        QVERIFY2(plan.startBitrateKbps >= 35000 && plan.startBitrateKbps <= 40000, qPrintable(QString::number(plan.startBitrateKbps)));

        // Healthy cadence at a smaller size permits a bounded larger trial.
        // Processing latency is not evidence the larger mode will fail.
        input.capacity = measured(1920, 1200, 60, 15, 28);
        const StreamPlan next = planStream(input);
        QCOMPARE(QSize(next.width, next.height), QSize(2160, 1350));
        QVERIFY(next.limitedByHost);
    }

    // Gaming PC: 8–11 ms per frame at the same size.
    void fastHostStaysNative()
    {
        StreamPlanInput input = macBook("Gaming PC");
        input.capacity = measured(2880, 1800, 60, 10, 22);
        const StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2880, 1800));
        QCOMPARE(plan.fps, 60);
        QVERIFY(!plan.limitedByHost);
        QVERIFY2(plan.startBitrateKbps >= 45000 && plan.startBitrateKbps <= 55000, qPrintable(QString::number(plan.startBitrateKbps)));
    }

    // This panel runs at 2×: every smaller size is its 1440×900 layout at one of Windows'
    // scale steps, so a host that picks its scale from the size lays out like this desktop.
    void smallerSizesKeepThisDesktopsLayout()
    {
        const auto rungs = StreamPlanning::ladder(2880, 1800, 60, true, 2.0);
        QList<QSize> sizes;
        for (const auto& rung : rungs) sizes << QSize(rung.width, rung.height);
        QCOMPARE(sizes.mid(0, 5), (QList<QSize> {{2880, 1800}, {2520, 1576}, {2160, 1350}, {1800, 1126}, {1440, 900}}));
        QCOMPARE(rungs.last().fps, 60); // Keep the display cadence at every supported size.

        // Synthetic service-limited host: its measured low source cadence selects a trial.
        StreamPlanInput input = macBook("Mini PC");
        input.displayScale = 2.0;
        input.capacity = measured(2880, 1800, 60, 39.4, 50.8);
        StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2520, 1576));
        QCOMPARE(plan.fps, 60);
        // A healthy smaller mode can try the next supported layout step.
        input.capacity = measured(1920, 1200, 60, 12.6, 18.5);
        plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2160, 1350));
        // Gaming PC, quick at full size, keeps it.
        input = macBook("Gaming PC");
        input.displayScale = 2.0;
        input.capacity = measured(2880, 1800, 60, 10, 22);
        QCOMPARE(planStream(input).width, 2880);
    }

    void spikyLatencyWithoutLostCadenceDoesNotReduceResolution()
    {
        StreamPlanInput input = macBook("Mini PC");
        input.capacity = measured(2880, 1800, 60, 14, 40); // p95 above two intervals
        QCOMPARE(planStream(input).width, 2880);
    }

    void roomToSpareStepsBackUp()
    {
        StreamPlanInput input = macBook("Mini PC");
        input.capacity = measured(1440, 900, 60, 4, 7);
        const StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(1920, 1200));
        QVERIFY(plan.limitedByHost);
        // A larger pipeline delay at the same healthy cadence cannot prohibit that trial.
        input.capacity = measured(1440, 900, 60, 7.5, 12);
        QCOMPARE(planStream(input).width, 1920);
    }

    void smallestSizeStillPreservesTheDisplayFrameRate()
    {
        StreamPlanInput input = macBook("Celeron");
        input.capacity = measured(960, 600, 60, 30, 45);
        StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(960, 600));
        QCOMPARE(plan.fps, 60);
        input.capacity = measured(960, 600, 30, 50, 90);
        plan = planStream(input);
        QCOMPARE(plan.fps, 60); // Nothing smaller to try.
    }

    // Launch, measure, plan again: the size settles rather than flipping each time.
    void settlesInsteadOfOscillating()
    {
        const QList<CliffHost> hosts {{120e6, 0.6}, {200e6, 0.8}, {60e6, 0.5}, {400e6, 0.95}};
        for (const CliffHost& host : hosts) {
            StreamPlanInput input = macBook("Cliff");
            QList<int> widths;
            for (int launch = 0; launch < 12; ++launch) {
                input.now = Now + launch * 3600;
                const StreamPlan plan = planStream(input);
                widths << plan.width;
                const double average = host.averageMs(plan);
                StreamPlanning::remember(input.capacity, plan.width, plan.height, plan.fps, plan.codec,
                                         average, average * 1.6, input.now, qMin(double(plan.fps), 1000 / average), true);
            }
            int changes = 0;
            for (int i = 6; i < widths.size(); ++i) changes += widths[i] != widths[i - 1];
            QVERIFY2(changes == 0, qPrintable(QStringLiteral("%1 Mpx/s host: %2").arg(host.pixelsPerSecond / 1e6)
                                                  .arg(QStringList([&] { QStringList s; for (int w : widths) s << QString::number(w); return s; }()).join(' '))));
            // And it settled on something the host keeps up with.
            const double rate = double(widths.last()) * widths.last() * 1800 / 2880 * 60;
            QVERIFY(rate <= host.pixelsPerSecond || widths.last() == 960);
        }
    }

    void failedModeHasABoundedRecoveryInterval()
    {
        StreamPlanInput input = macBook("Mini PC");
        input.capacity = measured(2880, 1800, 60, 38, 70);
        StreamPlanning::remember(input.capacity, 2520, 1576, 60, "H.264", 5, 8, Now - 60, 60, true);
        QCOMPARE(planStream(input).width, 2520); // Room to spare, but 2880×1800 failed.
        input.now = Now + StreamPlanning::OverloadMemorySeconds;
        QCOMPARE(planStream(input).width, 2880);
    }

    void codecIsTheBestThisComputerDecodesInHardware()
    {
        StreamPlanInput input = macBook("Host");
        input.serverCodecModeSupport = SCM_H264 | SCM_HEVC | SCM_AV1_MAIN8;
        QCOMPARE(planStream(input).codec, QString("H.264"));   // H.264-only client
        input.hevc = true;
        QCOMPARE(planStream(input).codec, QString("HEVC"));
        input.hevc = false; input.av1 = true;
        QCOMPARE(planStream(input).codec, QString("AV1"));
        input.serverCodecModeSupport = SCM_H264 | SCM_HEVC;  // host can't send AV1
        QCOMPARE(planStream(input).codec, QString("H.264"));
        input.h264 = false; input.av1 = false;                // nothing in hardware
        QCOMPARE(planStream(input).codec, QString("H.264"));
    }

    void manualChoicesAreKeptAndExplained()
    {
        StreamPlanInput input = macBook("Mini PC");
        input.capacity = measured(2880, 1800, 60, 38, 70);
        input.width = 2880; input.height = 1800;
        StreamPlan plan = planStream(input);
        QCOMPARE(QSize(plan.width, plan.height), QSize(2880, 1800));
        QVERIFY(!plan.limitedByHost);
        QVERIFY2(plan.resolutionWarning.contains("Mini PC delivered about 26 fps"), qPrintable(plan.resolutionWarning));

        // A size it hasn't streamed: estimated from the one it has.
        input.capacity = measured(1920, 1200, 60, 15, 28);
        plan = planStream(input);
        QVERIFY2(plan.resolutionWarning.contains("Mini PC") && plan.resolutionWarning.contains("fps"), qPrintable(plan.resolutionWarning));

        // A frame rate above the display's.
        input = macBook("Gaming PC");
        input.fps = 120;
        plan = planStream(input);
        QCOMPARE(plan.fps, 120);
        QVERIFY(plan.fpsWarning.contains("60"));

        // A codec this computer can only decode in software.
        input = macBook("Gaming PC");
        input.codec = "HEVC";
        plan = planStream(input);
        QCOMPARE(plan.codec, QString("HEVC"));
        QVERIFY(plan.codecWarning.contains("hardware"));
        // One the host doesn't send.
        input.codec = "AV1";
        QVERIFY(planStream(input).codecWarning.contains("Gaming PC"));

        // Nothing to say about a fast host at native.
        input = macBook("Gaming PC");
        input.capacity = measured(2880, 1800, 60, 10, 22);
        input.width = 2880; input.height = 1800; input.fps = 60;
        plan = planStream(input);
        QVERIFY(plan.resolutionWarning.isEmpty() && plan.fpsWarning.isEmpty());
    }

    void bitrateFollowsPixelsFramesAndCodec()
    {
        using StreamPlanning::budget;
        const auto native = budget("H.264", 2880, 1800, 60);
        const auto smaller = budget("H.264", 1920, 1200, 60);
        QVERIFY(qAbs(double(native.startKbps) / smaller.startKbps - 2.25) < 0.05);
        QVERIFY(budget("HEVC", 2880, 1800, 60).startKbps < native.startKbps);
        QVERIFY(budget("AV1", 2880, 1800, 60).startKbps < budget("HEVC", 2880, 1800, 60).startKbps);
        QVERIFY(budget("H.264", 2880, 1800, 30).startKbps * 2 <= native.startKbps + 100);
        QVERIFY(native.floorKbps < native.startKbps && native.startKbps < native.ceilingKbps);
        // About 0.04 bits per pixel per frame: 12 Mbps at 2880×1800@60.
        QVERIFY2(native.floorKbps >= 11500 && native.floorKbps <= 13000, qPrintable(QString::number(native.floorKbps)));

        StreamPlanInput input = macBook("Mini PC");
        input.bitrateKbps = 8000;
        StreamPlan plan = planStream(input);
        QCOMPARE(plan.startBitrateKbps, 8000);
        QCOMPARE(plan.ceilingKbps, 8000);
        QVERIFY(plan.bitrateWarning.contains("blocky"));
        input.bitrateKbps = 40000;
        QVERIFY(planStream(input).bitrateWarning.isEmpty());
    }

    // Only this stream's fields are replaced; how a bitrate change lands is kept.
    void capacitySurvivesASaveAndLoad()
    {
        HostCapacity before;
        before.bitrateChange = HostCapacity::BitrateChange::Rebuild;
        before.bitrateChangeStallMs = 1500;
        before.save("host-a");

        HostCapacity capacity = HostCapacity::load("host-a");
        StreamPlanning::remember(capacity, 2880, 1800, 60, "H.264", 38.5, 71, Now, 25, true);
        capacity.save("host-a");

        const HostCapacity after = HostCapacity::load("host-a");
        QVERIFY(after.measured());
        QCOMPARE(after.measuredAt, Now);
        QCOMPARE(QSize(after.width, after.height), QSize(2880, 1800));
        QCOMPARE(after.fps, 60);
        QCOMPARE(after.codec, QString("H.264"));
        QCOMPARE(after.hostLatencyAverageMs, 38.5);
        QCOMPARE(after.hostLatencyP95Ms, 71.0);
        // What it should still manage: at most half, for a host this far behind.
        QCOMPARE(after.overloadedPixelRate, qint64(2880) * 1800 * 60);
        QCOMPARE(after.overloadedAt, Now);
        QCOMPARE(after.bitrateChange, HostCapacity::BitrateChange::Rebuild);
        QCOMPARE(after.bitrateChangeStallMs, 1500);
        QVERIFY(!HostCapacity::load("host-b").measured());
    }

    void hostLatencyNeedsTwentySecondsOfVideo()
    {
        HostLatencyMeter meter;
        for (int frame = 0; frame < 60 * 19; ++frame)
            meter.addFrame(quint64(frame) * 16667, quint16(frame % 20 == 0 ? 700 : 300));
        QVERIFY(!meter.meaningful());
        for (int frame = 60 * 19; frame < 60 * 30; ++frame)
            meter.addFrame(quint64(frame) * 16667, quint16(frame % 20 == 0 ? 700 : 300));
        meter.addFrame(quint64(60 * 30) * 16667, 0); // Not reported: ignored.
        QVERIFY(meter.meaningful());
        QCOMPARE(meter.frames(), quint64(60 * 30 + 1));
        QVERIFY(qAbs(meter.averageMs() - 32) < 0.01);
        QVERIFY(meter.p95Ms() >= 30 && meter.p95Ms() <= 30.5);
        QVERIFY(meter.percentileMs(0.99) >= 70);
    }

    void pacingFollowsHowFramesArrive()
    {
        const quint64 frameUs = 16667;
        // Steady 60 fps: stays off.
        AutomaticPacing steady(60);
        for (quint64 i = 0; i < 60 * 60; ++i) QVERIFY(!steady.observe(i * frameUs, true));
        QVERIFY(!steady.active());

        // Pairs a millisecond apart, every other frame interval: on within a few seconds.
        AutomaticPacing bursty(60);
        quint64 changedAt = 0;
        for (quint64 i = 0; i < 60 * 10 && !changedAt; ++i) {
            const quint64 at = (i / 2) * 2 * frameUs + (i % 2) * 1000;
            if (bursty.observe(at, true)) changedAt = at;
        }
        QVERIFY(bursty.active());
        QVERIFY(changedAt <= 3 * AutomaticPacing::WindowUs);

        // Back to steady: off only after half a minute of it.
        quint64 offAt = 0;
        for (quint64 i = 1; i < 60 * 60 && !offAt; ++i) {
            if (bursty.observe(changedAt + i * frameUs, true)) offAt = changedAt + i * frameUs;
        }
        QVERIFY(!bursty.active());
        QVERIFY(offAt - changedAt >= AutomaticPacing::SteadyForUs);

        // Bunches nobody can see don't count.
        AutomaticPacing hidden(60);
        for (quint64 i = 0; i < 60 * 10; ++i) QVERIFY(!hidden.observe((i / 2) * 2 * frameUs + (i % 2) * 1000, false));
        QVERIFY(!hidden.active());
        QCOMPARE(hidden.visibleUs(), quint64(0));
    }

    // The desktop's presentation time at or after its own "now" used to wrap to
    // thousands of years of latency.
    void presentationLatencyNeverUnderflows()
    {
        const uint64_t desktopNowNs = 5000000000ull, nowUs = 9000000;
        QCOMPARE(PresentationMonitor::displayLatencyUs(desktopNowNs, desktopNowNs - 2000000, nowUs, nowUs - 10000), int64_t(8000));
        QCOMPARE(PresentationMonitor::displayLatencyUs(desktopNowNs, desktopNowNs, nowUs, nowUs - 10000), int64_t(-1));
        QCOMPARE(PresentationMonitor::displayLatencyUs(desktopNowNs, desktopNowNs + 3000000, nowUs, nowUs - 10000), int64_t(-1));
        // Shown before it was decoded: a clock quirk, not a latency of zero.
        QCOMPARE(PresentationMonitor::displayLatencyUs(desktopNowNs, desktopNowNs - 20000000, nowUs, nowUs - 10000), int64_t(-1));
        QCOMPARE(PresentationMonitor::displayLatencyUs(desktopNowNs, 0, 1000, 0), int64_t(-1));
    }
};

QTEST_GUILESS_MAIN(StreamPlanTest)
#include "tst_streamplan.moc"
