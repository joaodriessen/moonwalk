#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QtMath>
#include <algorithm>

namespace CursorSizePolicy {
inline bool validNominalSize(int size) { return size >= 8 && size <= 128; }
inline bool environmentPreference(const QString& desktop, int environmentSize)
{
    // Hyprland configures its cursor through compositor environment. A GNOME
    // portal namespace may still expose another desktop's saved preference.
    return desktop.contains(QStringLiteral("Hyprland"), Qt::CaseInsensitive) && validNominalSize(environmentSize);
}
inline int nominalSize(const QString& desktop, int portalSize, int environmentSize)
{
    if (environmentPreference(desktop, environmentSize)) return environmentSize;
    return validNominalSize(portalSize) ? portalSize : validNominalSize(environmentSize) ? environmentSize : 24;
}

// SDL color-cursor images use SDL window coordinates, which need not be physical
// pixels on Wayland. Convert local points once, independently of the remote video.
inline qreal coordinateScale(int windowWidth, int pixelWidth, qreal displayScale)
{
    if (!qIsFinite(displayScale) || displayScale <= 0) displayScale = 1;
    return windowWidth > 0 && pixelWidth > 0 ? displayScale * windowWidth / pixelWidth : displayScale;
}
// Shadows and nearly transparent edge pixels must not dictate pointer size.
inline QRect visibleBounds(const QImage& image)
{
    int left=image.width(), top=image.height(), right=-1, bottom=-1;
    for (int y=0; y<image.height(); ++y) for (int x=0; x<image.width(); ++x) {
        if (image.pixelColor(x,y).alpha() >= 128) {
            left=qMin(left,x); top=qMin(top,y); right=qMax(right,x); bottom=qMax(bottom,y);
        }
    }
    return right >= left ? QRect(QPoint(left,top),QPoint(right,bottom)) : QRect();
}
inline QSize pixels(const QImage& image, qreal displayScale, qreal localVisibleExtent)
{
    if (image.isNull()) return {};
    if (!qIsFinite(displayScale) || displayScale <= 0) displayScale = 1;
    const auto visible = visibleBounds(image);
    if (visible.isEmpty()) return {}; // A fully transparent host cursor stays hidden.
    // Match visible artwork, not the often much larger transparent host canvas.
    qreal scale = qreal(std::clamp(localVisibleExtent, qreal(1), qreal(256))) * displayScale
        / qMax(visible.width(),visible.height());
    scale = qMin(scale, qreal(256) / qMax(image.width(),image.height()));
    return {qMax(1,qRound(image.width()*scale)), qMax(1,qRound(image.height()*scale))};
}
}
