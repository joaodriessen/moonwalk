#pragma once

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QGuiApplication>
#include <QStyleHints>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QFontInfo>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QStringList>
#include <QSvgRenderer>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "SDL_compat.h"

// Geometry is in SDL window logical coordinates. The image covers the full
// window width and only the top of it; transparent margins let all renderers
// place it at (0, 0) without applying the remote picture transform.
namespace StreamToolbar {
enum Action { None, Controls, Mute, Fullscreen, Capture, ViewScale, Disconnect };
struct Button { Action action; QRect rect; QString label; };

// What the bar shows besides its buttons. A negative value is not known yet.
struct Status {
    QString host;
    int rttMs = -1;
    int fps = -1;
    int kbps = 0;
    bool poor = false;
    bool operator==(const Status& other) const {
        return host == other.host && rttMs == other.rttMs && fps == other.fps && kbps == other.kbps && poor == other.poor;
    }
};
struct State {
    bool muted = false;
    bool fullscreen = false;
    bool captured = false;
    bool actual = false;
    Action hovered = None;
    Action pressed = None;
    Status status;
};

constexpr int BarTop = 8;
constexpr int BarHeight = 56;
constexpr int ButtonSize = 44;
constexpr int ImageHeight = 120;

// topInset is where the picture starts when bars pad it: a pointer held inside the
// picture can't get above it, so the toolbar's own coordinates start there too.
struct Metrics {
    int width;
    qreal pixelRatio;
    qreal inputScale;
    int topInset = 0;
    QPoint point(int x, int y) const { return QPoint(qFloor(x / inputScale), qFloor(y / inputScale) - topInset); }
};
inline Metrics metrics(int windowWidth, int pixelWidth, qreal displayScale, int topInsetPixels = 0)
{
    if (!qIsFinite(displayScale) || displayScale <= 0 || displayScale > 8) displayScale = 1;
    const qreal windowScale = windowWidth > 0 && pixelWidth > 0 ? qreal(pixelWidth) / windowWidth : 1;
    const qreal inputScale = displayScale / windowScale;
    return {qMax(0, qRound(windowWidth / inputScale)), displayScale, inputScale, qMax(0, qFloor(topInsetPixels / displayScale))};
}

// Qt's platform palette can remain light inside a sandbox even when the
// desktop announces dark mode. Prefer that announcement, with palette fallback.
inline QPalette palette(const QPalette& native, int scheme)
{
    const bool dark = scheme == 2 || (scheme != 1 && native.color(QPalette::Window).lightnessF() < .5);
    QPalette result = native;
    if ((native.color(QPalette::Window).lightnessF() < .5) != dark) {
        result.setColor(QPalette::Window, QColor(dark ? "#25272c" : "#f6f6f8"));
        result.setColor(QPalette::WindowText, QColor(dark ? "#f5f5f7" : "#202024"));
        result.setColor(QPalette::Mid, QColor(dark ? "#60646c" : "#b3b5ba"));
    }
    auto background = result.color(QPalette::Window);
    background.setAlphaF(.94);
    result.setColor(QPalette::Window, background);
    return result;
}
inline QPalette palette()
{
    const auto native = QGuiApplication::palette();
    int scheme = 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    scheme = int(QGuiApplication::styleHints()->colorScheme());
#endif
    static qint64 previousKey = -1;
    static int previousScheme = -1;
    static QPalette result;
    if (previousKey != native.cacheKey() || previousScheme != scheme) {
        result = palette(native, scheme);
        previousKey = native.cacheKey();
        previousScheme = scheme;
    }
    // The desktop's accent reaches Qt's palette late, or not at all, in a sandbox.
    const QColor accent = qApp->property("starlightAccent").value<QColor>();
    if (accent.isValid() && result.color(QPalette::Highlight) != accent) result.setColor(QPalette::Highlight, accent);
    return result;
}
inline QFont font(QFont result)
{
    // The HUD image supplies display scaling through DPR. Fix the font in
    // local pixels too, so image DPI cannot disagree with layout metrics.
    result.setPixelSize(qMax(14, QFontInfo(result).pixelSize()));
    return result;
}

inline QString text(const char* source)
{
    return QCoreApplication::translate("StreamToolbar", source);
}

inline bool showsStatus(int logicalWidth) { return logicalWidth >= 760; }
constexpr int StatusWidth = 200;

// Which side of a button a divider belongs on, so related actions read as groups.
inline bool startsGroup(Action action) { return action == Controls || action == Disconnect; }

inline QVector<Button> layout(int logicalWidth)
{
    if (logicalWidth < 128) return {};
    const QVector<Action> actions = logicalWidth >= 480
        ? QVector<Action>{Mute, Capture, ViewScale, Fullscreen, Controls, Disconnect}
        : QVector<Action>{Controls, Capture, Disconnect};
    const int gap = 2;
    const int divider = logicalWidth >= 480 ? 13 : 0;
    const int count = static_cast<int>(actions.size());
    int total = (count - 1) * gap;
    for (const auto action : actions) total += startsGroup(action) && action != actions.first() ? divider - gap : 0;
    const int status = showsStatus(logicalWidth) ? StatusWidth + divider : 0;
    const int width = qMin(ButtonSize, (logicalWidth - 40 - status - total) / count);
    total += width * count;
    int x = (logicalWidth - total - status) / 2 + status;
    QVector<Button> result;
    for (const auto action : actions) {
        if (startsGroup(action) && action != actions.first()) x += divider - gap;
        QString label;
        switch (action) {
        case Controls: label = text("Controls"); break;
        case Mute: label = text("Sound"); break;
        case Fullscreen: label = text("Fullscreen"); break;
        case Capture: label = text("Input"); break;
        case ViewScale: label = text("View scale"); break;
        case Disconnect: label = text("Disconnect"); break;
        case None: break;
        }
        result.append({action, QRect(x, BarTop + (BarHeight - ButtonSize) / 2, width, ButtonSize), label});
        x += width + gap;
    }
    return result;
}

inline QRect bounds(int logicalWidth)
{
    const auto buttons = layout(logicalWidth);
    if (buttons.isEmpty()) return {};
    const int left = buttons.first().rect.left() - (showsStatus(logicalWidth) ? StatusWidth + 13 : 6);
    return QRect(left, BarTop, buttons.last().rect.right() + 7 - left, BarHeight);
}

inline Action hitTest(int logicalWidth, const QPoint& point)
{
    for (const auto& button : layout(logicalWidth))
        if (button.rect.contains(point)) return button.action;
    return None;
}

// Where the pill takes a click: the middle of the top edge, a few points tall, so a
// host's own menus and panels stay in reach.
inline QRect handleRect(int logicalWidth)
{
    if (logicalWidth < 112) return {};
    return QRect((logicalWidth - 120) / 2, 0, 120, 12);
}

// The pointer is on the pill. Anything above the picture counts too: KWin discards
// pointer motion that would cross a confinement edge rather than stopping at it, so
// a quick move up can end several points short of the picture's top.
inline bool onHandle(int logicalWidth, const QPoint& point, int slack = 0)
{
    const QRect handle = handleRect(logicalWidth).adjusted(-slack, -slack, slack, slack);
    return !handle.isEmpty() && point.x() >= handle.left() && point.x() <= handle.right() && point.y() <= handle.bottom();
}
// The pill brightens a little once the pointer is on its way to it.
inline bool nearHandle(int logicalWidth, const QPoint& point)
{
    return logicalWidth >= 112 && qAbs(point.x() - logicalWidth / 2) < 240 && point.y() < 72;
}

inline QColor mixed(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

// A soft shadow without a blur pass: stacked outlines, densest at the edge.
inline void paintShadow(QPainter& painter, const QRectF& rect, qreal radius)
{
    painter.save();
    painter.setPen(Qt::NoPen);
    for (int i = 10; i >= 1; --i) {
        painter.setBrush(QColor(0, 0, 0, 5 + (10 - i) / 2));
        painter.drawRoundedRect(rect.adjusted(-i, -i + 3, i, i + 3), radius + i, radius + i);
    }
    painter.restore();
}

// The same shadow under a capsule, for every moving frame. Its middle is the same
// all along, so it is painted once for a short capsule of this height and stretched.
inline void paintCapsuleShadow(QPainter& painter, const QRectF& capsule, qreal pixelRatio)
{
    const qreal radius = capsule.height() / 2;
    if (capsule.width() < 2 * radius + 8) { paintShadow(painter, capsule, radius); return; }
    constexpr qreal side = 11, above = 8, below = 14;
    static QImage piece;
    static QLinearGradient middle;
    static qreal pieceHeight = -1, pieceRatio = 0;
    if (pieceHeight != capsule.height() || pieceRatio != pixelRatio) {
        piece = QImage(qCeil((2 * radius + 4 + 2 * side) * pixelRatio), qCeil((capsule.height() + above + below) * pixelRatio),
                       QImage::Format_ARGB32_Premultiplied);
        piece.setDevicePixelRatio(pixelRatio);
        piece.fill(Qt::transparent);
        QPainter shadow(&piece);
        shadow.setRenderHint(QPainter::Antialiasing);
        paintShadow(shadow, QRectF(side, above, 2 * radius + 4, capsule.height()), radius);
        shadow.end();
        // The straight middle is one column of it, all the way along.
        const int column = qFloor((side + radius + 2) * pixelRatio);
        middle = QLinearGradient(0, 0, 0, piece.height() / pixelRatio);
        for (int y = 0; y < piece.height(); ++y)
            middle.setColorAt((y + .5) / piece.height(), piece.pixelColor(column, y));
        pieceHeight = capsule.height();
        pieceRatio = pixelRatio;
    }
    const qreal height = capsule.height() + above + below, top = capsule.top() - above;
    const qreal end = side + radius + 2; // one end of the piece, up to its straight middle
    const auto from = [&](qreal x, qreal width) { return QRectF(x * pixelRatio, 0, width * pixelRatio, height * pixelRatio); };
    painter.drawImage(QRectF(capsule.left() - side, top, end, height), piece, from(0, end));
    painter.save();
    painter.translate(0, top);
    painter.fillRect(QRectF(capsule.left() - side + end, 0, capsule.width() - 2 * (end - side), height), middle);
    painter.restore();
    painter.drawImage(QRectF(capsule.right() + side - end, top, end, height), piece, from(2 * radius + 4 + 2 * side - end, end));
}

inline QString label(Action action, bool muted, bool fullscreen, bool captured, bool actual)
{
    switch (action) {
    case Controls: return text("Stream controls");
    case Mute: return muted ? text("Unmute") : text("Mute");
    case Fullscreen: return fullscreen ? text("Windowed") : text("Fullscreen");
    case Capture: return captured ? text("Release input") : text("Capture input");
    case ViewScale: return actual ? text("Fit in Window") : text("Actual Size");
    case Disconnect: return text("Disconnect");
    case None: return {};
    }
    return {};
}

// The keys that do the same thing, for the buttons that have them.
inline QString shortcut(Action action)
{
    switch (action) {
    case Controls: return QStringLiteral("Ctrl+Alt+Shift+O");
    case Fullscreen: return QStringLiteral("Ctrl+Alt+Shift+X");
    case Capture: return QStringLiteral("Ctrl+Alt+Shift+Z");
    case Disconnect: return QStringLiteral("Ctrl+Alt+Shift+Q");
    default: return {};
    }
}

// Lucide glyphs, the app's own icon set, tinted with the current pen colour.
inline const char* glyph(Action action, bool active)
{
    switch (action) {
    case Controls: return "sliders-horizontal";
    case Mute: return active ? "volume-x" : "volume-2";
    case Fullscreen: return active ? "minimize" : "maximize";
    case Capture: return "mouse";
    case ViewScale: return active ? "zoom-out" : "zoom-in";
    case Disconnect: return "unplug";
    case None: break;
    }
    return nullptr;
}
inline void icon(QPainter& p, Action action, const QRect& rect, bool active)
{
    const char* name = glyph(action, active);
    if (!name) return;
    static QHash<QByteArray, QByteArray> sources;
    auto found = sources.find(name);
    if (found == sources.end()) {
        QFile file(QStringLiteral(":/res/lucide/%1.svg").arg(QLatin1String(name)));
        found = sources.insert(name, file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
    }
    if (found->isEmpty()) return;
    const QColor ink = p.pen().color();
    QByteArray svg = *found;
    svg.replace("currentColor", ink.name(QColor::HexRgb).toLatin1());
    QSvgRenderer renderer(svg);
    p.save();
    p.setOpacity(p.opacity() * ink.alphaF());
    renderer.render(&p, QRectF(QPointF(rect.center()) + QPointF(.5, .5) - QPointF(10, 10), QSizeF(20, 20)));
    p.restore();
}

inline QString statusText(const Status& status)
{
    QStringList parts;
    if (status.poor) return text("Slow connection");
    if (status.rttMs >= 0) parts << text("%1 ms").arg(status.rttMs);
    if (status.fps >= 0) parts << text("%1 fps").arg(status.fps);
    if (status.kbps > 0) parts << text("%1 Mbps").arg(qRound(status.kbps / 1000.0));
    return parts.isEmpty() ? text("Connected") : parts.join(QStringLiteral(" · "));
}

// The HUD is one capsule that changes shape: a thin handle at rest, a little larger
// and brighter under the pointer, a panel holding the controls once tapped, or a
// panel holding a line of words. Each state is the same capsule somewhere else, so a
// change of mind half-way only bends its path.
enum Mode { Pill, Message, Bar };

// A damped spring, as SwiftUI's spring(response:dampingFraction:) moves things:
// response is about how long it takes, damping below 1 lets it overshoot a little.
struct Spring {
    qreal value = 0;
    qreal velocity = 0;
    void snap(qreal target) { value = target; velocity = 0; }
    // Returns whether it is still on its way.
    bool step(qreal target, qreal seconds, qreal response, qreal damping, qreal tolerance)
    {
        constexpr qreal pi = 3.14159265358979323846;
        const qreal stiffness = std::pow(2 * pi / response, 2);
        const qreal friction = 4 * pi * damping / response;
        for (qreal left = qBound<qreal>(0, seconds, .05); left > 0; left -= 1.0 / 480) {
            const qreal dt = qMin<qreal>(left, 1.0 / 480);
            velocity += (stiffness * (target - value) - friction * velocity) * dt;
            value += velocity * dt;
        }
        if (qAbs(target - value) < tolerance && qAbs(velocity) < tolerance * 10) snap(target);
        return value != target;
    }
};

// One frame of the capsule. Geometry is in toolbar points.
struct Look {
    QRectF capsule;
    qreal glass = 0;     // 0 the handle, 1 the panel
    qreal controls = 0;  // how much of the controls shows
    qreal words = 0;     // how much of the message shows
    qreal bright = 0;    // the pill under the pointer
    bool operator==(const Look& other) const
    {
        return capsule == other.capsule && glass == other.glass && controls == other.controls
            && words == other.words && bright == other.bright;
    }
};

// emphasis: 0 at rest, up to 1 under the pointer. A press squeezes it, like a button.
inline QRectF pillRect(int logicalWidth, qreal emphasis, bool pressed)
{
    if (logicalWidth < 112) return {};
    const qreal width = (72 + 28 * emphasis) * (pressed ? .9 : 1);
    const qreal height = 5 + 3 * emphasis;
    return QRectF((logicalWidth - width) / 2, 8 - height / 2, width, height);
}
inline QRectF messageRect(int logicalWidth, const QFont& font, const QString& message)
{
    if (message.isEmpty() || logicalWidth < 112) return {};
    const qreal width = qMin<qreal>(logicalWidth - 8, qMax(112, QFontMetrics(font).horizontalAdvance(message) + 36));
    return QRectF((logicalWidth - width) / 2, 6, width, 32);
}

class Motion {
public:
    // Moves the capsule `seconds` further towards `mode`. Returns whether it is still moving.
    bool advance(Mode mode, const QRectF& pill, const QRectF& message, const QRectF& bar,
                 qreal bright, qreal seconds, bool reduced)
    {
        if (pill.isEmpty()) return false;
        if (mode == Bar && bar.isEmpty()) mode = Pill;
        if (mode == Message && message.isEmpty()) mode = Pill;
        if (!m_Placed) { snap(Pill, pill, message, bar); m_Placed = true; }
        if (reduced) {
            const bool moved = m_Mode != mode || look().capsule != (mode == Bar ? bar : mode == Message ? message : pill);
            snap(mode, pill, message, bar);
            m_Bright.snap(mode == Pill ? bright : 0);
            return moved;
        }
        // What's inside leaves first; the capsule follows once it has mostly gone.
        Mode shape = mode;
        if (mode != Bar && m_Controls.value > .05) shape = Bar;
        else if (mode != Message && m_Words.value > .05) shape = Message;
        m_Mode = mode;
        const QRectF target = shape == Bar ? bar : shape == Message ? message : pill;
        // Opening has a little spring in it; closing settles without bouncing.
        qreal response = .44, damping = .8;
        if (shape != Pill) m_Returning = true;
        else if (m_Returning) response = .34, damping = 1;
        else response = .26, damping = .7;
        bool moving = false;
        moving |= m_X.step(target.center().x(), seconds, response, damping, .02);
        moving |= m_Y.step(target.center().y(), seconds, response, damping, .02);
        moving |= m_W.step(target.width(), seconds, response, damping, .02);
        moving |= m_H.step(target.height(), seconds, response, damping, .02);
        if (!moving && shape == Pill) m_Returning = false;
        // It turns to glass as it grows past the pill's size and back as it returns,
        // so its colour always goes with its shape.
        const qreal grown = qBound<qreal>(0, (m_H.value - pill.height()) / 24, 1);
        m_Glass = grown * grown * (3 - 2 * grown);
        // The contents fade in once the capsule is nearly their size.
        const bool arrived = qAbs(m_W.value - target.width()) < target.width() * .12
            && qAbs(m_H.value - target.height()) < target.height() * .2;
        // In gently, out quickly: closing shouldn't wait on them.
        const bool showControls = mode == Bar && shape == Bar && arrived;
        const bool showWords = mode == Message && shape == Message && arrived;
        moving |= m_Controls.step(showControls ? 1 : 0, seconds, showControls ? .22 : .1, 1, .002);
        moving |= m_Words.step(showWords ? 1 : 0, seconds, showWords ? .22 : .1, 1, .002);
        moving |= m_Bright.step(shape == Pill ? bright : 0, seconds, .22, 1, .002);
        return moving;
    }
    void snap(Mode mode, const QRectF& pill, const QRectF& message, const QRectF& bar)
    {
        if (mode == Bar && bar.isEmpty()) mode = Pill;
        if (mode == Message && message.isEmpty()) mode = Pill;
        const QRectF target = mode == Bar ? bar : mode == Message ? message : pill;
        m_X.snap(target.center().x()); m_Y.snap(target.center().y());
        m_W.snap(target.width()); m_H.snap(target.height());
        m_Glass = mode == Pill ? 0 : 1;
        m_Controls.snap(mode == Bar ? 1 : 0);
        m_Words.snap(mode == Message ? 1 : 0);
        m_Mode = mode;
        m_Returning = false;
        m_Placed = !pill.isEmpty();
    }
    Look look() const
    {
        Look result;
        const qreal width = qMax<qreal>(0, m_W.value), height = qMax<qreal>(0, m_H.value);
        result.capsule = QRectF(m_X.value - width / 2, m_Y.value - height / 2, width, height);
        result.glass = m_Glass;
        result.controls = qBound<qreal>(0, m_Controls.value, 1);
        result.words = qBound<qreal>(0, m_Words.value, 1);
        result.bright = qBound<qreal>(0, m_Bright.value, 1);
        return result;
    }

private:
    Spring m_X, m_Y, m_W, m_H, m_Controls, m_Words, m_Bright;
    qreal m_Glass = 0;
    Mode m_Mode = Pill;
    bool m_Returning = false; // on its way back to being the pill: no bounce
    bool m_Placed = false;
};

inline QColor blend(const QColor& from, const QColor& to, qreal amount)
{
    if (amount <= 0) return from;
    if (amount >= 1) return to;
    const auto mix = [amount](qreal a, qreal b) { return a + (b - a) * amount; };
    return QColor::fromRgbF(mix(from.redF(), to.redF()), mix(from.greenF(), to.greenF()),
                            mix(from.blueF(), to.blueF()), mix(from.alphaF(), to.alphaF()));
}

inline bool paintable(int logicalWidth, qreal pixelRatio)
{
    return logicalWidth >= 112 && std::isfinite(pixelRatio) && pixelRatio > 0 && pixelRatio <= 8
        && logicalWidth * pixelRatio <= 16384;
}

// What sits on the panel: status, buttons and the hovered button's tip. Painted
// again only when one of those changes; every moving frame reuses it.
inline QImage controls(int logicalWidth, qreal pixelRatio, const QFont& font,
                       const QPalette& palette, const State& state)
{
    if (layout(logicalWidth).isEmpty() || !paintable(logicalWidth, pixelRatio)) return {};
    QImage result(qCeil(logicalWidth * pixelRatio), qCeil(ImageHeight * pixelRatio), QImage::Format_ARGB32_Premultiplied);
    if (result.isNull()) return {};
    result.setDevicePixelRatio(pixelRatio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    const QColor ink = palette.color(QPalette::WindowText);
    const QColor accent = palette.color(QPalette::Highlight);
    const QColor danger("#e01b24");
    const QRect bar = bounds(logicalWidth);

    const auto buttons = layout(logicalWidth);
    if (showsStatus(logicalWidth)) {
        const QRect block(bar.left() + 20, bar.top(), StatusWidth - 24, bar.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(state.status.poor ? QColor("#e5a50a") : QColor("#2ec27e"));
        painter.drawEllipse(QPointF(block.left() + 4, block.center().y()), 4, 4);
        const QRect words = block.adjusted(18, 0, 0, 0);
        QFont title = font; title.setBold(true);
        painter.setFont(title);
        painter.setPen(ink);
        const int line = QFontMetrics(title).height();
        painter.drawText(QRect(words.left(), words.center().y() - line, words.width(), line), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(title).elidedText(state.status.host, Qt::ElideRight, words.width()));
        painter.setFont(font);
        painter.setPen(mixed(ink, .72));
        painter.drawText(QRect(words.left(), words.center().y() + 1, words.width(), line), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(font).elidedText(statusText(state.status), Qt::ElideRight, words.width()));
        painter.setPen(mixed(ink, .14));
        const int x = buttons.first().rect.left() - 7;
        painter.drawLine(QPointF(x + .5, bar.top() + 14), QPointF(x + .5, bar.bottom() - 13));
    }
    QRect hoveredRect;
    for (const auto& button : buttons) {
        if (startsGroup(button.action) && button.action != buttons.first().action && logicalWidth >= 480) {
            painter.setPen(mixed(ink, .14));
            const int x = button.rect.left() - 7;
            painter.drawLine(QPointF(x + .5, bar.top() + 14), QPointF(x + .5, bar.bottom() - 13));
        }
        const bool active = button.action == Mute ? state.muted : button.action == Fullscreen ? state.fullscreen
            : button.action == Capture ? state.captured : button.action == ViewScale ? state.actual : false;
        // The fullscreen icon already says which way it goes; only real toggles light up.
        const bool lit = active && button.action != Fullscreen && button.action != ViewScale;
        const bool hovered = button.action == state.hovered;
        const bool pressed = button.action == state.pressed;
        const bool destructive = button.action == Disconnect;
        const QRectF fillRect = QRectF(button.rect).adjusted(1, 1, -1, -1);
        painter.setPen(Qt::NoPen);
        if (destructive && hovered) painter.setBrush(pressed ? danger.darker(115) : danger);
        else if (lit) painter.setBrush(mixed(accent, pressed ? .5 : hovered ? .42 : .3));
        else if (hovered) painter.setBrush(mixed(ink, pressed ? .2 : .1));
        else painter.setBrush(Qt::NoBrush);
        // Concentric with the capsule around it: round where the button is square.
        const qreal round = qMin(fillRect.width(), fillRect.height()) / 2;
        painter.drawRoundedRect(fillRect, round, round);
        if (hovered) hoveredRect = button.rect;
        painter.setPen(destructive ? (hovered ? QColor(Qt::white) : danger.lighter(ink.lightnessF() > .5 ? 135 : 100)) : ink);
        icon(painter, button.action, button.rect, active);
    }
    const QString tooltip = label(state.hovered, state.muted, state.fullscreen, state.captured, state.actual);
    if (!tooltip.isEmpty() && !hoveredRect.isEmpty()) {
        const QString keys = shortcut(state.hovered);
        const QFontMetrics metrics(font);
        const int keysWidth = keys.isEmpty() ? 0 : metrics.horizontalAdvance(keys) + 14;
        const int width = qMin(logicalWidth - 16, metrics.horizontalAdvance(tooltip) + keysWidth + 24);
        const QRect rect(std::clamp(hoveredRect.center().x() - width/2, 8, logicalWidth - width - 8), bar.bottom() + 8, width, 32);
        paintShadow(painter, rect, 16);
        painter.setPen(mixed(ink, .14)); painter.setBrush(palette.color(QPalette::Window));
        painter.drawRoundedRect(QRectF(rect).adjusted(.5, .5, -.5, -.5), 16, 16);
        painter.setPen(ink);
        painter.drawText(rect.adjusted(12, 0, -12 - keysWidth, 0), Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(tooltip, Qt::ElideRight, width - 24 - keysWidth));
        if (!keys.isEmpty()) {
            painter.setPen(mixed(ink, .62));
            painter.drawText(rect.adjusted(12, 0, -12, 0), Qt::AlignRight | Qt::AlignVCenter, keys);
        }
    }
    return result;
}

// Where the capsule comes to rest once open.
inline Look settled(int logicalWidth)
{
    Look result;
    result.capsule = bounds(logicalWidth);
    result.glass = result.controls = 1;
    return result;
}

// One frame of the HUD, `topInset` points down to where the picture starts.
// `layer` is what controls() painted; a pill-only frame is only as tall as the pill.
inline QImage hud(int logicalWidth, qreal pixelRatio, const QFont& font, const QPalette& palette,
                  const Look& look, const QImage& layer = {}, const QString& message = {}, int topInset = 0)
{
    if (!paintable(logicalWidth, pixelRatio) || look.capsule.isEmpty()) return {};
    const bool pillOnly = look.glass <= 0 && look.controls <= 0 && look.words <= 0;
    const int height = (pillOnly ? 16 : ImageHeight) + qMax(0, topInset);
    QImage result(qCeil(logicalWidth * pixelRatio), qCeil(height * pixelRatio), QImage::Format_ARGB32);
    if (result.isNull()) return {};
    result.setDevicePixelRatio(pixelRatio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(0, qMax(0, topInset));
    const QColor ink = palette.color(QPalette::WindowText);
    const QColor panel = palette.color(QPalette::Window);
    const bool dark = panel.lightnessF() < .5;
    const QRectF capsule = look.capsule;
    const qreal radius = capsule.height() / 2;
    painter.setPen(Qt::NoPen);
    // Keep the handle visible against a remote background of either tone.
    if (look.glass < 1) {
        const int tone = dark ? 255 : 0;
        painter.setBrush(QColor(tone, tone, tone, qRound((20 + 6 * look.bright) * (1 - look.glass))));
        for (int i = 3; i >= 1; --i)
            painter.drawRoundedRect(capsule.adjusted(-i, -i, i, i), radius + i, radius + i);
    }
    if (look.glass > 0) {
        painter.setOpacity(look.glass);
        paintCapsuleShadow(painter, capsule, pixelRatio);
        painter.setOpacity(1);
        painter.setPen(mixed(ink, .14 * look.glass));
    }
    QColor handle = dark ? panel : QColor(Qt::white);
    handle.setAlphaF(.75 + .22 * look.bright);
    painter.setBrush(blend(handle, panel, look.glass));
    painter.drawRoundedRect(capsule.adjusted(.5, .5, -.5, -.5), radius - .5, radius - .5);
    if (look.controls > 0 && !layer.isNull()) {
        // The controls rise into place as they appear, a whole device pixel at a
        // time so every frame is a plain copy.
        painter.setOpacity(look.controls);
        // Nothing shows outside the capsule while it's still on its way.
        if (look.capsule != settled(logicalWidth).capsule)
            painter.setClipRect(capsule.adjusted(radius / 2, 0, -radius / 2, ImageHeight));
        painter.translate(0, qRound(6 * (1 - look.controls) * pixelRatio) / pixelRatio);
        // Only the part around the bar has anything in it; the hovered button's tip
        // can hang a little past either end.
        const QRectF used = QRectF(bounds(logicalWidth)).adjusted(-160, -BarTop, 160, ImageHeight)
            .intersected(QRectF(0, 0, logicalWidth, ImageHeight));
        painter.drawImage(used, layer, QRectF(used.topLeft() * pixelRatio, used.size() * pixelRatio));
        painter.resetTransform();
        painter.setClipping(false);
        painter.translate(0, qMax(0, topInset));
        painter.setOpacity(1);
    }
    if (look.words > 0 && !message.isEmpty()) {
        painter.setOpacity(look.words);
        painter.setFont(font);
        painter.setPen(ink);
        painter.drawText(capsule.adjusted(16, 0, -16, 0), Qt::AlignCenter,
                         QFontMetrics(font).elidedText(message, Qt::ElideRight, qMax(0, int(capsule.width()) - 32)));
    }
    return result;
}

// The open bar at rest.
inline QImage image(int logicalWidth, qreal pixelRatio, const QFont& font,
                    const QPalette& palette, const State& state)
{
    if (layout(logicalWidth).isEmpty()) return {};
    return hud(logicalWidth, pixelRatio, font, palette, settled(logicalWidth),
               controls(logicalWidth, pixelRatio, font, palette, state));
}

// Moves a finished picture down to where the stream's picture starts.
inline QImage inset(const QImage& pixels, int points)
{
    if (pixels.isNull() || points <= 0) return pixels;
    const qreal ratio = pixels.devicePixelRatio();
    QImage result(pixels.width(), pixels.height() + qCeil(points * ratio), QImage::Format_ARGB32);
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawImage(QPointF(0, points), pixels);
    return result;
}

// The caller owns the returned surface and may transfer it to OverlayManager.
// ARGB8888 matches the existing SDL, EGL, VDPAU and Metal overlay contract.
inline SDL_Surface* copySurface(const QImage& pixels)
{
    if (pixels.isNull()) return nullptr;
    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, pixels.width(), pixels.height(), 32, SDL_PIXELFORMAT_ARGB8888);
    if (result) {
        for (int y = 0; y < pixels.height(); ++y)
            std::memcpy(static_cast<char*>(result->pixels) + y * result->pitch,
                        pixels.constScanLine(y), pixels.width() * 4);
        SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    }
    return result;
}
inline SDL_Surface* surface(int logicalWidth, qreal pixelRatio, const QFont& font,
                            const QPalette& palette, const State& state)
{
    return copySurface(image(logicalWidth, pixelRatio, font, palette, state));
}

}
