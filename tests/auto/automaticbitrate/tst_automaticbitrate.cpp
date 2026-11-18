#include <QtTest>
#include <deque>
#include <random>
#include "streaming/automaticbitrate.h"

using namespace AutomaticBitrate;

namespace {

struct Link {
    double capacityKbps = 100000;
    double bufferMs = 200;
    double randomLoss = 0;
    double clumpMs = 0;      // Up to this much arrival jitter, as Wi-Fi aggregation gives.
    int baseDelayMs = 3;
};

struct Encoder {
    double activity = 1;     // Below 1 the picture is too still to use the bitrate.
    int keyframeEverySeconds = 0;
};

struct Result {
    double finalTarget = 0;
    double meanQueueMs = 0;
    double maxQueueMs = 0;
    double lossPercent = 0;
    double reactionSeconds = -1;
    double utilisation = 0;
    double meanTarget = 0;
};

// One FIFO bottleneck. Frames come from an encoder following the controller.
class Simulation {
public:
    Simulation(int startKbps, int ceilingKbps, unsigned seed = 7, int floorKbps = 500)
        : controller(startKbps, ceilingKbps, 60, floorKbps), random(seed) {}

    Result run(double seconds, const std::function<void(double, Link&, Encoder&)>& script = nullptr,
               double reactionTo = 0)
    {
        Result result;
        const uint64_t frameUs = 1000000 / 60;
        std::uniform_real_distribution<double> unit(0, 1);
        std::normal_distribution<double> sizeNoise(1, 0.25);
        double queueDrainsAtUs = 0, queueSum = 0;
        uint64_t frames = 0, sent = 0, lost = 0, nextTick = 100000;
        double measureFrom = seconds * 0.5, delivered = 0, offered = 0, targetSum = 0;
        uint64_t targetTicks = 0, measured = 0;
        for (uint64_t now = frameUs; now < uint64_t(seconds * 1e6); now += frameUs) {
            const double t = now / 1e6;
            if (script) script(t, link, encoder);
            while (nextTick <= now) {
                while (!pending.empty() && pending.front().lastArrivalUs <= nextTick) {
                    controller.onFrame(pending.front());
                    pending.pop_front();
                }
                lastDecision = controller.tick(nextTick);
                nextTick += 100000;
                if (t >= measureFrom) { targetSum += lastDecision.targetKbps; ++targetTicks; }
            }
            int target = lastDecision.targetKbps ? lastDecision.targetKbps : startTarget();
            if (lastDecision.drainSequence != drainSeen) {
                drainSeen = lastDecision.drainSequence;
                skipFrames = lastDecision.drainFrames;
            }
            if (skipFrames > 0) { --skipFrames; continue; }
            double bytes = target * 1000.0 / 8 / 60 * std::max(0.3, sizeNoise(random)) * encoder.activity;
            if (encoder.keyframeEverySeconds && frames % (60 * encoder.keyframeEverySeconds) == 0) bytes *= 8;
            ++frames;
            const int packets = std::max(1, int(bytes / 1400));
            const double packetUs = 1400 * 8 / link.capacityKbps * 1000;
            FrameSample sample;
            sample.sentUs = now;
            const double waitedMs = std::max(0.0, queueDrainsAtUs - now) / 1000;
            bool first = true;
            for (int i = 0; i < packets; ++i) {
                ++sent;
                queueDrainsAtUs = std::max<double>(queueDrainsAtUs, now);
                const double waitUs = queueDrainsAtUs - now;
                if (waitUs > link.bufferMs * 1000 || unit(random) < link.randomLoss) {
                    ++lost; ++sample.lostPackets;
                    continue;
                }
                queueDrainsAtUs += packetUs;
                double arrival = queueDrainsAtUs + link.baseDelayMs * 1000;
                if (link.clumpMs > 0) arrival += unit(random) * link.clumpMs * 1000;
                if (first || uint64_t(arrival) < sample.firstArrivalUs) sample.firstArrivalUs = uint64_t(arrival);
                first = false;
                sample.lastArrivalUs = std::max<uint64_t>(sample.lastArrivalUs, uint64_t(arrival));
                sample.payloadBytes += 1400;
                ++sample.dataPackets;
            }
            if (first) { sample.firstArrivalUs = now + link.baseDelayMs * 1000; sample.lastArrivalUs = sample.firstArrivalUs; sample.sentUs = 0; }
            pending.push_back(sample);
            if (t >= measureFrom) {
                queueSum += waitedMs; result.maxQueueMs = std::max(result.maxQueueMs, waitedMs); ++measured;
                delivered += sample.payloadBytes; offered += bytes;
            }
            if (reactionTo > 0 && result.reactionSeconds < 0 && t > reactionTo
                    && lastDecision.targetKbps <= link.capacityKbps * 0.95)
                result.reactionSeconds = t - reactionTo;
        }
        result.finalTarget = lastDecision.targetKbps;
        result.meanQueueMs = measured ? queueSum / measured : 0;
        result.lossPercent = sent ? 100.0 * lost / sent : 0;
        result.utilisation = delivered * 8 / 1000 / (seconds * 0.5) / link.capacityKbps;
        result.meanTarget = targetTicks ? targetSum / targetTicks : 0;
        return result;
    }

    int startTarget() { return controller.tick(1).targetKbps; }

    std::deque<FrameSample> pending;
    Controller controller;
    Link link;
    Encoder encoder;
    Decision lastDecision;
    uint32_t drainSeen = 0;
    int skipFrames = 0;
    std::mt19937 random;
};

void show(const char* name, const Result& r)
{
    qInfo("%-28s target %6.1f Mbps  queue mean %5.1f max %5.1f ms  loss %5.2f%%  use %3.0f%%  react %4.2fs",
          name, r.finalTarget / 1000, r.meanQueueMs, r.maxQueueMs, r.lossPercent, r.utilisation * 100, r.reactionSeconds);
}

struct Picture {
    double fps = 20;
    int bytes = 2500;
    bool timed = true;       // A repeated still frame comes with no host latency to go on.
};

struct HostResult {
    int cuts = 0;
    int changes = 0;         // Confirmed by the host.
    int finalTarget = 0;
    int stallMs = -1;
    bool setOnce = false;
};

// A host told over HTTP, on a gigabit link that never fills. Its frames pick up delay
// on the host that their send times leave out, as Vibepollo's do. Each change it
// confirms can stop its video while it rebuilds the encoder, and what it captured
// meanwhile then comes out in a burst. Changes reach it the way session.cpp sends them.
class Host {
public:
    Host(int startKbps, int ceilingKbps, int floorKbps, bool setOnce, unsigned seed = 7)
        : controller(startKbps, ceilingKbps, 60, floorKbps), current(startKbps), setOnce(setOnce), random(seed)
    {
        controller.setStampsEstimated(true);
        controller.setSetOnce(setOnce);
    }

    HostResult run(double seconds, const std::function<Picture(double)>& picture,
                   const std::function<void(double, Host&)>& script = nullptr)
    {
        HostResult result;
        std::uniform_real_distribution<double> unit(0, 1);
        std::deque<FrameSample> inFlight;
        uint64_t nextCapture = 0, nextTick = 100000, departed = 0, askedAt = 0, ackAt = 0, stallUntil = 0;
        int previous = current, asked = 0;
        uint32_t measured = 0;
        bool afterStall = false;
        for (uint64_t now = 1000; now < uint64_t(seconds * 1e6); now += 1000) {
            const double t = now / 1e6;
            if (script) script(t, *this);
            const Picture frame = picture(t);
            if (now >= nextCapture) {
                const uint64_t capture = now;
                nextCapture = now + uint64_t(1e6 / frame.fps);
                // The encoder keeps to what it was last given; a still picture uses less.
                const int bytes = std::min<int>(frame.bytes, int(current * 1000.0 / 8 / frame.fps));
                uint64_t departs = capture + uint64_t(unit(random) * hostDelayMs * 1000);
                if (departs < stallUntil) { departs = stallUntil; afterStall = true; }
                departs = std::max(departs, departed + 500);
                departed = departs;
                FrameSample sample;
                sample.sentUs = frame.timed ? capture + 1 : 0;
                sample.firstArrivalUs = departs + 1000;
                sample.lastArrivalUs = sample.firstArrivalUs + uint64_t(bytes * 8 / 1000.0);
                const int packets = std::max(1, bytes / 1400);
                for (int i = 0; i < packets; ++i) {
                    if (unit(random) < loss) ++sample.lostPackets;
                    else { ++sample.dataPackets; sample.payloadBytes += 1400; }
                }
                if (afterStall && departs >= stallUntil && stallUntil) {
                    sample.keyframe = true;
                    afterStall = false;
                    stallUntil = 0;
                }
                inFlight.push_back(sample);
            }
            while (!inFlight.empty() && inFlight.front().lastArrivalUs <= now) {
                controller.onFrame(inFlight.front());
                inFlight.pop_front();
            }
            if (ackAt && now >= ackAt) {
                // The host answers, then rebuilds: video stops from here.
                ackAt = 0;
                current = asked;
                ++result.changes;
                controller.changeApplied(now);
                if (stallMs) stallUntil = now + uint64_t(stallMs) * 1000;
            }
            if (now >= nextTick) {
                nextTick += 100000;
                const Decision decision = controller.tick(now);
                if (decision.targetKbps < previous * 0.99) ++result.cuts;
                previous = decision.targetKbps;
                if (!ackAt && worthAsking(decision.targetKbps, current, now - askedAt, true)) {
                    askedAt = now;
                    asked = decision.targetKbps;
                    ackAt = now + 20000;
                }
                // As the driver does with each measurement.
                const Status status = controller.status();
                if (status.changesMeasured != measured) {
                    measured = status.changesMeasured;
                    result.stallMs = status.changeStallMs;
                    rebuilt = rebuilt || status.changeStallMs >= RebuildStallMs;
                    setOnce = !continuousChanges(false, !rebuilt);
                    controller.setSetOnce(setOnce);
                }
            }
        }
        result.finalTarget = previous;
        result.setOnce = setOnce;
        return result;
    }

    Controller controller;
    int current;
    bool setOnce;
    bool rebuilt = false;
    double hostDelayMs = 40;  // Uniform from none to this.
    int stallMs = 0;
    double loss = 0;
    std::mt19937 random;
};

}

class AutomaticBitrateTest : public QObject {
    Q_OBJECT
private slots:
    void automaticTargetsRequireLiveSupportAndNeverProbeARebuildingHost() {
        QVERIFY(!mayApplyLiveTarget(false,false,false));
        QVERIFY(!mayApplyLiveTarget(false,false,true));
        QVERIFY(mayApplyLiveTarget(false,true,false));
        QVERIFY(mayApplyLiveTarget(true,false,false));
        QVERIFY(!mayApplyLiveTarget(true,true,true));
    }
    void fastLinkClimbsToTheCeilingWithoutQueueing()
    {
        Simulation sim(30000, 64000);
        sim.link.capacityKbps = 500000;
        const auto r = sim.run(30);
        show("fast wired link", r);
        QCOMPARE(int(r.finalTarget), 64000);
        QVERIFY(r.meanQueueMs < 2);
        QCOMPARE(r.lossPercent, 0.0);
    }

    void slowLinkSettlesJustBelowCapacity()
    {
        Simulation sim(53000, 64000);
        sim.link.capacityKbps = 20000;
        const auto r = sim.run(60);
        show("20 Mbps link, start 53", r);
        QVERIFY2(r.meanTarget > 14000 && r.meanTarget < 20500, qPrintable(QString::number(r.meanTarget)));
        QVERIFY2(r.meanQueueMs < 15, qPrintable(QString::number(r.meanQueueMs)));
    }

    void capacityDropIsFoundQuickly()
    {
        Simulation sim(40000, 64000);
        sim.link.capacityKbps = 100000;
        const auto r = sim.run(40, [](double t, Link& link, Encoder&) {
            link.capacityKbps = t < 20 ? 100000 : 15000;
        }, 20);
        show("100 to 15 Mbps at 20s", r);
        QVERIFY2(r.reactionSeconds >= 0 && r.reactionSeconds < 1.5, qPrintable(QString::number(r.reactionSeconds)));
        QVERIFY(r.finalTarget < 15000);
    }

    void oneCapacityDropDoesNotCollapseTheBitrate()
    {
        Simulation sim(12000, 12000);
        double lowest = 12000;
        const auto r = sim.run(40, [&](double t, Link& link, Encoder&) {
            link.capacityKbps = t < 15 ? 200000 : 8000;
            if (t > 15) lowest = std::min<double>(lowest, sim.lastDecision.targetKbps);
        });
        show("12 Mbps stream, 8 Mbps link", r);
        QVERIFY2(lowest >= 4500, qPrintable(QString::number(lowest)));
        QVERIFY2(r.finalTarget >= 6000, qPrintable(QString::number(r.finalTarget)));
    }

    void capacityReturnIsReclaimed()
    {
        Simulation sim(40000, 64000);
        const auto r = sim.run(90, [](double t, Link& link, Encoder&) {
            link.capacityKbps = t > 10 && t < 30 ? 12000 : 200000;
        });
        show("dip to 12 Mbps and back", r);
        QVERIFY2(r.finalTarget > 50000, qPrintable(QString::number(r.finalTarget)));
    }

    void plentyOfRoomIsReclaimedQuickly()
    {
        Simulation sim(40000, 64000);
        double at = 0;
        sim.run(60, [&](double t, Link& link, Encoder&) {
            link.capacityKbps = t > 10 && t < 20 ? 8000 : 500000;
            if (t > 20 && !at && sim.lastDecision.targetKbps > 40000) at = t;
        });
        QVERIFY2(at > 20 && at < 36, qPrintable(QString::number(at)));
    }

    void noisyWifiDoesNotLoseQuality()
    {
        Simulation sim(40000, 64000);
        sim.link.capacityKbps = 200000;
        sim.link.clumpMs = 6;
        sim.link.randomLoss = 0.003;
        const auto r = sim.run(60);
        show("noisy Wi-Fi, plenty of room", r);
        QVERIFY2(r.meanTarget > 55000, qPrintable(QString::number(r.meanTarget)));
    }

    void heavyLossLowersTheBitrate()
    {
        Simulation sim(40000, 64000);
        sim.link.capacityKbps = 200000;
        sim.link.randomLoss = 0.15;
        const auto r = sim.run(20);
        show("15% loss", r);
        QVERIFY(r.finalTarget < 20000);
    }

    void aMomentOfRadioTroubleCostsLittle()
    {
        Simulation sim(40000, 64000);
        double lowest = 64000, at = 0;
        sim.run(40, [&](double t, Link& link, Encoder&) {
            link.capacityKbps = 300000;
            link.randomLoss = t > 15 && t < 15.5 ? 0.35 : 0;
            if (t > 15) lowest = std::min<double>(lowest, sim.lastDecision.targetKbps);
            if (t > 16 && !at && sim.lastDecision.targetKbps >= 55000) at = t;
        });
        QVERIFY2(lowest >= 20000, qPrintable(QString::number(lowest)));
        QVERIFY2(at && at < 24, qPrintable(QString::number(at)));
    }

    void stillPictureKeepsTheBitrateReady()
    {
        Simulation sim(40000, 64000);
        sim.link.capacityKbps = 200000;
        sim.encoder.activity = 0.05;
        const auto r = sim.run(120);
        show("still desktop", r);
        QVERIFY2(r.finalTarget > 60000, qPrintable(QString::number(r.finalTarget)));
    }

    void motionAfterStillnessRecoversFromOvershoot()
    {
        Simulation sim(40000, 64000);
        const auto r = sim.run(60, [](double t, Link& link, Encoder& encoder) {
            link.capacityKbps = 30000;
            encoder.activity = t < 20 ? 0.05 : 1;
        });
        show("still then motion on 30 Mbps", r);
        QVERIFY2(r.finalTarget < 32000, qPrintable(QString::number(r.finalTarget)));
        QVERIFY2(r.meanQueueMs < 15, qPrintable(QString::number(r.meanQueueMs)));
    }

    void keyframesAloneDoNotCutTheBitrate()
    {
        Simulation sim(40000, 64000);
        sim.link.capacityKbps = 80000;
        sim.encoder.keyframeEverySeconds = 5;
        const auto r = sim.run(60);
        show("keyframes every 5s", r);
        QVERIFY2(r.meanTarget > 35000, qPrintable(QString::number(r.meanTarget)));
    }

    void shallowBufferBurstLossTurnsOnPacing()
    {
        Controller controller(40000, 64000, 60);
        uint64_t now = 0;
        for (int i = 0; i < 300; ++i) {
            now += 16667;
            FrameSample frame;
            frame.firstArrivalUs = now; frame.lastArrivalUs = now + 2000;
            frame.sentUs = now - 3000;
            frame.payloadBytes = 80000; frame.dataPackets = 60; frame.lostPackets = 2;
            controller.onFrame(frame);
        }
        const auto decision = controller.tick(now);
        QCOMPARE(decision.paceKbps, decision.targetKbps * 3);
    }

    void hostSideDelayWithoutLossKeepsTheBitrate()
    {
        // The Mini PC: 0% loss, a queue of 15-48 ms, and cuts all the way to the floor.
        for (const auto& [fps, bytes, delayMs] : {std::tuple{20.0, 2500, 40.0}, {20.0, 2500, 60.0}, {60.0, 20000, 40.0}}) {
            Host host(21200, 41388, 500, false);
            host.hostDelayMs = delayMs;
            const auto r = host.run(120, [&](double) { return Picture{fps, bytes, true}; });
            QVERIFY2(r.cuts == 0, qPrintable(QStringLiteral("%1 fps, %2 ms: %3 cuts").arg(fps).arg(delayMs).arg(r.cuts)));
            QVERIFY(r.finalTarget >= 21200);
        }
    }

    void expensiveHostChangesRarely()
    {
        // Not yet measured, so set once. Every change stops video for 1.5 s and then
        // releases a burst; real loss from 30 s on still gets an answer, once a minute.
        Host host(21200, 41388, floorKbps(1920, 1200, 60), true);
        host.stallMs = 1500;
        const auto r = host.run(150, [](double) { return Picture{60, 30000, true}; },
                                [](double t, Host& host) { host.loss = t >= 30 ? 0.05 : 0; });
        qInfo("expensive host: %d changes, final %d kbps, stall %d ms", r.changes, r.finalTarget, r.stallMs);
        QVERIFY2(r.changes >= 1 && r.changes <= 2, qPrintable(QString::number(r.changes)));
        QVERIFY(r.stallMs >= 1400);
        QVERIFY(r.setOnce);
    }

    void recurringLossBurstsStepDownOnce()
    {
        // The Mini PC over a busy link: no lasting loss, but every few seconds a burst
        // that froze the picture until the next keyframe (4 Nov, 12:29). A host that
        // rebuilds for a change still gets one firm step down, and not a flurry.
        Host host(19500, 41388, floorKbps(2880, 1800, 60), true);
        host.stallMs = 1200;
        const auto r = host.run(150, [](double) { return Picture{60, 30000, true}; },
                                [](double t, Host& host) { host.loss = t >= 20 && std::fmod(t, 4.5) < 0.4 ? 0.3 : 0; });
        qInfo("recurring loss bursts: %d changes, final %d kbps", r.changes, r.finalTarget);
        QVERIFY2(r.changes >= 1 && r.changes <= 3, qPrintable(QString::number(r.changes)));
        QVERIFY2(r.finalTarget <= 19500 * 0.75, qPrintable(QString::number(r.finalTarget)));
        QVERIFY(r.setOnce);
    }

    void idleDesktopNeitherFreezesNorRatchets()
    {
        // A still desktop: repeated frames with no timing, and now and then a moment of
        // typing that picks up host-side delay. A host that changes in place, left low
        // by an earlier dip, as the Mini PC sat at 4 Mbps for minutes while 135 kbps came.
        Host host(8000, 41388, floorKbps(1920, 1200, 60), false);
        host.hostDelayMs = 60;
        const auto r = host.run(150, [](double t) {
            return std::fmod(t, 20) < 5 ? Picture{20, 4000, true} : Picture{20, 2000, false};
        });
        qInfo("idle desktop: %d cuts, final %d kbps", r.cuts, r.finalTarget);
        QCOMPARE(r.cuts, 0);
        QVERIFY2(r.finalTarget >= 16000, qPrintable(QString::number(r.finalTarget)));
    }

    void changeCostDecidesHowTheBitrateMoves()
    {
        // Direct reports change in place; over HTTP, set once until seen to change live.
        QVERIFY(continuousChanges(true, false));
        QVERIFY(!continuousChanges(false, false));
        QVERIFY(continuousChanges(false, true));

        Controller controller(20000, 40000, 60);
        uint64_t now = 0;
        const auto frames = [&](double seconds, bool keyframeFirst) {
            for (uint64_t end = now + uint64_t(seconds * 1e6); now < end;) {
                now += 16667;
                FrameSample frame;
                frame.firstArrivalUs = now; frame.lastArrivalUs = now + 1000; frame.sentUs = now - 2000;
                frame.payloadBytes = 30000; frame.dataPackets = 22;
                frame.keyframe = keyframeFirst;
                keyframeFirst = false;
                controller.onFrame(frame);
            }
        };
        frames(2, false);
        // A host that rebuilt: 400 ms without video, back with a keyframe.
        controller.changeApplied(now);
        now += 400000;
        frames(1.5, true);
        auto status = controller.status();
        QCOMPARE(status.changesMeasured, 1u);
        QVERIFY(status.changeStallMs >= 400 && status.changeKeyframe);
        QVERIFY(!continuousChanges(false, status.changeStallMs < RebuildStallMs));
        // One that changed in place: video never stopped.
        controller.changeApplied(now);
        frames(1.5, false);
        status = controller.status();
        QCOMPARE(status.changesMeasured, 2u);
        QVERIFY(status.changeStallMs < RebuildStallMs && !status.changeKeyframe);
        QVERIFY(continuousChanges(false, status.changeStallMs < RebuildStallMs));
    }

    void lossStopsAtAFloorForThePictureSize()
    {
        // ceiling/20 left 2650 kbps for 2880×1800 at 60 fps: a blur, not a stream.
        Simulation sim(40000, 53000, 7, floorKbps(2880, 1800, 60));
        sim.link.capacityKbps = 200000;
        sim.link.randomLoss = 0.15;
        const auto r = sim.run(20);
        QVERIFY2(r.finalTarget >= 12000 && r.finalTarget <= 13000, qPrintable(QString::number(r.finalTarget)));
    }

    void realCongestionCanSpendQualityBeforeResolution()
    {
        Simulation sim(40000,53000,7,floorKbps(2880,1800,60));
        sim.link.capacityKbps=6000;
        const auto result=sim.run(30);
        show("below quality estimate",result);
        QVERIFY(result.finalTarget < 6500);
        QVERIFY(result.meanQueueMs < 40);
    }

    void aQualifiedCongestedStartIsNotRaisedToTheQualityEstimate()
    {
        Controller remembered(2500,53000,60,12000,true);
        QCOMPARE(remembered.tick(1000).targetKbps,2500);
        Controller unqualified(2500,53000,60,12000);
        QCOMPARE(unqualified.tick(1000).targetKbps,12000);
    }

    void ceilingIsAlwaysRespected()
    {
        Controller controller(90000, 20000, 60);
        QCOMPARE(controller.tick(1000).targetKbps, 20000);
        controller.setCeiling(5000);
        QCOMPARE(controller.tick(2000).targetKbps, 5000);
    }
};

QTEST_GUILESS_MAIN(AutomaticBitrateTest)
#include "tst_automaticbitrate.moc"
