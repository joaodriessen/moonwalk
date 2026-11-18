#pragma once

#include <QList>
#include <QRect>
#include <QString>
#include <QtMath>

// One record per SDL display index, including an empty mode when its query
// fails. Keeping the metadata together prevents stale rates or index shifts.
struct DisplayMetadata
{
    QString name;
    QRect bounds;
    QRect nativeResolution;
    QRect safeAreaResolution;
    int refreshRate = 0;
};

class DisplaySnapshot
{
public:
    QList<DisplayMetadata> displays;

    template<class Query>
    static DisplaySnapshot capture(int count, Query query)
    {
        DisplaySnapshot current;
        for (int index = 0; index < count; ++index) current.displays.append(query(index));
        return current;
    }

    int indexForScreen(const QString& name, const QRect& bounds, qreal pixelRatio = 1) const
    {
        int named = -1, namedCount = 0;
        int positioned = -1, positionedCount = 0;
        for (int index = 0; index < displays.size(); ++index) {
            const auto& display = displays[index];
            if (display.name == name) {
                if (display.bounds == bounds) return index;
                named = index;
                ++namedCount;
            }
            if (display.bounds == bounds) {
                positioned = index;
                ++positionedCount;
            }
        }
        if (namedCount == 1) return named;
        if (positionedCount == 1) return positioned;
        // Qt reports logical geometry; SDL2-compat can report physical pixels
        // and a different human-readable name for the very same Wayland output.
        // Match a complete scaled rectangle, and never guess between mirrors.
        if (!qIsFinite(pixelRatio) || pixelRatio <= 1) return -1;
        const QSize pixels(qRound(bounds.width() * pixelRatio), qRound(bounds.height() * pixelRatio));
        const QPoint scaledOrigin(qRound(bounds.x() * pixelRatio), qRound(bounds.y() * pixelRatio));
        int scaled = -1, scaledCount = 0;
        for (int index = 0; index < displays.size(); ++index) {
            const auto& candidate = displays[index].bounds;
            if (candidate.size() == pixels &&
                    (candidate.topLeft() == bounds.topLeft() || candidate.topLeft() == scaledOrigin)) {
                scaled = index;
                ++scaledCount;
            }
        }
        return scaledCount == 1 ? scaled : -1;
    }
};
