#pragma once

#include <QSize>
#include <QtMath>

namespace WindowGeometryPolicy {
// SDL stores this floating size while fullscreen. Leave room for decorations
// and desktop panels when it restores the window, even at native resolution.
inline QSize initialWindowedSize(const QSize& stream, const QSize& usable)
{
    if (stream.isEmpty() || usable.isEmpty()) return {};
    const QSize available(qMax(2, qFloor(usable.width() * .8) & ~1),
                          qMax(2, qFloor(usable.height() * .8) & ~1));
    if (stream.width() <= available.width() && stream.height() <= available.height()) return stream;
    const auto fit = stream.scaled(available, Qt::KeepAspectRatio);
    return {qMax(2, fit.width() & ~1), qMax(2, fit.height() & ~1)};
}
}
