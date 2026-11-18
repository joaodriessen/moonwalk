#pragma once
#include <QRect>
#include <QSize>
#include <QtMath>
#include <cmath>
#include "SDL_compat.h"
#include "input/wheelaccumulator.h"

namespace ViewScalePolicy {
// Local view movement uses physical output pixels. Keep fractional wheel motion
// until a full pixel is available, independently on each axis.
class WheelPan {
public:
    QPoint take(const SDL_MouseWheelEvent& event) {
        float horizontal = event.x, vertical = event.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (event.preciseX != 0.0f) horizontal = event.preciseX;
        if (event.preciseY != 0.0f) vertical = event.preciseY;
#endif
        const int direction = event.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        return {m_Horizontal.take(horizontal * direction, 80.0),
                m_Vertical.take(vertical * direction, 80.0)};
    }
    void reset() { m_Horizontal = {}; m_Vertical = {}; }
private:
    WheelAccumulator m_Horizontal, m_Vertical;
};

inline QPoint pan(QSize video, QSize viewport, QPoint current, QPoint delta) {
    const auto bounded=[](int extent,int available,qint64 value) {
        const int excess=qMax(0,extent-available);
        return int(qBound(qint64(-(excess+1)/2),value,qint64(excess/2)));
    };
    return {bounded(video.width(),viewport.width(),qint64(current.x())+delta.x()),
            bounded(video.height(),viewport.height(),qint64(current.y())+delta.y())};
}

inline QRect destination(QSize video, QRect viewport, bool actual, qreal pixelRatio = 1.0,
                         QPoint pan = {}) {
    if (video.isEmpty() || viewport.isEmpty()) return viewport;
    QSize size;
    if (actual) {
        if (!std::isfinite(pixelRatio) || pixelRatio <= 0) pixelRatio = 1;
        size = QSize(qMax(1, qRound(video.width()/pixelRatio)),
                     qMax(1, qRound(video.height()/pixelRatio)));
    } else {
        const auto scale = qMin(qreal(viewport.width())/video.width(), qreal(viewport.height())/video.height());
        size = QSize(qCeil(video.width()*scale), qCeil(video.height()*scale));
    }
    const int x = (viewport.width()-size.width())/2;
    const int y = (viewport.height()-size.height())/2;
    const auto shift = [](int value, int extent, int available) {
        return extent > available ? qBound(-(extent-available+1)/2,value,(extent-available)/2) : 0;
    };
    return QRect(viewport.x()+x+shift(pan.x(),size.width(),viewport.width()),
                 viewport.y()+y+shift(pan.y(),size.height(),viewport.height()), size.width(),size.height());
}
}
