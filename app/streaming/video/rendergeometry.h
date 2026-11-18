#pragma once

#include <QRectF>
#include <QtMath>
#include "SDL_compat.h"

// Shared output transforms. Input always maps through the original top-left
// destination; clipping source and destination by the same proportion preserves
// that mapping on APIs whose output rectangles cannot contain negative values.
namespace RenderGeometry {
inline SDL_Rect lowerLeft(SDL_Rect topLeft, int viewportHeight)
{
    topLeft.y = viewportHeight - topLeft.y - topLeft.h;
    return topLeft;
}
inline bool same(const SDL_Rect& a, const SDL_Rect& b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
struct Clipped {
    QRectF source;
    SDL_Rect destination {};
    bool empty() const { return source.isEmpty() || destination.w <= 0 || destination.h <= 0; }
};
inline Clipped clip(SDL_Rect source, SDL_Rect destination, SDL_Rect viewport)
{
    if (source.w <= 0 || source.h <= 0 || destination.w <= 0 || destination.h <= 0) return {};
    const QRect visible = QRect(destination.x, destination.y, destination.w, destination.h)
        .intersected(QRect(viewport.x, viewport.y, viewport.w, viewport.h));
    if (visible.isEmpty()) return {};
    const qreal xScale = qreal(source.w) / destination.w;
    const qreal yScale = qreal(source.h) / destination.h;
    return {{source.x + (visible.x() - destination.x) * xScale,
             source.y + (visible.y() - destination.y) * yScale,
             visible.width() * xScale, visible.height() * yScale},
            {visible.x(), visible.y(), visible.width(), visible.height()}};
}
// VDPAU/MMAL source rectangles use integer pixels. Actual Size has an exact
// integer crop; fractional scaled edges conservatively retain their last pixel.
inline SDL_Rect integerSource(const Clipped& clipped)
{
    const int x = qFloor(clipped.source.x()), y = qFloor(clipped.source.y());
    return {x, y, qCeil(clipped.source.x() + clipped.source.width()) - x,
                  qCeil(clipped.source.y() + clipped.source.height()) - y};
}
}
