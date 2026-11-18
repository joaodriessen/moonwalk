#pragma once

#include <QQuickImageProvider>

class QQmlEngine;

// Lucide's line icons, from res/lucide, drawn in any colour on the CPU.
// The SVGs stroke with currentColor, which is replaced before rendering, so
// no shader recolouring is involved: that rendered glyphs invisible on
// non-GPU render paths. QML asks for "image://lucide/NAME/AARRGGBB"; see
// gui/starlight/Lucide.js, which builds that from a colour.
class LucideIconProvider : public QQuickImageProvider
{
public:
    LucideIconProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    static void install(QQmlEngine* engine);
};
