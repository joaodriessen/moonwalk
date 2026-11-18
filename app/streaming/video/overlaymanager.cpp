#include "overlaymanager.h"
#include "path.h"
#include "streaming/streamtoolbar.h"
#include <QGuiApplication>

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

    m_Overlays[OverlayType::OverlayDebug].color = {0xD0, 0xD0, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 20;

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = 36;

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    SDL_utf8strlcpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::updateToolbar(SDL_Surface* surface)
{
    const bool wasEnabled = m_Overlays[OverlayToolbar].enabled;
    m_Overlays[OverlayToolbar].enabled = surface != nullptr;
    SDL_Surface* oldSurface = static_cast<SDL_Surface*>(SDL_AtomicSetPtr(
        reinterpret_cast<void**>(&m_Overlays[OverlayToolbar].surface), surface));
    // Ownership transfers even when a renderer has not been attached yet.
    if (oldSurface && oldSurface != surface) SDL_FreeSurface(oldSurface);
    if (m_Renderer && (surface || wasEnabled)) m_Renderer->notifyOverlayUpdated(OverlayToolbar);
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    m_Renderer = renderer;
}

void OverlayManager::setDisplayWidth(int width, qreal pixelRatio)
{
    // Overlay surfaces are drawn at their own size. Keep them inside the current
    // window, including when a fullscreen stream becomes a small desktop window.
    const int wrapWidth = qBound(1, width - 20, 1024);
    if (!std::isfinite(pixelRatio) || pixelRatio<=0 || pixelRatio>8) pixelRatio=1;
    const bool sameRatio=qFuzzyCompare(m_StatusPixelRatio,pixelRatio);
    m_StatusPixelRatio=pixelRatio;
    if (wrapWidth==m_WrapWidth.exchange(wrapWidth) && sameRatio) return;
    // Status rendering belongs to this SDL thread. Decoder-thread statistics
    // pick up the atomic width on their next update; don't race their font here.
    if (m_Overlays[OverlayStatusUpdate].enabled) notifyOverlayUpdated(OverlayStatusUpdate);
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    if (m_Renderer == nullptr) {
        return;
    }

    // The toolbar is already painted with the platform font and palette.
    if (type == OverlayToolbar) {
        m_Renderer->notifyOverlayUpdated(type);
        return;
    }

    if (type==OverlayStatusUpdate) {
        SDL_Surface* surface=nullptr;
        if (m_Overlays[type].enabled && m_Overlays[type].text[0]) {
            // The same desktop-following colours and readable size as the toolbar,
            // floating clear of the window's corner.
            const auto font=StreamToolbar::font(QGuiApplication::font());
            const auto palette=StreamToolbar::palette();
            constexpr int margin=12;
            const int maxWidth=qMax(1,m_WrapWidth.load()-24-2*margin);
            const QString text=QString::fromUtf8(m_Overlays[type].text);
            const auto textRect=QFontMetrics(font).boundingRect(QRect(0,0,maxWidth,4096),Qt::TextWordWrap,text);
            const QSize bubble(textRect.width()+24,textRect.height()+16);
            const QSize logical=bubble+QSize(2*margin,2*margin);
            QImage pixels(QSize(qCeil(logical.width()*m_StatusPixelRatio),qCeil(logical.height()*m_StatusPixelRatio)),QImage::Format_ARGB32);
            if (!pixels.isNull()) {
                pixels.setDevicePixelRatio(m_StatusPixelRatio); pixels.fill(Qt::transparent);
                QPainter painter(&pixels); painter.setFont(font);
                painter.setRenderHint(QPainter::Antialiasing);
                const QRectF rect(margin,margin,bubble.width(),bubble.height());
                StreamToolbar::paintShadow(painter,rect,10);
                painter.setPen(StreamToolbar::mixed(palette.color(QPalette::WindowText),.14)); painter.setBrush(palette.color(QPalette::Window));
                painter.drawRoundedRect(rect.adjusted(.5,.5,-.5,-.5),10,10);
                painter.setPen(palette.color(QPalette::WindowText));
                painter.drawText(QRect(margin+12,margin+8,maxWidth,textRect.height()),Qt::TextWordWrap,text);
                painter.end(); surface=StreamToolbar::copySurface(pixels);
            }
        }
        auto old=static_cast<SDL_Surface*>(SDL_AtomicSetPtr(reinterpret_cast<void**>(&m_Overlays[type].surface),surface));
        m_Renderer->notifyOverlayUpdated(type);
        SDL_FreeSurface(old);
        return;
    }

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
                                               1,
                                               m_Overlays[type].fontSize);
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    // Exchange the old surface with the new one
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr(
        (void**)&m_Overlays[type].surface,
        m_Overlays[type].enabled ?
            // The _Wrapped variant is required for line breaks to work
            RenderTextOutlinedWrapped(m_Overlays[type].font,
                                      m_Overlays[type].text,
                                      m_Overlays[type].color,
                                      {0, 0, 0, 255},
                                      4,
                                      m_WrapWidth.load())
            : nullptr);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

SDL_Surface* OverlayManager::RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);

    // Verify that the string won't require wrapping (which could cause the outline and the text
    // to diverge due to different wrapping positions).
    //
    // FIXME: We do this rather than just disabling wrapping entirely (wrapWidth = 0) because we
    // need further testing to ensure that all renderers can handle non-NPOT overlay textures.
    for (const QString& line : QString(text).split('\n')) {
        int extent, count;
        if (TTF_MeasureUTF8(font, line.toUtf8(), wrapWidth, &extent, &count) == 0 && count < line.size()) {
            // If it requires wrapping, render it without the outline
            TTF_SetFontOutline(font, oldOutline);
            return TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
        }
    }

    // Draw text twice, but outline is a bit bigger
    auto outlineSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, outlineColor, wrapWidth);
    TTF_SetFontOutline(font, 0);
    auto textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    TTF_SetFontOutline(font, oldOutline);

    if (outlineSurface == nullptr || textSurface == nullptr) {
        SDL_FreeSurface(outlineSurface);
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Merge the texts
    SDL_Rect dst = { outlineWidth, outlineWidth, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dst);

    SDL_FreeSurface(textSurface);
    return outlineSurface;
}


