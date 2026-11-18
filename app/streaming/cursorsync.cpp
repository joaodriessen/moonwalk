#include "cursorsync.h"
#include "cursorsizepolicy.h"

#include <QImage>
#include <QMutexLocker>
#include <QNetworkReply>
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QLibrary>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QFile>
#if __has_include(<X11/Xcursor/Xcursor.h>)
#include <X11/Xcursor/Xcursor.h>
#undef None
#define STARLIGHT_XCURSOR_IMAGE 1
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace {

struct CursorFrame {
    QImage image;
    int hotX;
    int hotY;
    std::uint64_t focusGeneration;
};

constexpr int kMaxCursorPixels = 256;
constexpr int kMaxPngBytes = 512 * 1024;
constexpr int kLongPollTimeoutMs = 8000; // host holds at most 5 seconds
// Mac arrow art occupies only part of its transparent image canvas. At the
// local display scale, 1.125x point size closely matches the native pointer.
constexpr double kCursorSizeCorrection = 1.125;

#ifdef Q_OS_LINUX
QVariant cursorSetting(const QString& key)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.portal.Settings"),
        QStringLiteral("Read"));
    message << QStringLiteral("org.gnome.desktop.interface") << key;
    const auto reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 150);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return {};
    QVariant value = reply.arguments().first();
    for (int i = 0; i < 2 && value.canConvert<QDBusVariant>(); ++i)
        value = value.value<QDBusVariant>().variant();
    return value;
}

qreal localCursorExtent(int size, const QString& theme, double scale)
{
#ifdef STARLIGHT_XCURSOR_IMAGE
    // Use the same Xcursor theme assets as the desktop, including host themes
    // exposed read-only by Flatpak. Loading images does not require an X server.
    QLibrary library(QStringLiteral("Xcursor"), 1);
    const auto load = reinterpret_cast<decltype(&XcursorFilenameLoadImage)>(library.resolve("XcursorFilenameLoadImage"));
    const auto destroy = reinterpret_cast<decltype(&XcursorImageDestroy)>(library.resolve("XcursorImageDestroy"));
    if (load && destroy) {
        auto roots = qEnvironmentVariable("XCURSOR_PATH").split(':',Qt::SkipEmptyParts);
        roots << QDir::homePath()+"/.icons";
        for (const auto& path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) roots << path+"/icons";
        roots << QStringLiteral("/run/host/share/icons");
        QStringList themes{theme};
        for (int i=0; i<themes.size() && i<16; ++i) for (const auto& root : roots) {
            const QString folder = root+"/"+themes[i];
            for (const auto& name : {QStringLiteral("left_ptr"),QStringLiteral("default")}) {
                auto* cursor = load(QFile::encodeName(folder+"/cursors/"+name).constData(), qCeil(size*scale));
                if (!cursor) continue;
                int extent = 0;
                if (cursor->width > 0 && cursor->height > 0 && cursor->width <= 512 && cursor->height <= 512) {
                    const QImage image(reinterpret_cast<const uchar*>(cursor->pixels),cursor->width,cursor->height,QImage::Format_ARGB32);
                    const auto visible = CursorSizePolicy::visibleBounds(image);
                    extent = qMax(visible.width(),visible.height());
                    // Themes may select a nearby nominal size; preserve its
                    // actual artwork rather than assuming a full size×size box.
                }
                destroy(cursor);
                if (extent > 0) return extent/scale;
            }
            QSettings index(folder+"/index.theme",QSettings::IniFormat);
            const auto parents=index.value("Icon Theme/Inherits").toStringList();
            for (const auto& parent : parents) if (!parent.isEmpty() && !themes.contains(parent)) themes << parent;
        }
    }
#else
    Q_UNUSED(theme);
#endif
    return size; // Missing optional theme assets: honor the nominal preference.
}
#endif

bool validPngHeader(const QByteArray &data)
{
    static const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < 24 || data.size() > kMaxPngBytes ||
        std::memcmp(data.constData(), signature, sizeof(signature)) != 0 ||
        std::memcmp(data.constData() + 12, "IHDR", 4) != 0) {
        return false;
    }
    const auto *p = reinterpret_cast<const unsigned char *>(data.constData() + 16);
    unsigned width = (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) | (unsigned(p[2]) << 8) | p[3];
    unsigned height = (unsigned(p[4]) << 24) | (unsigned(p[5]) << 16) | (unsigned(p[6]) << 8) | p[7];
    return width > 0 && width <= kMaxCursorPixels && height > 0 && height <= kMaxCursorPixels;
}

bool parseCursorState(const QByteArray &header, QByteArray &hash, int &hotX, int &hotY,
                      int &pointWidth, int &pointHeight)
{
    const auto parts = header.split(',');
    if (parts.size() != 5 || parts[0].size() != 16) {
        return false;
    }
    for (char c : parts[0]) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    bool xOk = false;
    bool yOk = false;
    bool widthOk = false;
    bool heightOk = false;
    int x = parts[1].toInt(&xOk);
    int y = parts[2].toInt(&yOk);
    int width = parts[3].toInt(&widthOk);
    int height = parts[4].toInt(&heightOk);
    if (!xOk || !yOk || !widthOk || !heightOk || x < 0 || y < 0 ||
        width < 1 || height < 1 || width > kMaxCursorPixels || height > kMaxCursorPixels ||
        x >= kMaxCursorPixels || y >= kMaxCursorPixels) {
        return false;
    }
    hash = parts[0];
    hotX = x;
    hotY = y;
    pointWidth = width;
    pointHeight = height;
    return true;
}

int SDLCALL discardCursorEvents(void *, SDL_Event *event)
{
    if (event->type == SDL_USEREVENT && event->user.code == SDL_CODE_CURSOR_SHAPE) {
        delete static_cast<CursorFrame *>(event->user.data1);
        return 0;
    }
    return 1;
}

}

CursorSync::CursorSync(NvComputer *computer, double displayScale, double coordinateScale) :
    m_Computer(computer),
    m_VisibilityScale(std::clamp(coordinateScale > 0 ? coordinateScale : displayScale, 0.25, 8.0)),
    m_AssetScale(std::clamp(displayScale, 0.5, 8.0))
{
    setObjectName(QStringLiteral("CursorSync"));
#ifdef Q_OS_LINUX
    const int preference = cursorSetting(QStringLiteral("cursor-size")).toInt();
    const int environment = qEnvironmentVariableIntValue("XCURSOR_SIZE");
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    m_EnvironmentCursorPreference = CursorSizePolicy::environmentPreference(desktop, environment);
    m_LocalCursorSize = CursorSizePolicy::nominalSize(desktop, preference, environment);
    m_LocalCursorTheme = m_EnvironmentCursorPreference ? qEnvironmentVariable("XCURSOR_THEME")
        : cursorSetting(QStringLiteral("cursor-theme")).toString();
    if (m_LocalCursorTheme.isEmpty()) m_LocalCursorTheme = qEnvironmentVariable("XCURSOR_THEME", "default");
    m_LocalCursorExtent.store(localCursorExtent(m_LocalCursorSize,m_LocalCursorTheme,m_AssetScale.load()));
    if (qEnvironmentVariableIsSet("STARLIGHT_CURSOR_TRACE"))
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Local cursor theme %s nominal %d scale %.3f visible %.3f points",
            qPrintable(m_LocalCursorTheme),m_LocalCursorSize,m_VisibilityScale.load(),m_LocalCursorExtent.load());
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.portal.Settings"),
        QStringLiteral("SettingChanged"), this, SLOT(cursorSettingChanged(QString,QString,QDBusVariant)));
#endif
}

CursorSync::~CursorSync()
{
    stopAndWait();
    resetCursor();
}

void CursorSync::park(unsigned long milliseconds)
{
    QMutexLocker locker(&m_WakeMutex);
    if (!m_StopRequested.load(std::memory_order_acquire)) {
        m_Wake.wait(&m_WakeMutex, milliseconds);
    }
}

void CursorSync::setFocused(bool focused)
{
    if (m_Focused.exchange(focused, std::memory_order_acq_rel) != focused) {
        m_FocusGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
    if (!focused) {
        m_Cancel.abortIfArmed();
    }
    QMutexLocker locker(&m_WakeMutex);
    m_Wake.wakeAll();
}

void CursorSync::setDisplayScale(double displayScale, double coordinateScale)
{
    const double scale = std::clamp(coordinateScale > 0 ? coordinateScale : displayScale, 0.25, 8.0);
    const double assetScale = std::clamp(displayScale, 0.5, 8.0);
    const bool assetChanged = std::abs(m_AssetScale.exchange(assetScale) - assetScale) >= 0.01;
    if (std::abs(m_VisibilityScale.exchange(scale, std::memory_order_acq_rel) - scale) < 0.01 && !assetChanged) {
        return;
    }
#ifdef Q_OS_LINUX
    m_LocalCursorExtent.store(localCursorExtent(m_LocalCursorSize,m_LocalCursorTheme,assetScale),std::memory_order_release);
#endif
    // Invalidate an in-flight image and ask the host for the same shape again.
    m_FocusGeneration.fetch_add(1, std::memory_order_acq_rel);
    m_Cancel.abortIfArmed();
    QMutexLocker locker(&m_WakeMutex);
    m_Wake.wakeAll();
}

void CursorSync::stopAndWait()
{
    m_StopRequested.store(true, std::memory_order_release);
    m_Cancel.cancel();
    {
        QMutexLocker locker(&m_WakeMutex);
        m_Wake.wakeAll();
    }
    if (isRunning()) {
        wait();
    }
    // A shape may have been posted just before the SDL loop exited. Clear only
    // our events; never leak their QImage payload into a later stream session.
    SDL_FilterEvents(discardCursorEvents, nullptr);
}

void CursorSync::resetCursor()
{
    if (m_CurrentCursor) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        SDL_FreeCursor(m_CurrentCursor);
        m_CurrentCursor = nullptr;
    }
}

void CursorSync::applyAndDeleteFrame(void *payload)
{
    std::unique_ptr<CursorFrame> frame(static_cast<CursorFrame *>(payload));
    m_EventPending.store(false, std::memory_order_release);
    if (!frame || !m_Focused.load(std::memory_order_acquire) ||
        frame->focusGeneration != m_FocusGeneration.load(std::memory_order_acquire) ||
        frame->image.isNull() ||
        frame->hotX >= frame->image.width() || frame->hotY >= frame->image.height()) {
        return;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uchar *>(frame->image.constBits()), frame->image.width(),
        frame->image.height(), 32, frame->image.bytesPerLine(), SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return;
    }
    SDL_Cursor *next = SDL_CreateColorCursor(surface, frame->hotX, frame->hotY);
    SDL_FreeSurface(surface);
    if (!next) {
        return;
    }
    SDL_SetCursor(next);
    if (m_CurrentCursor) {
        SDL_FreeCursor(m_CurrentCursor);
    }
    m_CurrentCursor = next;
    if (qEnvironmentVariableIsSet("STARLIGHT_CURSOR_TRACE")) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Applied remote cursor %dx%d hotspot %d,%d",
                    frame->image.width(), frame->image.height(), frame->hotX, frame->hotY);
    }
}

void CursorSync::run()
{
    // NvHTTP and its QNetworkAccessManager must be constructed on this thread.
    NvHTTP http(m_Computer);
    QByteArray since("0");
    std::uint64_t lastFocusGeneration = 0;
    while (!m_StopRequested.load(std::memory_order_acquire)) {
        bool focused = m_Focused.load(std::memory_order_acquire);
        if (!focused) {
            park(1000);
            continue;
        }
        const auto focusGeneration = m_FocusGeneration.load(std::memory_order_acquire);
        if (focusGeneration != lastFocusGeneration) {
            since = "0"; // get a fresh shape on every focus regain
            lastFocusGeneration = focusGeneration;
        }
        if (m_EventPending.load(std::memory_order_acquire)) {
            park(16); // bound queued cursor images to one
            continue;
        }
        try {
            const auto raw = http.openConnectionRaw(
                http.m_BaseUrlHttps, "actions/cursor",
                QStringLiteral("since=%1").arg(QString::fromLatin1(since)),
                kLongPollTimeoutMs, QByteArray("X-Skylight-Cursor-State"),
                NvHTTP::NVLL_ERROR, &m_Cancel, kMaxPngBytes);

            if (raw.httpStatusCode == 204) {
                continue;
            }
            if (raw.httpStatusCode == 404 || raw.httpStatusCode == 403) {
                // Old/other host: retain the ordinary local cursor for this session.
                return;
            }
            if (raw.httpStatusCode != 200) {
                park(raw.httpStatusCode == 503 ? 250 : 1000);
                continue;
            }

            QByteArray nextState;
            int hotX = 0, hotY = 0, pointWidth = 0, pointHeight = 0;
            if (!parseCursorState(raw.headerValue, nextState, hotX, hotY,
                                  pointWidth, pointHeight) ||
                !validPngHeader(raw.body)) {
                park(1000);
                continue;
            }
            QImage image = QImage::fromData(raw.body, "PNG");
            if (image.isNull() || image.width() > kMaxCursorPixels ||
                image.height() > kMaxCursorPixels ||
                hotX >= image.width() || hotY >= image.height()) {
                park(1000);
                continue;
            }
            const double displayScale = m_VisibilityScale.load(std::memory_order_acquire);
            const double localExtent = m_LocalCursorExtent.load(std::memory_order_acquire);
            const QSize target = localExtent > 0
                ? CursorSizePolicy::pixels(image, displayScale, localExtent)
                : QSize(std::min(kMaxCursorPixels, int(std::lround(pointWidth * displayScale * kCursorSizeCorrection))),
                        std::min(kMaxCursorPixels, int(std::lround(pointHeight * displayScale * kCursorSizeCorrection))));
            // Retain a transparent cursor for the host's hidden-pointer state.
            const QSize safeTarget = target.isEmpty() ? QSize(1,1) : target;
            const int targetWidth = safeTarget.width();
            const int targetHeight = safeTarget.height();
            const int targetHotX = std::clamp((int) std::lround((double) hotX * targetWidth / image.width()),
                                              0, targetWidth - 1);
            const int targetHotY = std::clamp((int) std::lround((double) hotY * targetHeight / image.height()),
                                              0, targetHeight - 1);
            auto *frame = new CursorFrame {image.scaled(targetWidth, targetHeight, Qt::IgnoreAspectRatio,
                                                        Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888),
                                           targetHotX, targetHotY,
                                           focusGeneration};
            SDL_Event event = {};
            event.type = SDL_USEREVENT;
            event.user.code = SDL_CODE_CURSOR_SHAPE;
            event.user.data1 = frame;
            m_EventPending.store(true, std::memory_order_release);
            if (SDL_PushEvent(&event) == 1) {
                since = nextState;
            } else {
                m_EventPending.store(false, std::memory_order_release);
                delete frame;
            }
        }
        catch (const GfeHttpResponseException &) {
            if (!m_StopRequested.load(std::memory_order_acquire)) {
                park(1000);
            }
        }
        catch (const QtNetworkReplyException &) {
            if (!m_StopRequested.load(std::memory_order_acquire)) {
                park(1000);
            }
        }
    }
}

#ifdef Q_OS_LINUX
void CursorSync::cursorSettingChanged(const QString& nameSpace, const QString& key, const QDBusVariant& wrapped)
{
    if (nameSpace != QLatin1String("org.gnome.desktop.interface")) return;
    QVariant value = wrapped.variant();
    if (value.canConvert<QDBusVariant>()) value=value.value<QDBusVariant>().variant();
    if (m_EnvironmentCursorPreference) return;
    if (key == QLatin1String("cursor-size")) {
        const int size=value.toInt();
        if (size < 8 || size > 128) return;
        m_LocalCursorSize=size;
    } else if (key == QLatin1String("cursor-theme")) {
        if (value.toString().isEmpty()) return;
        m_LocalCursorTheme=value.toString();
    } else return;
    m_LocalCursorExtent.store(localCursorExtent(m_LocalCursorSize,m_LocalCursorTheme,m_AssetScale.load()),std::memory_order_release);
    m_FocusGeneration.fetch_add(1,std::memory_order_acq_rel);
    m_Cancel.abortIfArmed();
    QMutexLocker locker(&m_WakeMutex);
    m_Wake.wakeAll();
}
#endif
