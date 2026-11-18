#pragma once

#include "hostcapacity.h"
#include "hostlatencymeter.h"

#include <Limelight.h>
#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <cmath>

// What to stream to this display from this host. Automatic aims for the display's own
// size and refresh, and steps down only when the host has shown it can't keep up.
// Manual choices are kept exactly, with a plain sentence for each one expected to hurt.
struct StreamPlanInput {
    int nativeWidth = 0, nativeHeight = 0;   // local display, physical pixels
    int displayHz = 0;                       // local refresh
    double displayScale = 0;                 // local desktop scale (2 for this Retina panel); 0 unknown
    bool integerDesktopScale = false;        // host offers only 1x/2x logical backing modes
    // Client HARDWARE decode support.
    bool h264 = true, hevc = false, hevc10 = false, av1 = false, av1_10 = false, yuv444 = false;
    quint32 serverCodecModeSupport = SCM_H264; // from serverinfo
    HostCapacity capacity;                   // remembered for this host
    // Manual choices, 0/empty = Automatic:
    int width = 0, height = 0, fps = 0, bitrateKbps = 0;
    // A next-connection or explicitly applied trial, independent of rolling learning.
    int trialWidth = 0, trialHeight = 0;
    QString codec;                           // "H.264", "HEVC", "AV1"
    // For the sentences, and when a remembered limit has gone stale.
    QString hostName;
    QString performanceContext;
    qint64 now = 0;                          // seconds since the epoch; 0 = now
};

struct StreamPlan {
    int width = 0, height = 0, fps = 0;
    QString codec;
    int startBitrateKbps = 0, ceilingKbps = 0, floorKbps = 0; // wire budget at launch
    bool limitedByHost = false;  // Automatic chose below native because of the host
    QString limitReason;         // evidence-qualified explanation of a planned trial
    // For each Manual choice the plan expects to hurt, a plain-language consequence.
    // Empty when fine.
    QString resolutionWarning, fpsWarning, codecWarning, bitrateWarning;
};

namespace StreamPlanning {

struct AdaptationEvidence {
    bool active = false;
    bool slow = false;
    bool canTrySize = false;
    double deliveredFps = 0;
};

inline AdaptationEvidence adaptationEvidence(const HostLatencyMeter& meter, int width, int height, int fps,
                                             bool captureSupported, double captureFps, bool sourceHealthy,
                                             bool networkClean)
{
    AdaptationEvidence result;
    result.deliveredFps = meter.deliveredFps();
    const bool hostDelay = meter.averageMs() * fps / 1000 > 1.25;
    // Payload activity is not proof of encoder demand. Without an extension,
    // it permits only a bounded trial whose benefit must be verified.
    result.active = meter.demandObserved(width, height)
        && (!captureSupported || (sourceHealthy &&
            (captureFps >= fps * .9 || (captureFps > 0 && hostDelay))));
    const bool lostCadence = fps > 0 && result.deliveredFps < fps * .85;
    const bool missingFrames = result.deliveredFps < meter.sourceFps() * .95;
    // Capture/composition can be slow before the host's processing timestamp.
    // Sequence cadence distinguishes that producer limit from missing wire frames.
    const bool sourceLimited = meter.sourceFps() < fps * .85;
    result.slow = result.active && lostCadence && (hostDelay || sourceLimited || missingFrames || !networkClean);
    // Quality is the first remedy for missing delivery. Pixel work is tried
    // when timing or the producer's own cadence also supports that hypothesis.
    result.canTrySize = result.slow && (hostDelay || sourceLimited);
    return result;
}

inline QString tr(const char* text) { return QCoreApplication::translate("StreamPlan", text); }

inline QString viewingContext(const QString& route, int width, int height, int hz, double scale)
{
    return route + QStringLiteral("/%1x%2@%3/%4/desktop-v2").arg(width).arg(height).arg(hz).arg(scale,0,'f',3);
}

inline QString decoderContext(const QString& screen, int width, int height, int fps, double scale, int selection)
{
    return QStringLiteral("%1/%2x%3@%4/%5/%6/%7").arg(screen).arg(width).arg(height).arg(fps)
        .arg(scale, 0, 'f', 3).arg(selection).arg(QLatin1String(qVersion()));
}

struct Rung {
    int width, height, fps;
    double pixelRate() const { return double(width) * height * fps; }
};

inline int even(double value) { return qMax(2, int(std::lround(value / 2)) * 2); }

// The display's own size, then smaller ones, with the frame rate kept at the display's
// throughout adaptation. On a scaled display each smaller size
// is this desktop's own layout at one of Windows' scale steps: 2880×1800 at 2× gives
// 2520×1576, 2160×1350, 1800×1126 and 1440×900, at 175, 150, 125 and 100%, so a host
// that picks its scale from the size lays its desktop out exactly as this one. Below
// that, preserve the logical desktop. Unscaled displays start with small steps too.
inline QVector<Rung> ladder(int width, int height, int fps, bool fpsAutomatic, double displayScale = 0, bool integerScale = false)
{
    QVector<Rung> rungs {{width, height, fps}};
    double logicalWidth = width, logicalHeight = height;
    if (displayScale >= 1.2) {
        logicalWidth = width / displayScale;
        logicalHeight = height / displayScale;
        for (const double scale : {5.0, 4.5, 4.0, 3.5, 3.0, 2.5, 2.25, 2.0, 1.75, 1.5, 1.25, 1.0}) {
            const Rung rung {even(logicalWidth * scale), even(logicalHeight * scale), fps};
            if (rung.width >= width * 0.97 || rung.height < 360) continue;
            if (integerScale && scale != 1.0 && scale != 2.0) continue;
            rungs.append(rung);
        }
    }
    // Never shrink a scaled desktop below its logical 1x layout.
    for (const double scale : {7.0 / 8, 3.0 / 4, 2.0 / 3, 1.0 / 2, 1.0 / 3}) {
        if (displayScale >= 1.2) break;
        const Rung rung {even(logicalWidth * scale), even(logicalHeight * scale), fps};
        if (rung.height < 360) break;
        if (rung.width < rungs.last().width * 0.97) rungs.append(rung);
    }
    Q_UNUSED(fpsAutomatic); // Automatic preserves the viewing display cadence.
    return rungs;
}

// Pipeline delay in frame intervals. This is a latency signal, never throughput
// evidence by itself: a pipelined encoder can have multiple frames in flight.
inline double load(double milliseconds, int fps) { return fps > 0 ? milliseconds * fps / 1000 : 0; }
inline bool overloaded(double averageMs, double p95Ms, int fps)
{
    return load(averageMs, fps) > 1 || load(p95Ms, fps) > 2;
}

// Healthy observed cadence permits a bounded larger-mode trial. A size actually
// seen failing is retried only after a quiet recovery interval.
constexpr int MaximumAdjustments = 4;
constexpr qint64 SampleLifetimeSeconds = 7 * 24 * 3600;
constexpr qint64 OverloadMemorySeconds = 24 * 3600;

inline bool rememberedOverload(const HostCapacity& capacity, double pixelRate, qint64 now)
{
    return capacity.overloadedPixelRate > 0 && pixelRate >= capacity.overloadedPixelRate * 0.95
        && now - capacity.overloadedAt < OverloadMemorySeconds;
}

// The end of a stream long enough to tell: what the host managed, and whether it kept up.
inline void remember(HostCapacity& capacity, int width, int height, int fps, const QString& codec,
                     double averageMs, double p95Ms, qint64 now,
                     double sourceFps = 0, bool demandObserved = false, double deliveredFps = 0)
{
    capacity.measuredAt = now;
    capacity.width = width;
    capacity.height = height;
    capacity.fps = fps;
    capacity.codec = codec;
    capacity.hostLatencyAverageMs = averageMs;
    capacity.hostLatencyP95Ms = p95Ms;
    capacity.sourceFps = sourceFps;
    capacity.deliveredFps = deliveredFps > 0 ? deliveredFps : sourceFps;
    capacity.demandObserved = demandObserved;
    const qint64 pixelRate = qint64(width) * height * fps;
    if (demandObserved && capacity.cadenceFps() > 0 && capacity.cadenceFps() < fps * 0.85
            && overloaded(averageMs, p95Ms, fps)) {
        // Remember only a mode actually seen failing, not an extrapolated lower limit.
        const qint64 limit = pixelRate;
        const bool stale = now - capacity.overloadedAt >= OverloadMemorySeconds;
        if (!capacity.overloadedPixelRate || stale || limit < capacity.overloadedPixelRate)
            capacity.overloadedPixelRate = limit;
        capacity.overloadedAt = now;
    }
    else if (capacity.overloadedPixelRate && pixelRate >= capacity.overloadedPixelRate) {
        capacity.overloadedPixelRate = capacity.overloadedAt = 0;
    }
}

// A smaller picture has to buy measurable cadence or latency improvement.
inline bool reductionHelped(const HostCapacity& before, double sourceFps, double latencyMs)
{
    return sourceFps >= before.cadenceFps() * 1.10
        || (before.hostLatencyAverageMs > 0 && sourceFps >= before.cadenceFps() * .98 && latencyMs <= before.hostLatencyAverageMs * .75);
}

struct Budget { int startKbps, ceilingKbps, floorKbps; };

// Content-dependent starting estimates for bits per pixel per frame, with codec
// efficiency and room for desktop motion. These are not quality guarantees. Past 60 fps each frame
// differs less from the last, so the rate grows more slowly.
inline Budget budget(const QString& codec, int width, int height, int fps)
{
    const double efficiency = codec == QLatin1String("AV1") ? 0.6 : codec == QLatin1String("HEVC") ? 0.7 : 1.0;
    const double motionFps = fps <= 60 ? fps : 60 * std::sqrt(fps / 60.0);
    const double pixels = double(width) * height * motionFps * efficiency;
    const auto kbps = [pixels](double bitsPerPixel) {
        return qBound(1000, int(std::lround(pixels * bitsPerPixel / 100000)) * 100, 150000);
    };
    return {kbps(0.16), kbps(0.20), kbps(0.04)};
}

inline QString size(int width, int height) { return QStringLiteral("%1×%2").arg(width).arg(height); }

} // namespace StreamPlanning

inline StreamPlan planStream(const StreamPlanInput& in)
{
    using namespace StreamPlanning;
    StreamPlan plan;
    const QString host = in.hostName.isEmpty() ? tr("The host") : in.hostName;
    const qint64 now = in.now > 0 ? in.now : QDateTime::currentSecsSinceEpoch();
    const bool manualSize = in.width > 0 && in.height > 0;
    const bool manualFps = in.fps > 0;
    const int nativeWidth = in.nativeWidth > 0 ? in.nativeWidth : manualSize ? in.width : 1920;
    const int nativeHeight = in.nativeHeight > 0 ? in.nativeHeight : manualSize ? in.height : 1080;
    const int displayHz = in.displayHz > 0 ? in.displayHz : 60;

    // Codec: the best this computer decodes in hardware that the host encodes.
    const quint32 modes = in.serverCodecModeSupport ? in.serverCodecModeSupport : SCM_H264;
    const bool hostHevc = modes & SCM_HEVC, hostAv1 = modes & SCM_AV1_MAIN8;
    const QString automaticCodec = in.hevc && hostHevc ? QStringLiteral("HEVC")
        : in.av1 && hostAv1 ? QStringLiteral("AV1") : QStringLiteral("H.264");
    plan.codec = in.codec.isEmpty() ? automaticCodec : in.codec;
    if (!in.codec.isEmpty()) {
        const bool decodes = in.codec == QLatin1String("AV1") ? in.av1 : in.codec == QLatin1String("HEVC") ? in.hevc : in.h264;
        const bool encodes = in.codec == QLatin1String("AV1") ? hostAv1 : in.codec == QLatin1String("HEVC") ? hostHevc : true;
        if (!encodes)
            plan.codecWarning = tr("%1 can't send %2, so the stream will use %3 instead.").arg(host, in.codec, automaticCodec);
        else if (!decodes)
            plan.codecWarning = tr("This computer can't decode %1 in hardware, which may reduce performance.").arg(in.codec);
    }

    // Size and frame rate.
    const HostCapacity& capacity = in.capacity;
    const bool known = capacity.measured() && capacity.demandObserved && capacity.cadenceFps() > 0
        && capacity.codec == plan.codec && (in.performanceContext.isEmpty() || in.performanceContext == capacity.performanceContext)
        && now >= capacity.measuredAt
        && now - capacity.measuredAt <= SampleLifetimeSeconds;
    const Rung measured {capacity.width, capacity.height, capacity.fps};
    const bool measuredOverloaded = known && capacity.cadenceFps() < capacity.fps * 0.85
        && overloaded(capacity.hostLatencyAverageMs, capacity.hostLatencyP95Ms, capacity.fps);
    const int topFps = manualFps ? in.fps : displayHz;
    const QVector<Rung> rungs = manualSize ? QVector<Rung> {{in.width, in.height, topFps}}
                                           : ladder(nativeWidth, nativeHeight, topFps, !manualFps, in.displayScale, in.integerDesktopScale);
    int chosen = 0;
    enum { Unlimited, Stays, Steps } why = Unlimited;
    if (!manualSize && known) {
        const double rate = measured.pixelRate();
        if (measuredOverloaded && now >= capacity.reductionRejectedUntil) {
            // Probe one supported step, never extrapolate a large pixel cut from
            // a bad frame rate. Each trial must demonstrate improvement.
            chosen = int(rungs.size()) - 1;
            for (int i = 0; i < rungs.size(); ++i) {
                if (rungs[i].pixelRate() < rate * .97) { chosen = i; break; }
            }
            why = Steps;
        }
        else {
            chosen = int(rungs.size()) - 1;
            for (int i = 0; i < rungs.size(); ++i) {
                if (rungs[i].pixelRate() <= rate * 1.15) { chosen = i; break; }
            }
            if (chosen > 0 && now >= capacity.reductionRejectedUntil) {
                // Spare encoder throughput cannot be inferred from pipeline delay.
                // A healthy cadence permits one bounded trial after failure memory expires.
                if (capacity.cadenceFps() >= capacity.fps * .97
                        && !rememberedOverload(capacity, rungs[chosen - 1].pixelRate(), now))
                    --chosen;
            }
            why = Stays;
        }
    }
    if (!manualSize && in.trialWidth > 0 && in.trialHeight > 0) {
        // Never substitute a cache-derived mode for an explicit in-flight trial.
        // Only current display/host scale rungs are eligible.
        for (int i = 0; i < rungs.size(); ++i) {
            if (rungs[i].width == in.trialWidth && rungs[i].height == in.trialHeight) {
                chosen = i;
                why = Stays;
                break;
            }
        }
    }
    const Rung rung = rungs[chosen];
    plan.width = rung.width;
    plan.height = rung.height;
    plan.fps = rung.fps;
    plan.limitedByHost = !manualSize && chosen > 0;
    if (plan.limitedByHost) {
        const QString target = size(rung.width, rung.height);
        if (why == Steps)
            plan.limitReason = tr("%1 delivered about %2 fps at %3 when %4 fps was requested. Automatic will try %5 at %6 fps")
                .arg(host).arg(qRound(capacity.cadenceFps())).arg(size(measured.width, measured.height))
                .arg(measured.fps).arg(target).arg(rung.fps);
        else
            plan.limitReason = tr("Trying %1 at %2 fps, based on the last measurement from %3 at %4").arg(target).arg(rung.fps).arg(host, size(measured.width, measured.height));
    }

    // What a Manual size or frame rate will cost on this host.
    QString hostCost;
    if (known && (manualSize || manualFps)) {
        const Rung asked = rung;
        const double growth = asked.pixelRate() / measured.pixelRate();
        if (capacity.cadenceFps() < capacity.fps * 0.85 || growth > 1.15) {
            const int managed = qMax(1, qMin(asked.fps, qRound(capacity.cadenceFps())));
            if (asked.width == measured.width && asked.height == measured.height)
                hostCost = tr("%1 delivered about %2 fps at this size last time.").arg(host).arg(managed);
            else
                hostCost = tr("This size has not been measured. Last time %1 delivered about %2 fps with %3 ms host latency at %4.")
                    .arg(host).arg(managed).arg(qRound(capacity.hostLatencyAverageMs)).arg(size(measured.width, measured.height));
        }
    }
    const auto join = [](const QString& first, const QString& second) {
        return first.isEmpty() ? second : second.isEmpty() ? first : first + QLatin1Char(' ') + second;
    };
    if (manualSize) {
        QString display;
        if (in.width > nativeWidth || in.height > nativeHeight)
            display = tr("Larger than this display (%1), so the extra detail is scaled away while %2 still has to encode it.")
                .arg(size(nativeWidth, nativeHeight), host);
        else if (in.width < nativeWidth && in.height < nativeHeight)
            display = tr("Smaller than this display (%1), so the picture will be softer.").arg(size(nativeWidth, nativeHeight));
        plan.resolutionWarning = join(hostCost, display);
    }
    if (manualFps) {
        QString display;
        if (in.fps > displayHz)
            display = tr("This display shows at most %1 fps, so the extra frames are never seen.").arg(displayHz);
        else if (in.fps < displayHz)
            display = tr("Motion will be less smooth than this display's %1 Hz.").arg(displayHz);
        plan.fpsWarning = join(manualSize ? QString() : hostCost, display);
    }

    // Bitrate from what is sent, not a flat figure.
    const Budget needs = budget(plan.codec, plan.width, plan.height, plan.fps);
    plan.startBitrateKbps = needs.startKbps;
    plan.ceilingKbps = needs.ceilingKbps;
    plan.floorKbps = needs.floorKbps;
    if (in.bitrateKbps > 0) {
        plan.startBitrateKbps = plan.ceilingKbps = in.bitrateKbps;
        plan.floorKbps = qMin(needs.floorKbps, in.bitrateKbps);
        if (in.bitrateKbps < needs.floorKbps)
            plan.bitrateWarning = tr("%1 Mbps is less than the %2 Mbps this stream needs, so moving detail may look blocky.")
                .arg(in.bitrateKbps / 1000.0, 0, 'f', 1).arg(needs.floorKbps / 1000.0, 0, 'f', 1);
        else if (in.bitrateKbps > needs.ceilingKbps * 2)
            plan.bitrateWarning = tr("Above the estimated budget for this stream (about %1 Mbps). Extra bandwidth may add delay without useful detail.")
                .arg(needs.ceilingKbps / 1000.0, 0, 'f', 0);
    }
    return plan;
}
