#pragma once

#include <QSizeF>
#include <QString>
#include <cmath>

namespace DesktopScalePolicy {
inline QString forViewport(const QString& preference, const QSize& video, const QSizeF& logicalViewport)
{
    if (preference != QStringLiteral("auto") || video.width() <= 0 || video.height() <= 0 ||
            !std::isfinite(logicalViewport.width()) || !std::isfinite(logicalViewport.height()) ||
            logicalViewport.width() <= 0 || logicalViewport.height() <= 0)
        return preference;

    // Aspect fit can leave bars on either axis. Pick the supported desktop
    // scale whose UI is closest to local logical size in the video region.
    // The host only supports exact 1x/2x backing stores, so odd sizes need 1x.
    const qreal videoPixelsPerPoint = qMax(video.width() / logicalViewport.width(),
                                          video.height() / logicalViewport.height());
    return !(video.width() % 2) && !(video.height() % 2) && videoPixelsPerPoint >= 1.5
            ? QStringLiteral("2") : QStringLiteral("1");
}
}
