#pragma once

#include "streamadaptation.h"
#include <QCryptographicHash>

// A change for the next normal connection, never permission to interrupt video.
// Keep its baseline independent of the rolling capacity measurement so an
// unsuccessful trial can return to the previously observed picture.
struct StreamRecommendation {
    StreamPlanning::Rung target {0, 0, 0};
    HostCapacity baseline;
    qint64 createdAt = 0;
    bool rollback = false;
    int bitrateKbps = 0; // encoder units; zero means a picture-only recommendation

    bool valid(const QVector<StreamPlanning::Rung>& rungs, const QString& context,
               const QString& codec, qint64 now) const {
        if (!baseline.measured() || baseline.performanceContext != context || baseline.codec != codec ||
            createdAt <= 0 || now < createdAt || now - createdAt > StreamPlanning::OverloadMemorySeconds ||
            target.fps != baseline.fps || ((rollback || bitrateKbps > 0) != StreamAdaptation::same(target, StreamAdaptation::mode(baseline))) ||
            bitrateKbps < 0 || bitrateKbps > 500000 || (bitrateKbps > 0 && bitrateKbps < 500)) return false;
        for (const auto& rung : rungs) if (StreamAdaptation::same(rung, target)) return true;
        return false;
    }
    static QString key(const QString& host, const QString& context, const QString& codec) {
        return host + QStringLiteral("/recommendation/") + QString::fromLatin1(QCryptographicHash::hash(
            (context + QLatin1Char('/') + codec).toUtf8(), QCryptographicHash::Sha256).toHex());
    }
    void save(const QString& host, const QString& context, const QString& codec) const {
        const auto path = key(host, context, codec);
        baseline.save(path + QStringLiteral("/baseline"));
        QSettings settings;
        settings.beginGroup(HostCapacity::key(path));
        settings.setValue("width", target.width); settings.setValue("height", target.height);
        settings.setValue("fps", target.fps); settings.setValue("createdAt", createdAt);
        settings.setValue("rollback", rollback);
        settings.setValue("bitrateKbps", bitrateKbps);
    }
    static StreamRecommendation load(const QString& host, const QString& context, const QString& codec) {
        const auto path = key(host, context, codec);
        StreamRecommendation result;
        result.baseline = HostCapacity::load(path + QStringLiteral("/baseline"));
        QSettings settings;
        settings.beginGroup(HostCapacity::key(path));
        result.target = {settings.value("width").toInt(), settings.value("height").toInt(), settings.value("fps").toInt()};
        result.createdAt = settings.value("createdAt").toLongLong();
        result.rollback = settings.value("rollback").toBool();
        result.bitrateKbps = settings.value("bitrateKbps").toInt();
        return result;
    }
    static void clear(const QString& host, const QString& context, const QString& codec) {
        QSettings().remove(HostCapacity::key(key(host, context, codec)));
    }
};
