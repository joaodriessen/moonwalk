#include "automaticbitratedriver.h"
#include "../../moonlight-common-c/adaptivefeedback.h"
#include <QElapsedTimer>
#include <Limelight.h>
#include <SDL.h>

AutomaticBitrateDriver::AutomaticBitrateDriver(int startKbps, int ceilingKbps, int framesPerSecond, int floorKbps,
                                               bool hostStampsEveryFrame, Deliver deliver, bool changesRebuild)
    : m_Controller(startKbps, ceilingKbps, framesPerSecond, floorKbps, startKbps < floorKbps), m_InitialKbps(startKbps),
      m_HostStampsEveryFrame(hostStampsEveryFrame), m_Deliver(std::move(deliver)), m_Rebuilt(changesRebuild)
{
    setObjectName("Automatic bitrate");
    m_Controller.setStampsEstimated(!hostStampsEveryFrame);
}

AutomaticBitrateDriver::~AutomaticBitrateDriver() { stopAndWait(); }

void AutomaticBitrateDriver::observeConnection()
{
    StarlightSetFrameObserver([](const StarlightFrameObservation* frame, void* context) {
        static_cast<AutomaticBitrateDriver*>(context)->onObservation(frame);
    }, this);
}

void AutomaticBitrateDriver::stopAndWait()
{
    { QMutexLocker lock(&m_Mutex); m_Stopping = true; m_Wake.wakeAll(); }
    if (isRunning()) wait();
}

int AutomaticBitrateDriver::settledKbps() const
{
    return m_Seconds >= 20 && (!m_HitFloor || m_CutForLoss) ? int(m_Settled) : 0;
}

void AutomaticBitrateDriver::changeApplied()
{
    m_Controller.changeApplied(LiGetMicroseconds());
}

uint64_t AutomaticBitrateDriver::Unwrapped::next(uint32_t value)
{
    if (started && value < last && last - value > 0x80000000u) high += uint64_t(1) << 32;
    started = true;
    last = value;
    return high + value;
}

void AutomaticBitrateDriver::onObservation(const void* raw)
{
    const auto* frame = static_cast<const StarlightFrameObservation*>(raw);
    AutomaticBitrate::FrameSample sample;
    sample.firstArrivalUs = frame->firstArrivalUs;
    sample.lastArrivalUs = frame->lastArrivalUs;
    sample.payloadBytes = frame->payloadBytes;
    sample.dataPackets = frame->dataPackets;
    sample.lostPackets = frame->lostPackets;
    sample.keyframe = frame->keyframe;
    // When it left the host. Skylight says outright. Otherwise it was captured at this time
    // and then spent this long in the host; a repeated frame has no latency to go on.
    if (m_HostStampsEveryFrame) {
        if (frame->sendStamp) sample.sentUs = m_Stamp.next(frame->sendStamp) * 100 / 9 + 1;
    }
    else if (frame->hostLatency) {
        sample.sentUs = m_Rtp.next(frame->rtpTimestamp) * 100 / 9 + uint64_t(frame->hostLatency) * 100 + 1;
    }
    m_Controller.onFrame(sample);
    static const bool frameTrace = qEnvironmentVariableIntValue("STARLIGHT_BITRATE_TRACE") >= 2;
    if (frameTrace) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "frame-trace rtp=%u sent=%llu first=%llu last=%llu bytes=%u packets=%u lost=%u host=%u",
                    frame->rtpTimestamp,
                    (unsigned long long)sample.sentUs, (unsigned long long)sample.firstArrivalUs, (unsigned long long)sample.lastArrivalUs,
                    sample.payloadBytes, sample.dataPackets, sample.lostPackets, unsigned(frame->hostLatency));
    }
}

QByteArray AutomaticBitrateDriver::report(const AutomaticBitrate::Decision& decision)
{
    QByteArray bytes(20, '\0');
    const auto put = [&bytes](int at, uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes[at + i] = char((value >> (8 * i)) & 0xFF);
    };
    bytes[0] = 1;
    bytes[1] = char(qBound(0, decision.drainFrames, 8));
    bytes[2] = char(qBound(0, decision.lossPercent, 100));
    put(4, uint32_t(decision.targetKbps));
    put(8, uint32_t(decision.paceKbps));
    put(12, uint32_t(qBound(0, decision.queueMs, 65535)));
    put(16, decision.drainSequence);
    return bytes;
}

void AutomaticBitrateDriver::run()
{
    QElapsedTimer clock; clock.start();
    const bool trace = qEnvironmentVariableIsSet("STARLIGHT_BITRATE_TRACE");
    AutomaticBitrate::Decision sent;
    qint64 sentAt = -1, loggedAt = -1, tallyAt = 0;
    int loggedKbps = 0;
    uint32_t measured = 0;
    while (true) {
        { QMutexLocker lock(&m_Mutex);
          if (!m_Stopping) m_Wake.wait(&m_Mutex, 100);
          if (m_Stopping) return; }
        const qint64 now = clock.elapsed();
        const auto decision = m_Controller.tick(LiGetMicroseconds());
        if (trace) {
            const auto status = m_Controller.status();
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "bitrate-trace t=%lld target=%d recv=%d queue=%.1f jitter=%.1f loss=%.2f spread=%.1f pace=%d drains=%u phase=%d",
                        now, status.targetKbps, status.receivedKbps, status.queueMs, status.jitterMs, status.lossPercent, status.frameSpreadMs, decision.paceKbps, decision.drainSequence, int(status.phase));
        }
        const bool changed = abs(decision.targetKbps - sent.targetKbps) * 50 >= qMax(1, sent.targetKbps)
                             || decision.paceKbps != sent.paceKbps
                             || decision.drainSequence != sent.drainSequence;
        if (sentAt < 0 || changed || now - sentAt >= 500) {
            m_Deliver(decision);
            if (changed) emit statusChanged(decision.targetKbps);
            sent = decision;
            sentAt = now;
        }
        {
            const auto status = m_Controller.status();
            if (status.changesMeasured != measured) {
                measured = status.changesMeasured;
                // Once a change has stopped the video, the next one will too. One that
                // didn't lets the bitrate follow the link from here on.
                m_Rebuilt = m_Rebuilt || status.changeStallMs >= AutomaticBitrate::RebuildStallMs;
                m_Controller.setSetOnce(!AutomaticBitrate::continuousChanges(m_HostStampsEveryFrame, !m_Rebuilt));
                emit changeMeasured(status.changeStallMs, status.changeKeyframe);
            }
            m_HitFloor = m_HitFloor || decision.targetKbps <= status.floorKbps;
            m_CutForLoss = status.cutForLoss || status.cutForCongestion;
        }
        if (now - tallyAt >= 1000) {
            tallyAt = now;
            m_Settled = m_Seconds ? m_Settled + (decision.targetKbps - m_Settled) / qMin(m_Seconds + 1, 60)
                                  : double(decision.targetKbps);
            ++m_Seconds;
        }
        if (loggedAt < 0 || abs(decision.targetKbps - loggedKbps) * 8 >= qMax(1, loggedKbps) || now - loggedAt >= 30000) {
            const auto status = m_Controller.status();
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Automatic bitrate: target %d kbps, received %d kbps, queue %.1f ms, jitter %.1f ms, loss %.2f%%, frame spread %.1f ms",
                        status.targetKbps, status.receivedKbps, status.queueMs, status.jitterMs, status.lossPercent, status.frameSpreadMs);
            loggedAt = now;
            loggedKbps = decision.targetKbps;
        }
    }
}
