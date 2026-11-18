#pragma once

#include <QSettings>
#include <QString>

// What a host has shown it can do, learned from real streams and remembered per
// host, so the next stream starts at something it sustains without anyone
// choosing settings. Kept beside the remembered automatic bitrate.
struct HostCapacity
{
    // How a mid-stream bitrate change lands on this host.
    enum class BitrateChange { Unknown, Live, Rebuild };

    // The last stream it was measured on; 0 when never measured.
    qint64 measuredAt = 0; // seconds since the epoch
    int width = 0;
    int height = 0;
    int fps = 0;
    QString performanceContext; // host build, route and viewing display; not universal hardware capacity
    QString bitrateContext;
    qint64 bitrateMeasuredAt = 0;
    QString codec; // "H.264", "HEVC", "AV1"
    // Time the host spends on each frame (capture, convert, encode), from the
    // video stream's frame headers.
    double hostLatencyAverageMs = 0;
    double hostLatencyP95Ms = 0;
    // Observed delivery under changing content, not an inverse of pipeline latency.
    // Zero/false also identifies old latency-only records, which cannot prove capacity.
    double sourceFps = 0;
    double deliveredFps = 0;
    double cadenceFps() const { return deliveredFps > 0 ? deliveredFps : sourceFps; }
    bool demandObserved = false;
    BitrateChange bitrateChange = BitrateChange::Unknown;
    // Longest gap in frames that followed a bitrate change, when one was seen.
    int bitrateChangeStallMs = 0;
    // The smallest stream (pixels a second) the host was seen unable to keep up with,
    // and when, so a stream that fits isn't grown back into one that didn't; 0 if none.
    qint64 overloadedPixelRate = 0;
    qint64 reductionRejectedUntil = 0;
    qint64 overloadedAt = 0; // seconds since the epoch

    bool measured() const { return measuredAt > 0 && width > 0 && height > 0 && fps > 0; }

    static QString key(const QString& uuid) { return QStringLiteral("hostcapacity/") + uuid; }

    static HostCapacity load(const QString& uuid)
    {
        HostCapacity result;
        if (uuid.isEmpty()) return result;
        QSettings settings;
        settings.beginGroup(key(uuid));
        result.measuredAt = settings.value(QStringLiteral("measuredAt"), 0).toLongLong();
        result.width = settings.value(QStringLiteral("width"), 0).toInt();
        result.height = settings.value(QStringLiteral("height"), 0).toInt();
        result.fps = settings.value(QStringLiteral("fps"), 0).toInt();
        result.performanceContext = settings.value(QStringLiteral("performanceContext")).toString();
        result.bitrateContext = settings.value(QStringLiteral("bitrateContext")).toString();
        result.bitrateMeasuredAt = settings.value(QStringLiteral("bitrateMeasuredAt"), 0).toLongLong();
        result.codec = settings.value(QStringLiteral("codec")).toString();
        result.hostLatencyAverageMs = settings.value(QStringLiteral("hostLatencyAverageMs"), 0).toDouble();
        result.hostLatencyP95Ms = settings.value(QStringLiteral("hostLatencyP95Ms"), 0).toDouble();
        result.sourceFps = settings.value(QStringLiteral("sourceFps"), 0).toDouble();
        result.deliveredFps = settings.value(QStringLiteral("deliveredFps"), 0).toDouble();
        result.demandObserved = settings.value(QStringLiteral("demandObserved"), false).toBool();
        const QString change = settings.value(QStringLiteral("bitrateChange")).toString();
        result.bitrateChange = change == QLatin1String("live") ? BitrateChange::Live
            : change == QLatin1String("rebuild") ? BitrateChange::Rebuild : BitrateChange::Unknown;
        result.bitrateChangeStallMs = settings.value(QStringLiteral("bitrateChangeStallMs"), 0).toInt();
        result.overloadedPixelRate = settings.value(QStringLiteral("overloadedPixelRate"), 0).toLongLong();
        result.reductionRejectedUntil = settings.value(QStringLiteral("reductionRejectedUntil"), 0).toLongLong();
        result.overloadedAt = settings.value(QStringLiteral("overloadedAt"), 0).toLongLong();
        return result;
    }

    void save(const QString& uuid) const
    {
        if (uuid.isEmpty()) return;
        QSettings settings;
        settings.beginGroup(key(uuid));
        settings.setValue(QStringLiteral("measuredAt"), measuredAt);
        settings.setValue(QStringLiteral("width"), width);
        settings.setValue(QStringLiteral("height"), height);
        settings.setValue(QStringLiteral("fps"), fps);
        settings.setValue(QStringLiteral("codec"), codec);
        settings.setValue(QStringLiteral("performanceContext"), performanceContext);
        settings.setValue(QStringLiteral("bitrateContext"), bitrateContext);
        settings.setValue(QStringLiteral("bitrateMeasuredAt"), bitrateMeasuredAt);
        settings.setValue(QStringLiteral("hostLatencyAverageMs"), hostLatencyAverageMs);
        settings.setValue(QStringLiteral("hostLatencyP95Ms"), hostLatencyP95Ms);
        settings.setValue(QStringLiteral("sourceFps"), sourceFps);
        settings.setValue(QStringLiteral("deliveredFps"), deliveredFps);
        settings.setValue(QStringLiteral("demandObserved"), demandObserved);
        settings.setValue(QStringLiteral("bitrateChange"), bitrateChange == BitrateChange::Live ? QStringLiteral("live")
            : bitrateChange == BitrateChange::Rebuild ? QStringLiteral("rebuild") : QStringLiteral("unknown"));
        settings.setValue(QStringLiteral("bitrateChangeStallMs"), bitrateChangeStallMs);
        settings.setValue(QStringLiteral("overloadedPixelRate"), overloadedPixelRate);
        settings.setValue(QStringLiteral("overloadedAt"), overloadedAt);
        settings.setValue(QStringLiteral("reductionRejectedUntil"), reductionRejectedUntil);
    }
};
