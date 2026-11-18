#pragma once

#include <QtGlobal>
#include <array>
#include <cmath>

// The host's time per frame (capture, convert, encode), as each frame's header reports it,
// kept for the whole stream so its average and 95th percentile describe what the host
// sustained. Owned by the session; the caller serialises access.
class HostLatencyMeter {
public:
    // Too short to say anything about the host.
    static constexpr quint64 MeaningfulUs = 20000000;

    // tenthsOfMs is the frame header's unit; 0 means the host didn't say.
    void addFrame(quint64 arrivalUs, quint16 tenthsOfMs, quint32 bytes = 0, quint32 sequence = 0)
    {
        if (!m_Frames) { m_FirstUs = arrivalUs; m_FirstSequence = sequence; }
        m_LastSequence = sequence;
        m_HasSequence = m_HasSequence || sequence != 0;
        m_LastUs = qMax(m_LastUs, arrivalUs);
        ++m_Frames;
        m_TotalBytes += bytes;
        if (!tenthsOfMs) return; // Still count repeated/untimed frames for cadence.
        ++m_LatencyFrames;
        m_TotalTenths += tenthsOfMs;
        ++m_Buckets[qMin<size_t>(tenthsOfMs / BucketTenths, m_Buckets.size() - 1)];
    }

    quint64 frames() const { return m_Frames; }
    quint64 spanUs() const { return m_Frames ? m_LastUs - m_FirstUs : 0; }
    bool meaningful() const { return spanUs() >= MeaningfulUs && m_Frames >= 100; }
    // Enough to tell a host that plainly can't keep up, a few seconds in.
    bool enoughToJudge() const { return spanUs() >= 5000000 && m_Frames >= 90; }
    double averageMs() const { return m_LatencyFrames ? m_TotalTenths / 10.0 / m_LatencyFrames : 0; }
    double percentileMs(double fraction) const
    {
        if (!m_LatencyFrames) return 0;
        const quint64 wanted = qMax<quint64>(1, quint64(std::ceil(fraction * m_LatencyFrames - 1e-6)));
        quint64 seen = 0;
        for (size_t i = 0; i < m_Buckets.size(); ++i) {
            seen += m_Buckets[i];
            if (seen >= wanted) return (i + 1) * BucketTenths / 10.0;
        }
        return m_Buckets.size() * BucketTenths / 10.0;
    }
    double p95Ms() const { return percentileMs(0.95); }
    double sourceFps() const { return spanUs() && m_Frames > 1 ? (m_HasSequence ? quint32(m_LastSequence - m_FirstSequence) : m_Frames - 1) * 1e6 / spanUs() : 0; }
    double deliveredFps() const { return spanUs() && m_Frames > 1 ? (m_Frames - 1) * 1e6 / spanUs() : 0; }
    // Reject sparse, tiny updates. This is only an activity filter; host capture
    // cadence, when available, strengthens the evidence for a reversible trial.
    bool demandObserved(int width, int height) const {
        return enoughToJudge() && m_Frames && m_TotalBytes / m_Frames >= quint64(width) * height / 500;
    }

private:
    // Half-millisecond steps up to a second; anything slower lands in the last.
    static constexpr int BucketTenths = 5;
    std::array<quint32, 2000> m_Buckets {};
    quint64 m_Frames = 0;
    quint64 m_LatencyFrames = 0;
    quint64 m_TotalTenths = 0;
    quint64 m_TotalBytes = 0;
    quint32 m_FirstSequence = 0, m_LastSequence = 0;
    bool m_HasSequence = false;
    quint64 m_FirstUs = 0;
    quint64 m_LastUs = 0;
};
