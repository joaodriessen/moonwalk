#pragma once

#include "SDL_compat.h"
#include "pngclipboard.h"
#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QImageReader>
#include <QLibrary>
#include <QMimeData>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <cstring>
#include <memory>
#include <new>

// Every entry point belongs to the GUI/SDL thread. Never request clipboard
// payloads from the other toolkit's connection on that same thread.
namespace NativeClipboard {
inline const QMimeData* qtOwnedMimeData();
namespace Detail {
    inline void cancelInputRetry();
    inline bool onMainThread()
    {
        return QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
    }
    inline bool textTooLarge(const QString& text)
    {
        constexpr qint64 limit = 1024 * 1024;
        if (text.size() > limit)
            return true;
        qint64 size = 0;
        for (qsizetype index = 0; index < text.size(); ++index) {
            const auto value = text[index].unicode();
            if (value < 0x80)
                ++size;
            else if (value < 0x800)
                size += 2;
            else if (QChar::isHighSurrogate(value) && index + 1 < text.size() && text[index + 1].isLowSurrogate()) {
                size += 4;
                ++index;
            } else
                size += QChar::isSurrogate(value) ? 1 : 3; // Qt UTF-8 replaces lone surrogates with '?'.
            if (size > limit)
                return true;
        }
        return false;
    }
    inline bool qtOnly()
    {
        if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
            return false;
        const auto platform = QGuiApplication::platformName();
        return platform != QStringLiteral("xcb") && !platform.startsWith(QStringLiteral("wayland"));
    }
    inline bool localWaylandMime(const QMimeData* mime)
    {
        // QWaylandClipboard's empty sentinel is also plain QMimeData. It does
        // not establish local ownership when the inactive Qt connection has no offer.
        return mime && mime->metaObject() == &QMimeData::staticMetaObject && !mime->formats().isEmpty();
    }
    using DataCallback = const void*(SDLCALL*)(void*, const char*, size_t*);
    using CleanupCallback = void(SDLCALL*)(void*);
    struct Functions {
        bool(SDLCALL* set)(DataCallback, CleanupCallback, void*, const char* const*, size_t) = nullptr;
        bool(SDLCALL* clear)() = nullptr;
        bool(SDLCALL* has)(const char*) = nullptr;
        void*(SDLCALL* get)(const char*, size_t*) = nullptr;
        void(SDLCALL* free)(void*) = nullptr;
        Uint32(SDLCALL* wasInit)(Uint32) = nullptr;
        int(SDLCALL* version)() = nullptr;
        const char*(SDLCALL* driver)() = nullptr;
        SDL_Window*(SDLCALL* fromId)(Uint32) = nullptr;
    };
    struct Library {
        QLibrary library;
        Functions functions;
        Library()
        {
#ifdef Q_OS_WIN
            library.setFileName(QStringLiteral("SDL3.dll"));
#elif defined(Q_OS_MACOS)
            library.setFileName(QStringLiteral("libSDL3.dylib"));
#else
            library.setFileName(QStringLiteral("libSDL3.so.0"));
#endif
            library.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint);
            if (!library.load())
                return;
#define SL_RESOLVE(member, symbol) functions.member = reinterpret_cast<decltype(functions.member)>(library.resolve(symbol))
            SL_RESOLVE(set, "SDL_SetClipboardData");
            SL_RESOLVE(clear, "SDL_ClearClipboardData");
            SL_RESOLVE(has, "SDL_HasClipboardData");
            SL_RESOLVE(get, "SDL_GetClipboardData");
            SL_RESOLVE(free, "SDL_free");
            SL_RESOLVE(wasInit, "SDL_WasInit");
            SL_RESOLVE(version, "SDL_GetVersion");
            SL_RESOLVE(driver, "SDL_GetCurrentVideoDriver");
            SL_RESOLVE(fromId, "SDL_GetWindowFromID");
#undef SL_RESOLVE
        }
    };
    inline const Functions* rich()
    {
        if (!onMainThread())
            return nullptr;
        SDL_version version;
        SDL_GetVersion(&version);
        // sdl2-compat reports a deliberately high patch level. Do not load/init a
        // separate SDL3 when the application is using classic SDL2.
        if (version.major != 2 || version.minor < 30 || version.patch < 50)
            return nullptr;
        static Library loaded;
        const auto& api = loaded.functions;
        if (!api.set || !api.clear || !api.has || !api.get || !api.free || !api.wasInit || !api.version || !api.driver || !api.fromId || api.version() < 3002000 || !(api.wasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO))
            return nullptr;
        const char* nativeDriver = api.driver();
        const char* linkedDriver = SDL_GetCurrentVideoDriver();
        if (!nativeDriver || !linkedDriver || std::strcmp(nativeDriver, linkedDriver))
            return nullptr;
        // sdl2-compat passes SDL_Window pointers/IDs through to this same SDL3.
        if (auto* focus = SDL_GetKeyboardFocus()) {
            if (api.fromId(SDL_GetWindowID(focus)) != focus)
                return nullptr;
        }
        return &api;
    }
    struct Payload {
        QByteArray bytes;
        bool png = false;
        QPointer<QMimeData> priorQtMime;
    };
    inline Payload*& ownedPayload();
    struct SignalLease {
        QPointer<QClipboard> clipboard;
        bool wasBlocked = false;
    };
    inline SignalLease& signalLease()
    {
        static SignalLease lease;
        return lease;
    }
    inline void holdClipboardSignals(QClipboard* clipboard)
    {
        auto& lease = signalLease();
        if (!lease.clipboard) {
            lease.clipboard = clipboard;
            lease.wasBlocked = clipboard->blockSignals(true);
        }
    }
    inline void acquireSignalLease()
    {
        if (!onMainThread() || !QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
            return;
        holdClipboardSignals(QGuiApplication::clipboard());
    }
    inline void releaseSignalLease(bool refreshLocalCopy = false)
    {
        auto& lease = signalLease();
        const auto clipboard = lease.clipboard;
        const bool wasBlocked = lease.wasBlocked;
        lease = {};
        if (!clipboard)
            return;
        clipboard->blockSignals(wasBlocked);
        // A Qt copy could have occurred while signals were suppressed. Refresh
        // only a verified local Qt payload, never an offer still served by SDL.
        if (refreshLocalCopy && !wasBlocked) {
            QTimer::singleShot(0, clipboard, [clipboard] {
                if (clipboard && !ownedPayload() && !clipboard->signalsBlocked() && qtOwnedMimeData()) {
                    emit clipboard->dataChanged();
                    emit clipboard->changed(QClipboard::Clipboard);
                }
            });
        }
    }
    inline Payload*& ownedPayload()
    {
        static Payload* current = nullptr;
        return current;
    }
    inline const void* SDLCALL provide(void* userdata, const char* mime, size_t* size)
    {
        *size = 0;
        if (!userdata || !mime)
            return nullptr;
        const auto* payload = static_cast<const Payload*>(userdata);
        const bool matching = payload->png ? !std::strcmp(mime, "image/png") : (!std::strcmp(mime, "text/plain;charset=utf-8") || !std::strcmp(mime, "text/plain") || !std::strcmp(mime, "UTF8_STRING"));
        if (!matching)
            return nullptr;
        *size = size_t(payload->bytes.size());
        return payload->bytes.constData();
    }
    inline void SDLCALL cleanup(void* userdata)
    {
        auto* payload = static_cast<Payload*>(userdata);
        if (ownedPayload() == payload) {
            ownedPayload() = nullptr;
            releaseSignalLease(true);
        }
        delete payload;
    }
    inline bool offer(const Functions& api, const QByteArray& bytes, bool png)
    {
        QPointer<QMimeData> priorQtMime = const_cast<QMimeData*>(qtOwnedMimeData());
        if (!priorQtMime && ownedPayload())
            priorQtMime = ownedPayload()->priorQtMime;
        auto* payload = new (std::nothrow) Payload { bytes, png, priorQtMime };
        if (!payload)
            return false;
        cancelInputRetry();
        static const char* const imageTypes[] = { "image/png" };
        static const char* const textTypes[] = { "text/plain;charset=utf-8", "text/plain", "UTF8_STRING" };
        // Qt Quick's hidden editors read clipboard text in dataChanged handlers.
        // Wayland must retain SDL's focused input serial; suppress those handlers
        // while this tracked native owner would need the same thread to answer.
        acquireSignalLease();
        ownedPayload() = payload;
        if (api.set(provide, cleanup, payload, png ? imageTypes : textTypes, png ? 1 : 3))
            return true;
        // SDL3 can return false after taking userdata; allocation failure can also
        // have already invoked cleanup. Clear first, then release only if untouched.
        api.clear();
        if (ownedPayload() == payload)
            cleanup(payload);
        return false;
    }
    inline std::unique_ptr<QMimeData> pngMime(const QByteArray& png, bool pngOnly = false)
    {
        if (!PngClipboard::valid(png))
            return { };
        QBuffer source;
        source.setData(png);
        source.open(QIODevice::ReadOnly);
        QImageReader reader(&source, "PNG");
        const QImage image = reader.read();
        if (image.isNull() || image.width() > 4096 || image.height() > 4096 || quint64(image.width()) * image.height() > 16 * 1024 * 1024)
            return { };
        auto mime = std::make_unique<QMimeData>();
        mime->setData("image/png", png);
        if (!pngOnly)
            mime->setImageData(image);
        return mime;
    }
}
inline const QMimeData* qtOwnedMimeData()
{
    if (!Detail::onMainThread() || !qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        return nullptr;
    auto* clipboard = QGuiApplication::clipboard();
    const QMimeData* local = nullptr;
    if (Detail::qtOnly() || clipboard->ownsClipboard())
        local = clipboard->mimeData();
    // Qt6.10's wlr-data-control source is not reflected by ownsClipboard().
    // In verified Qt6.4/6.10, remote Wayland offers have QInternalMimeData's
    // metaobject; plain QMimeData is our local setText/image or QtQuick copy.
    if (!local && QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        const auto* mime = clipboard->mimeData(); // pointer lookup, no data request
        if (Detail::localWaylandMime(mime))
            local = mime;
    }
    // SDL can notify our loop before Qt receives the old source's cancellation.
    // Ignore only that previous Qt object; a newer Qt copy is immediately valid.
    const auto* native = Detail::ownedPayload();
    return native && native->priorQtMime == local ? nullptr : local;
}
inline bool supportsPng()
{
    return Detail::onMainThread() && (Detail::qtOnly() || Detail::rich());
}
inline bool needsQtEvents()
{
    return Detail::onMainThread() && (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland")) || Detail::rich());
}
inline bool hasNativeFormat(const char* mime)
{
    const auto* api = Detail::rich();
    return api && api->has(mime);
}
inline QByteArray readNativeData(const char* mime, qint64 maxBytes)
{
    const auto* api = Detail::rich();
    if (!api || maxBytes <= 0 || !api->has(mime))
        return { };
    size_t size = 0;
    void* data = api->get(mime, &size);
    QByteArray result;
    if (data && size && size <= size_t(maxBytes))
        result = QByteArray(static_cast<const char*>(data), qsizetype(size));
    api->free(data);
    return result;
}
inline QByteArray readText(bool* tooLarge = nullptr)
{
    if (tooLarge)
        *tooLarge = false;
    if (!Detail::onMainThread())
        return { };
    if (const auto* mime = qtOwnedMimeData()) {
        const auto text = mime->text();
        if (Detail::textTooLarge(text)) {
            if (tooLarge)
                *tooLarge = true;
            return { };
        }
        return text.toUtf8();
    }
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO))
        return { };
    char* raw = SDL_GetClipboardText();
    if (!raw)
        return { };
    size_t size = 0;
    while (size <= 1024 * 1024 && raw[size])
        ++size;
    if (tooLarge)
        *tooLarge = size > 1024 * 1024;
    auto text = size <= 1024 * 1024 ? QByteArray(raw, qsizetype(size)) : QByteArray();
    SDL_free(raw);
    return text;
}
inline bool publishText(const QByteArray& text, bool forQtControls = false)
{
    if (!Detail::onMainThread() || text.isEmpty() || text.size() > 1024 * 1024 || text.contains('\0'))
        return false;
    // Hidden Qt Quick editors query X11 TARGETS on clipboard changes. Keep Qt
    // as the sole publisher there, so that query never waits on our SDL loop.
    if (Detail::qtOnly() || QGuiApplication::platformName() == QStringLiteral("xcb") || forQtControls) {
        if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
            return false;
        Detail::releaseSignalLease();
        QGuiApplication::clipboard()->setText(QString::fromUtf8(text));
        return true;
    }
    if (const auto* api = Detail::rich())
        return Detail::offer(*api, text, false);
    return (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) && SDL_SetClipboardText(text.constData()) == 0;
}
inline bool publishPng(const QByteArray& png, bool forQtControls = false)
{
    if (!Detail::onMainThread())
        return false;
    // Qt Wayland serializes application/x-qt-image as BMP, losing transparency.
    // Advertising only the validated PNG makes default external imageData use PNG.
    auto mime = Detail::pngMime(png, QGuiApplication::platformName().startsWith(QStringLiteral("wayland")));
    if (!mime)
        return false;
    if (Detail::qtOnly() || QGuiApplication::platformName() == QStringLiteral("xcb") || forQtControls) {
        if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
            return false;
        Detail::releaseSignalLease();
        QGuiApplication::clipboard()->setMimeData(mime.release());
        return true;
    }
    if (const auto* api = Detail::rich())
        return Detail::offer(*api, png, true);
    return false;
}
inline bool prepareForQtControls()
{
    if (!Detail::onMainThread())
        return false;
    if (qtOwnedMimeData()) {
        Detail::releaseSignalLease(true);
        return true;
    }
    if (!Detail::rich())
        return !QGuiApplication::platformName().startsWith(QStringLiteral("wayland")); // legacy Wayland has no safe ownership token
    SDL_PumpEvents(); // expire any SelectionClear before considering our payload
    if (qtOwnedMimeData()) {
        Detail::releaseSignalLease(true);
        return true;
    }
    const auto* owned = Detail::ownedPayload();
    if (!owned)
        return true; // preserve external selections and all unknown formats
    const QByteArray bytes = owned->bytes;
    return owned->png ? publishPng(bytes, true) : publishText(bytes, true);
}
namespace Detail {
    class InputRetry;
    inline QPointer<InputRetry>& inputRetry()
    {
        static QPointer<InputRetry> retry;
        return retry;
    }
    class InputRetry : public QObject {
    public:
        InputRetry(const QByteArray& bytes, bool png, const QMimeData* expected)
            : QObject(QCoreApplication::instance())
            , m_bytes(bytes)
            , m_png(png)
            , m_expected(const_cast<QMimeData*>(expected))
        {
            QCoreApplication::instance()->installEventFilter(this);
            connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this] { verifyOwnership(); });
            connect(m_expected, &QObject::destroyed, this, [this] { stop(); });
        }
        ~InputRetry() override
        {
            if (inputRetry() == this)
                inputRetry() = nullptr;
        }
        void stop()
        {
            if (!m_active)
                return;
            m_active = false;
            m_bytes.clear();
            QCoreApplication::instance()->removeEventFilter(this);
            disconnect(QGuiApplication::clipboard(), nullptr, this, nullptr);
            if (m_expected)
                disconnect(m_expected, nullptr, this, nullptr);
            if (inputRetry() == this)
                inputRetry() = nullptr;
            deleteLater();
        }
        bool verifyOwnership()
        {
            const bool valid = m_active && m_expected && qtOwnedMimeData() == m_expected;
            if (!valid)
                stop();
            return valid;
        }
        void considerInput(QEvent::Type type, bool spontaneous)
        {
            if (!verifyOwnership() || !spontaneous || (type != QEvent::KeyPress && type != QEvent::MouseButtonPress))
                return;
            const auto bytes = m_bytes;
            const bool png = m_png;
            stop();
            // Qt has already recorded this real input's fresh seat serial.
            // A new MIME object avoids Qt's same-pointer setMimeData no-op.
            if (png)
                publishPng(bytes, true);
            else
                publishText(bytes, true);
        }
    protected:
        bool eventFilter(QObject*, QEvent* event) override
        {
            considerInput(event->type(), event->spontaneous());
            return false;
        }
    private:
        QByteArray m_bytes;
        bool m_png;
        QPointer<QMimeData> m_expected;
        bool m_active = true;
    };
    inline void cancelInputRetry()
    {
        if (inputRetry())
            inputRetry()->stop();
    }
}
inline bool finishSession()
{
    if (!Detail::onMainThread())
        return false;
    Detail::cancelInputRetry();
    SDL_PumpEvents();
    if (qtOwnedMimeData()) {
        Detail::releaseSignalLease(true);
        return true;
    }
    const auto* owned = Detail::ownedPayload();
    if (!owned) {
        Detail::releaseSignalLease(true);
        return true;
    }
    const auto bytes = owned->bytes;
    const bool png = owned->png;
    const bool result = png ? publishPng(bytes, true) : publishText(bytes, true);
    Detail::releaseSignalLease(); // includes decode/publication failure
    if (!result)
        return false;
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        const auto* expected = qtOwnedMimeData();
        if (!expected)
            return false;
        Detail::inputRetry() = new Detail::InputRetry(bytes, png, expected);
    }
    return true;
}
}
