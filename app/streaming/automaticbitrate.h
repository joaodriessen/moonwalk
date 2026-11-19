#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>

// Decides the stream's bitrate from how video actually arrives. Host-independent
// and clock-free, so it runs the same against a real link and a simulated one.
//
// A link that is full shows it first as delay: packets wait in a queue somewhere
// before any are lost. So the first packet of every frame is compared against
// when the host sent it. The host's clock is unknown, but the smallest difference
// seen recently is the empty-queue baseline, and anything above that is queueing.
//
// Wi-Fi delivers in clumps, which looks like delay but isn't a queue that lasts.
// The lowest delay in a short window is what can't be explained by a clump, so
// that is what counts as a standing queue. Scattered loss with no queue is left
// to FEC, since lowering quality doesn't cure a noisy radio.
//
// Delay picked up on the host, before the first packet leaves, looks the same, and
// lowering the bitrate doesn't cure that either. It shows as video arriving well
// below the target in frames that cross the link far faster than that.
namespace AutomaticBitrate {

// Below about 0.04 bits per pixel per frame a picture is plainly soft whatever the
// link does: about 12 Mbps at 2880×1800 and 60 fps. Going lower trades a stall for blur.
inline int floorKbps(int width, int height, int framesPerSecond)
{
    return int(std::lround(double(width) * height * framesPerSecond * 0.04 / 1000));
}

// A pause this long after a change is treated as costly. Timing alone does not
// establish that an encoder rebuild caused it.
constexpr int RebuildStallMs = 150;

// Whether a host's bitrate may follow the link during a stream. One that takes
// reports directly changes in place. One told over HTTP may rebuild its encoder for
// each change and freeze the picture while it does, so until a change has been seen
// to land in place, its bitrate is chosen before launch and then set once: lowered
// only for real, lasting loss, at most once a minute, and raised only by the next
// stream starting from what this one settled on.
inline bool continuousChanges(bool directReports, bool measuredLive)
{
    return directReports || measuredLive;
}

// Performance tuning may change a target only over a live control contract or
// after an explicit/manual change demonstrated that this host applies it live.
// Unknown HTTP hosts learn targets for the next launch without a disruptive probe.
inline bool mayApplyLiveTarget(bool directReports, bool measuredLive, bool measuredRebuild)
{
    return !measuredRebuild && (directReports || measuredLive);
}

// Whether a host told over HTTP should hear a new target yet. Each request can cost
// it a new encoder, so small moves aren't worth one. It never hears one before its
// video starts, which it would only hold for some later rebuild, or the value it runs.
inline bool worthAsking(int targetKbps, int currentKbps, uint64_t sinceAskedUs, bool videoStarted)
{
    if (!videoStarted || targetKbps <= 0 || targetKbps == currentKbps) return false;
    const bool raising = targetKbps > currentKbps;
    return std::abs(targetKbps - currentKbps) * 8 >= currentKbps
           && sinceAskedUs >= (raising ? 4000000u : 1000000u);
}

struct FrameSample {
    uint64_t firstArrivalUs = 0;
    uint64_t lastArrivalUs = 0;
    uint64_t sentUs = 0;        // On the host's clock. 0 when it gave no timing for this frame.
    uint32_t payloadBytes = 0;  // Video data received, not FEC.
    uint32_t dataPackets = 0;
    uint32_t lostPackets = 0;
    bool keyframe = false;      // The host began a new sequence here (an IDR).
};

enum class Phase { Measuring, Climbing, Holding, Steady, BackingOff };

struct Decision {
    int targetKbps = 0;
    int paceKbps = 0;       // 0: send as fast as the host can.
    int queueMs = 0;
    int lossPercent = 0;
    int drainFrames = 0;    // Frames the host should skip so a queue can empty.
    uint32_t drainSequence = 0;
};

struct Status {
    Phase phase = Phase::Measuring;
    int targetKbps = 0;
    int receivedKbps = 0;
    int ceilingKbps = 0;
    double queueMs = 0;
    double jitterMs = 0;
    double lossPercent = 0;
    double frameSpreadMs = 0;   // First packet of a frame to its last, typically.
    int floorKbps = 0;
    bool cutForCongestion = false;
    bool cutForLoss = false;    // Some cut answered real loss, not only delay.
    // The last change the host confirmed: how long video stopped after it, and
    // whether it came back with a keyframe. Counted, so a new one can be noticed.
    int changeStallMs = 0;
    bool changeKeyframe = false;
    uint32_t changesMeasured = 0;
};

class Controller {
public:
    Controller(int startKbps, int ceilingKbps, int framesPerSecond, int floorKbps = 500, bool resumeCongested = false)
        : m_Ceiling(std::max(500, ceilingKbps)),
          m_Floor(std::clamp(floorKbps, 500, m_Ceiling)),
          m_EffectiveFloor(resumeCongested ? 500 : m_Floor),
          m_FrameUs(1000000 / std::max(1, framesPerSecond)),
          m_Target(std::clamp<double>(startKbps, m_EffectiveFloor, m_Ceiling)) {}

    // The receive thread, once per frame.
    void onFrame(const FrameSample& frame)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const uint64_t previousUs = m_LastArrivalUs;
        m_LastArrivalUs = std::max(m_LastArrivalUs, frame.firstArrivalUs);
        if (m_Measuring && frame.firstArrivalUs >= m_ChangeAtUs) measureChange(frame, previousUs);
        // What the host's own change did to its video says nothing about the link.
        if (frame.firstArrivalUs < m_IgnoreUntilUs) return;
        if (frame.dataPackets >= 8) {
            const double spread = (frame.lastArrivalUs - frame.firstArrivalUs) / 1000.0;
            m_Status.frameSpreadMs += (spread - m_Status.frameSpreadMs) / 32;
            // A frame sent in one burst comes out at the speed of the slowest hop.
            const double trainKbps = frame.payloadBytes * 8 / std::max(spread, 0.25);
            m_TrainKbps = m_TrainKbps > 0 ? m_TrainKbps + (trainKbps - m_TrainKbps) / 16 : trainKbps;
        }
        Stored& slot = m_Frames[m_Head];
        slot.arrivalUs = frame.firstArrivalUs;
        slot.payloadBytes = frame.payloadBytes;
        slot.dataPackets = frame.dataPackets;
        slot.lostPackets = frame.lostPackets;
        slot.queueUs = -1;
        if (frame.sentUs) {
            const int64_t delay = int64_t(frame.firstArrivalUs) - int64_t(frame.sentUs);
            rotateBaseline(frame.firstArrivalUs);
            m_BaselineNow = m_HasBaseline ? std::min(m_BaselineNow, delay) : delay;
            m_BaselineOld = m_HasBaseline ? m_BaselineOld : delay;
            m_HasBaseline = true;
            slot.queueUs = std::max<int64_t>(0, delay - std::min(m_BaselineNow, m_BaselineOld));
        }
        // A big frame (a keyframe, say) legitimately holds up the next few behind it.
        if (frame.payloadBytes > 4 * std::max<uint32_t>(m_TypicalFrameBytes, 1) && m_TypicalFrameBytes > 0)
            m_BigFrameAtUs = frame.firstArrivalUs;
        m_TypicalFrameBytes = m_TypicalFrameBytes
            ? uint32_t((uint64_t(m_TypicalFrameBytes) * 31 + frame.payloadBytes) / 32) : frame.payloadBytes;
        m_Head = (m_Head + 1) % m_Frames.size();
        m_Count = std::min(m_Count + 1, m_Frames.size());
    }

    // The control thread, around ten times a second.
    Decision tick(uint64_t nowUs)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const double seconds = m_LastTickUs ? double(nowUs - m_LastTickUs) / 1e6 : 0;
        m_LastTickUs = nowUs;
        if (!m_StartUs) m_StartUs = nowUs;

        const Window rate = window(nowUs, 500000);
        // Frames can be sparse: on a still desktop, or when the host is leaving some out.
        Window recent = window(nowUs, 200000);
        if (recent.delayed < 3) recent = rate;
        const double receivedKbps = rate.seconds > 0 ? rate.payloadBytes * 8 / rate.seconds / 1000 : 0;
        const uint64_t losable = uint64_t(rate.dataPackets) + rate.lostPackets;
        const double loss = losable >= 20 ? double(rate.lostPackets) / losable : 0;
        m_Status.receivedKbps = int(receivedKbps);
        m_Status.lossPercent = loss * 100;

        Decision decision;
        const bool ignoring = nowUs < m_IgnoreUntilUs;
        // Loss that keeps coming back in short bursts. Each burst freezes the picture
        // until the next keyframe, though none of them lasts long.
        if (!ignoring && loss >= 0.02 && !m_InLossBurst) {
            m_InLossBurst = true;
            m_LossBursts[m_LossBurstHead++ % m_LossBursts.size()] = nowUs;
        }
        else if (loss < 0.005) m_InLossBurst = false;
        int recentBursts = 0;
        for (const uint64_t at : m_LossBursts) recentBursts += at && nowUs - at <= 20000000;
        if (ignoring) {
            m_OverTicks = 0;
            m_LossSinceUs = 0;
            m_Status.phase = Phase::Holding;
        } else if (recent.delayed >= 3) {
            const double standing = recent.minQueueUs / 1000.0;
            const double typical = recent.medianQueueUs / 1000.0;
            const double high = std::clamp(2.5 * m_JitterMs + 3, 5.0, 20.0);
            const double low = std::max(2.0, high * 0.4);
            m_Status.queueMs = standing;
            m_Status.jitterMs = m_JitterMs;

            const bool bigFrameGrace = m_BigFrameAtUs && nowUs - m_BigFrameAtUs < 300000;
            const bool collapsed = !bigFrameGrace && recent.lastQueueUs / 1000.0 >= 60
                                   && typical >= 40 && standing >= 25;
            m_OverTicks = standing > high ? m_OverTicks + 1 : 0;
            const bool heavyLoss = loss >= 0.10;
            const bool queuedLoss = loss >= 0.02 && (standing > low || m_LossSinceUs);
            if (loss >= 0.02 && !m_LossSinceUs) m_LossSinceUs = nowUs;
            if (loss < 0.02) m_LossSinceUs = 0;
            const bool sustainedLoss = m_LossSinceUs && nowUs - m_LossSinceUs >= 1000000;
            // The first keyframe and the first few frames are never typical. Only a
            // link in real trouble counts that early.
            const bool settling = nowUs - m_StartUs < 1500000;
            // Frames crossing the link far faster than video arrives: it isn't the link
            // holding them up. Where the host's send times are estimates, that alone
            // says so; otherwise only while the picture also leaves most of the target unused.
            const bool hostSideDelay = loss < 0.02
                && ((receivedKbps < 0.5 * m_Target && (m_TrainKbps <= 0 || m_TrainKbps >= 2 * receivedKbps))
                    || (m_EstimatedStamps && m_TrainKbps >= 4 * std::max(receivedKbps, 1.0)));
            bool over = collapsed || heavyLoss
                        || (!settling && ((m_OverTicks >= 2 && !hostSideDelay) || (queuedLoss && (standing > low || sustainedLoss))));
            // Only what a new encoder would cure is worth one: lasting real loss, or a
            // link plainly overwhelmed. And not again within the minute.
            const uint64_t changedAtUs = std::max(m_DecreaseAtUs, m_ChangeAtUs);
            const bool recurringLoss = recentBursts >= 3;
            if (m_SetOnce) {
                const bool lastingLoss = m_LossSinceUs && nowUs - m_LossSinceUs >= 3000000;
                over = !settling && (lastingLoss || recurringLoss || (collapsed && !hostSideDelay))
                       && (!changedAtUs || nowUs - changedAtUs >= (collapsed && !hostSideDelay ? 5000000u : 60000000u));
            }

            if (loss >= 0.01) m_LossAtUs = nowUs;
            if (loss >= 0.01 && standing <= low) m_PaceUntilUs = nowUs + 8000000;

            // A cut takes time to show: the queue has to drain first. Going again before
            // then would cut for a problem already dealt with, unless the queue is still
            // growing regardless. Loss with no queue is often just a moment's radio trouble,
            // which is no reason to keep going down.
            const bool stillGrowing = standing > m_QueueAtDecreaseMs * 1.25;
            const uint64_t refractory = stillGrowing && standing > low ? 300000 : 800000;
            if (over && nowUs - m_DecreaseAtUs >= refractory) {
                // One change a minute has to count, so recurring loss takes the larger step.
                const bool severe = collapsed || heavyLoss || standing > 3 * high || (m_SetOnce && recurringLoss);
                // The picture-quality estimate is a soft floor. Real standing
                // congestion can spend more quality to preserve resolution and cadence;
                // unqueued radio loss still stops at the quality estimate.
                if (!bigFrameGrace && !hostSideDelay && standing > high && (collapsed || m_OverTicks >= 2)) {
                    m_EffectiveFloor = 500;
                    m_Status.cutForCongestion = true;
                }
                backOff(receivedKbps, severe);
                if (loss >= 0.02) m_Status.cutForLoss = true;
                m_DecreaseAtUs = nowUs;
                m_QueueAtDecreaseMs = standing;
                m_OverTicks = 0;
                m_CleanSinceUs = 0;
                m_PaceUntilUs = std::max(m_PaceUntilUs, nowUs + 5000000);
                if (severe) {
                    m_DrainFrames = int(std::clamp<double>(std::ceil(typical * 1000 / m_FrameUs), 1, 4));
                    ++m_DrainSequence;
                }
                m_Status.phase = Phase::BackingOff;
            } else if (!over) {
                // Learn how noisy this link is from its quiet moments only.
                if (standing <= low) {
                    const double spread = typical - standing;
                    m_JitterMs += (std::min(spread, 12.0) - m_JitterMs) * std::min(1.0, seconds * 0.5);
                }
                const bool clean = standing <= low && (!m_LossAtUs || nowUs - m_LossAtUs >= 1000000);
                if (clean && !m_CleanSinceUs) m_CleanSinceUs = nowUs;
                if (!clean) m_CleanSinceUs = 0;
                if (clean && nowUs - m_DecreaseAtUs >= 1000000 && !m_SetOnce) raise(receivedKbps, seconds, nowUs);
                else m_Status.phase = Phase::Holding;
            }
        } else if (rate.dataPackets && loss < 0.01 && !m_SetOnce) {
            // Frames without timing, as a host repeats a still picture: nothing to measure,
            // but nothing wrong either, so a dip doesn't stay for the rest of the stream.
            if (!m_CleanSinceUs) m_CleanSinceUs = nowUs;
            if (nowUs - m_CleanSinceUs >= 5000000 && receivedKbps < 0.55 * m_Target)
                m_Target = std::min<double>(m_Target * (1 + 0.02 * seconds), m_Ceiling);
            m_Status.phase = Phase::Steady;
        }
        if (m_Target >= m_Floor && m_CleanSinceUs && nowUs - m_CleanSinceUs >= 30000000)
            m_EffectiveFloor = m_Floor;
        m_Status.targetKbps = int(m_Target);
        m_Status.ceilingKbps = m_Ceiling;
        m_Status.floorKbps = m_EffectiveFloor;
        decision.targetKbps = int(m_Target);
        decision.paceKbps = nowUs < m_PaceUntilUs ? int(m_Target) * 3 : 0;
        decision.queueMs = int(m_Status.queueMs);
        decision.lossPercent = int(std::lround(m_Status.lossPercent));
        decision.drainFrames = m_DrainFrames;
        decision.drainSequence = m_DrainSequence;
        return decision;
    }

    // A host whose bitrate is better set once (see continuousChanges).
    void setSetOnce(bool setOnce)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_SetOnce = setOnce;
    }

    // Send times worked out from the capture time and the host's own latency figure,
    // which can leave out delay it adds afterwards.
    void setStampsEstimated(bool estimated)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_EstimatedStamps = estimated;
    }

    // The host confirmed a new bitrate. Its video may stop while it rebuilds the encoder
    // and come out in a burst after. Nothing is judged until two seconds after that,
    // and how long it stopped is measured.
    void changeApplied(uint64_t nowUs)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_ChangeAtUs = nowUs;
        m_IgnoreUntilUs = std::max(m_IgnoreUntilUs, nowUs + 2000000);
        m_Measuring = true;
        m_ChangeGapUs = 0;
        m_ChangeKeyframe = false;
    }

    void setCeiling(int ceilingKbps)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ceiling = std::max(500, ceilingKbps);
        m_Floor = std::min(m_Floor, m_Ceiling);
        m_EffectiveFloor = std::min(m_EffectiveFloor, m_Floor);
        m_Target = std::min<double>(m_Target, m_Ceiling);
    }

    Status status() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Status;
    }

private:
    struct Stored {
        uint64_t arrivalUs = 0;
        int64_t queueUs = -1;
        uint32_t payloadBytes = 0;
        uint32_t dataPackets = 0;
        uint32_t lostPackets = 0;
    };
    struct Window {
        double seconds = 0;
        uint64_t payloadBytes = 0;
        uint32_t dataPackets = 0;
        uint32_t lostPackets = 0;
        int delayed = 0;
        int64_t minQueueUs = 0;
        int64_t medianQueueUs = 0;
        int64_t lastQueueUs = 0;
    };

    // Every 5s, so a route that got slower becomes the new normal within 10s.
    void rotateBaseline(uint64_t nowUs)
    {
        if (!m_BaselineAtUs) m_BaselineAtUs = nowUs;
        if (nowUs - m_BaselineAtUs >= 5000000 && m_HasBaseline) {
            m_BaselineOld = m_BaselineNow;
            m_BaselineNow = INT64_MAX;
            m_BaselineAtUs = nowUs;
        }
    }

    Window window(uint64_t nowUs, uint64_t spanUs) const
    {
        Window out;
        std::array<int64_t, 128> queues;
        int delayed = 0;
        uint64_t oldest = nowUs;
        for (size_t i = 1; i <= m_Count; ++i) {
            const Stored& frame = m_Frames[(m_Head + m_Frames.size() - i) % m_Frames.size()];
            if (frame.arrivalUs + spanUs < nowUs) break;
            out.payloadBytes += frame.payloadBytes;
            out.dataPackets += frame.dataPackets;
            out.lostPackets += frame.lostPackets;
            oldest = std::min(oldest, frame.arrivalUs);
            if (frame.queueUs >= 0 && delayed < int(queues.size())) {
                if (!delayed) out.lastQueueUs = frame.queueUs;
                queues[delayed++] = frame.queueUs;
            }
        }
        out.seconds = double(std::max<uint64_t>(nowUs - oldest, spanUs / 2)) / 1e6;
        out.delayed = delayed;
        if (delayed) {
            std::nth_element(queues.begin(), queues.begin() + delayed / 2, queues.begin() + delayed);
            out.medianQueueUs = queues[delayed / 2];
            out.minQueueUs = *std::min_element(queues.begin(), queues.begin() + delayed);
        }
        return out;
    }

    void measureChange(const FrameSample& frame, uint64_t previousUs)
    {
        const uint64_t sinceUs = std::max(previousUs, m_ChangeAtUs);
        const uint64_t gapUs = frame.firstArrivalUs > sinceUs ? frame.firstArrivalUs - sinceUs : 0;
        const bool stalled = gapUs >= uint64_t(RebuildStallMs) * 1000;
        if (stalled && frame.firstArrivalUs - m_ChangeAtUs < 10000000)
            m_IgnoreUntilUs = std::max(m_IgnoreUntilUs, frame.firstArrivalUs + 2000000);
        m_ChangeGapUs = std::max(m_ChangeGapUs, gapUs);
        m_ChangeKeyframe = m_ChangeKeyframe || frame.keyframe;
        // A stall ends with the first frame back; a change made in place shows within a second.
        if (stalled || frame.firstArrivalUs - m_ChangeAtUs >= 1000000) {
            m_Measuring = false;
            m_Status.changeStallMs = int(m_ChangeGapUs / 1000);
            m_Status.changeKeyframe = m_ChangeKeyframe;
            ++m_Status.changesMeasured;
        }
    }

    void backOff(double receivedKbps, bool severe)
    {
        const double factor = severe ? 0.7 : 0.85;
        // A quiet picture says nothing about the link, so only trust the received
        // rate when the encoder was actually using most of what it was given.
        const double from = receivedKbps >= 0.5 * m_Target ? std::min(receivedKbps, m_Target) : m_Target;
        m_LimitKbps = m_EverBackedOff && from > m_LimitKbps / 2 ? (m_LimitKbps + from) / 2 : from;
        m_EverBackedOff = true;
        // With a queue, what's arriving is what the link can carry, so that's as far as
        // this needs to go. While a queue drains, more cuts would only overshoot.
        const double proven = severe ? receivedKbps * 0.6 : receivedKbps * 0.75;
        m_Target = std::clamp<double>(std::max(std::min(from * factor, m_Target * 0.95), std::min(proven, m_Target)), m_EffectiveFloor, m_Ceiling);
    }

    void raise(double receivedKbps, double seconds, uint64_t nowUs)
    {
        const bool quiet = receivedKbps < 0.55 * m_Target;
        // Frames coming in far faster than the bitrate means the room is plainly there.
        // Paced frames can't come faster than they were sent, which is three times the
        // bitrate, so getting most of that through says the same.
        const bool plentyOfRoom = m_TrainKbps >= (nowUs < m_PaceUntilUs ? 2.5 : 4) * m_Target;
        double perSecond;
        Phase phase = Phase::Climbing;
        if (quiet) {
            // Nothing being tested, so drift back towards the top rather than
            // leave a temporary dip in place for the next burst of motion.
            perSecond = m_CleanSinceUs && nowUs - m_CleanSinceUs >= 5000000 ? m_Target * 0.02 : 0;
            phase = Phase::Steady;
        } else if (plentyOfRoom) {
            perSecond = m_Target * 0.30;
        } else if (!m_EverBackedOff) {
            perSecond = m_Target * 0.15;
        } else if (m_LimitKbps > 0 && m_Target >= 0.85 * m_LimitKbps && m_Target <= 1.15 * m_LimitKbps
                   && nowUs - m_DecreaseAtUs < 60000000) {
            // Close to where it last ran out of room: go carefully.
            perSecond = std::max(200.0, m_Target * 0.015);
        } else {
            // Well past that limit, or it was long ago: the link has changed.
            if (m_Target > 1.15 * m_LimitKbps) m_LimitKbps = 0;
            perSecond = m_Target * 0.08;
        }
        m_Target = std::min<double>(m_Target + perSecond * seconds, m_Ceiling);
        m_Status.phase = m_Target >= m_Ceiling ? Phase::Steady : phase;
    }

    mutable std::mutex m_Mutex;
    int m_Ceiling;
    int m_Floor;
    int m_EffectiveFloor;
    int64_t m_FrameUs;
    double m_Target;
    std::array<Stored, 512> m_Frames;
    size_t m_Head = 0;
    size_t m_Count = 0;
    bool m_HasBaseline = false;
    int64_t m_BaselineNow = 0;
    int64_t m_BaselineOld = 0;
    uint64_t m_BaselineAtUs = 0;
    uint32_t m_TypicalFrameBytes = 0;
    uint64_t m_BigFrameAtUs = 0;
    uint64_t m_LastTickUs = 0;
    uint64_t m_StartUs = 0;
    uint64_t m_DecreaseAtUs = 0;
    double m_QueueAtDecreaseMs = 0;
    uint64_t m_CleanSinceUs = 0;
    uint64_t m_LossSinceUs = 0;
    std::array<uint64_t, 8> m_LossBursts {};
    size_t m_LossBurstHead = 0;
    bool m_InLossBurst = false;
    uint64_t m_LossAtUs = 0;
    uint64_t m_PaceUntilUs = 0;
    double m_JitterMs = 2;
    double m_LimitKbps = 0;
    double m_TrainKbps = 0;
    bool m_EverBackedOff = false;
    bool m_SetOnce = false;
    bool m_EstimatedStamps = false;
    uint64_t m_LastArrivalUs = 0;
    uint64_t m_ChangeAtUs = 0;
    uint64_t m_IgnoreUntilUs = 0;
    bool m_Measuring = false;
    uint64_t m_ChangeGapUs = 0;
    bool m_ChangeKeyframe = false;
    int m_OverTicks = 0;
    int m_DrainFrames = 0;
    uint32_t m_DrainSequence = 0;
    Status m_Status;
};

}
