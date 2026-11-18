#include "lucideiconprovider.h"

#include <QColor>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace {

QByteArray glyph(const QString& name)
{
    static QMutex mutex;
    static QHash<QString, QByteArray> cache;
    QMutexLocker lock(&mutex);
    auto found = cache.constFind(name);
    if (found != cache.constEnd()) {
        return *found;
    }
    QFile file(QStringLiteral(":/res/lucide/%1.svg").arg(name));
    QByteArray data = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    cache.insert(name, data);
    return data;
}

}

LucideIconProvider::LucideIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage LucideIconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    static const QRegularExpression valid(QStringLiteral("^([a-z0-9-]+)/([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
    const QRegularExpressionMatch match = valid.match(id);
    QByteArray svg = match.hasMatch() ? glyph(match.captured(1)) : QByteArray();

    // Lucide draws on a 24 grid; a request with one side keeps the square.
    QSize target(24, 24);
    if (requestedSize.width() > 0 || requestedSize.height() > 0) {
        const int side = qMax(requestedSize.width(), requestedSize.height());
        target = QSize(requestedSize.width() > 0 ? requestedSize.width() : side,
                       requestedSize.height() > 0 ? requestedSize.height() : side);
    }
    if (size) {
        *size = target;
    }

    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (svg.isEmpty()) {
        return image;
    }

    const QColor color(QLatin1Char('#') + match.captured(2));
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
    QSvgRenderer renderer(svg);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(color.alphaF());
    renderer.render(&painter);
    return image;
}

void LucideIconProvider::install(QQmlEngine* engine)
{
    engine->addImageProvider(QStringLiteral("lucide"), new LucideIconProvider);
}
