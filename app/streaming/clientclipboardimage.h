#pragma once
#include "nativeclipboard.h"
#include "pngclipboard.h"
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QMimeData>
#include <memory>

namespace ClientClipboardImage {
class BoundedBuffer final : public QBuffer {
public:
    explicit BoundedBuffer(QByteArray* bytes)
        : QBuffer(bytes)
    {
    }

protected:
    qint64 writeData(const char* data, qint64 size) override
    {
        if (size < 0 || pos() > PngClipboard::maxBytes - size)
            return -1;
        return QBuffer::writeData(data, size);
    }
};
inline QByteArray encode(const QImage& image)
{
    if (image.isNull() || image.width() > 4096 || image.height() > 4096 || quint64(image.width()) * image.height() > 16 * 1024 * 1024)
        return { };
    QByteArray bytes;
    BoundedBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    return image.save(&buffer, "PNG") && PngClipboard::valid(bytes) ? bytes : QByteArray();
}
struct Value {
    bool image = false;
    QByteArray png;
    QByteArray hash;
};
// GUI/SDL thread only. Keep native PNG bytes where available so a remote image
// round trip doesn't change its digest merely because Qt re-encoded it.
inline Value read()
{
    const auto* mime = NativeClipboard::qtOwnedMimeData();
    if (mime) {
        if (!mime->hasImage() && !mime->hasFormat("image/png"))
            return { };
        if (mime->hasFormat("image/png")) {
            const auto bytes = mime->data("image/png");
            return { true, PngClipboard::valid(bytes) ? bytes : QByteArray(), PngClipboard::hash(bytes) };
        }
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        const auto bytes = encode(image);
        return { true, bytes, bytes.isEmpty() ? PngClipboard::hash(QByteArray("unsupported:") + QByteArray::number(image.cacheKey())) : PngClipboard::hash(bytes) };
    }
    if (NativeClipboard::hasNativeFormat("image/png")) {
        const auto bytes = NativeClipboard::readNativeData("image/png", PngClipboard::maxBytes);
        static quint64 unsupportedIdentity = 0;
        const auto identity = bytes.isEmpty() ? QByteArray("unsupported-native:") + QByteArray::number(++unsupportedIdentity) : bytes;
        return { true, PngClipboard::valid(bytes) ? bytes : QByteArray(), PngClipboard::hash(identity) };
    }
    // Preserve non-PNG image copy identity without requesting a Qt-owned/native
    // selection through the other toolkit. Conversion uses only Qt's pure codec.
    for (const auto* format : { "image/jpeg", "image/tiff", "image/bmp" }) {
        if (!NativeClipboard::hasNativeFormat(format))
            continue;
        const auto data = NativeClipboard::readNativeData(format, PngClipboard::maxBytes);
        QBuffer source;
        source.setData(data);
        source.open(QIODevice::ReadOnly);
        QImageReader reader(&source);
        const auto size = reader.size();
        const auto bytes = size.isValid() && size.width() <= 4096 && size.height() <= 4096 && quint64(size.width()) * size.height() <= 16 * 1024 * 1024 ? encode(reader.read()) : QByteArray();
        static quint64 unsupportedIdentity = 0;
        return { true, bytes, PngClipboard::hash(data.isEmpty() ? QByteArray("unsupported-native-image:") + QByteArray::number(++unsupportedIdentity) : data) };
    }
    return { };
}
inline std::unique_ptr<QMimeData> decoded(const QByteArray& png)
{
    return NativeClipboard::Detail::pngMime(png);
}
inline bool publish(const QByteArray& png, bool forQtControls = false)
{
    return NativeClipboard::publishPng(png, forQtControls);
}
inline bool supportsPng() { return NativeClipboard::supportsPng(); }
}
