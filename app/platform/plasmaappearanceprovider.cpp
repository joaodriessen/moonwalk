#include "plasmaappearanceprovider.h"

#include <QApplication>
#include <QEvent>
#include <QStyle>
#include <QTimer>

#if defined(STARLIGHT_PLASMA_DESKTOP_ADAPTER)
#include <QDBusConnection>
#include <QDebug>
#endif

namespace {

struct PaletteRole {
    const char* name;
    QPalette::ColorRole role;
};

constexpr PaletteRole kPaletteRoles[] = {
    { "window", QPalette::Window },
    { "windowText", QPalette::WindowText },
    { "base", QPalette::Base },
    { "alternateBase", QPalette::AlternateBase },
    { "text", QPalette::Text },
    { "placeholderText", QPalette::PlaceholderText },
    { "button", QPalette::Button },
    { "buttonText", QPalette::ButtonText },
    { "highlight", QPalette::Highlight },
    { "highlightedText", QPalette::HighlightedText },
    { "toolTipBase", QPalette::ToolTipBase },
    { "toolTipText", QPalette::ToolTipText },
    { "light", QPalette::Light },
    { "midlight", QPalette::Midlight },
    { "mid", QPalette::Mid },
    { "dark", QPalette::Dark },
    { "shadow", QPalette::Shadow },
};

} // namespace

PlasmaAppearanceProvider::PlasmaAppearanceProvider(
        bool runtimeActive,
        QObject* parent)
    : QObject(parent)
{
    if (runtimeActive && qApp) {
        qApp->installEventFilter(this);
#if defined(STARLIGHT_PLASMA_DESKTOP_ADAPTER)
        const bool connected = QDBusConnection::sessionBus().connect(
                QString(),
                QStringLiteral("/KGlobalSettings"),
                QStringLiteral("org.kde.KGlobalSettings"),
                QStringLiteral("notifyChange"),
                this,
                SLOT(handleKdeGlobalSettingsChange(int,int)));
        if (!connected) {
            qWarning() << "Plasma appearance: KDE global-settings signal unavailable;"
                          " Qt palette events remain active";
        }
#endif
        requestRefresh();
    }
}

bool PlasmaAppearanceProvider::runtimeActiveForAdapter(
        bool plasmaAdapterActive,
        bool adaptiveShellRequested)
{
    return plasmaAdapterActive && adaptiveShellRequested;
}

bool PlasmaAppearanceProvider::ready() const
{
    return m_Ready;
}

quint64 PlasmaAppearanceProvider::revision() const
{
    return m_Revision;
}

QVariantMap PlasmaAppearanceProvider::active() const
{
    return m_Active;
}

QVariantMap PlasmaAppearanceProvider::inactive() const
{
    return m_Inactive;
}

QVariantMap PlasmaAppearanceProvider::disabled() const
{
    return m_Disabled;
}

QString PlasmaAppearanceProvider::widgetStyle() const
{
    return m_WidgetStyle;
}

void PlasmaAppearanceProvider::applySnapshotForTest(
        const QPalette& palette,
        const QString& widgetStyle)
{
    publishSnapshot(palette, widgetStyle);
}

void PlasmaAppearanceProvider::requestRefresh()
{
    if (m_RefreshPending) {
        return;
    }
    m_RefreshPending = true;
    QTimer::singleShot(0, this, &PlasmaAppearanceProvider::refreshNow);
}

void PlasmaAppearanceProvider::handleKdeGlobalSettingsChange(
        int changeType,
        int argument)
{
    Q_UNUSED(changeType)
    Q_UNUSED(argument)
    if (qApp && QApplication::style()) {
        QApplication::setPalette(QApplication::style()->standardPalette());
    }
    requestRefresh();
}

bool PlasmaAppearanceProvider::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp
            && (event->type() == QEvent::ApplicationPaletteChange
                || event->type() == QEvent::ApplicationFontChange
                || event->type() == QEvent::StyleChange)) {
        requestRefresh();
    }
    return QObject::eventFilter(watched, event);
}

QVariantMap PlasmaAppearanceProvider::colorGroup(
        const QPalette& palette,
        QPalette::ColorGroup group)
{
    QVariantMap result;
    for (const PaletteRole& entry : kPaletteRoles) {
        result.insert(QString::fromLatin1(entry.name),
                      palette.color(group, entry.role));
    }
    return result;
}

void PlasmaAppearanceProvider::refreshNow()
{
    m_RefreshPending = false;
    if (!qApp) {
        return;
    }
    const QString styleName = QApplication::style()
            ? QApplication::style()->objectName()
            : QString();
    publishSnapshot(QApplication::palette(), styleName);
}

void PlasmaAppearanceProvider::publishSnapshot(
        const QPalette& palette,
        const QString& widgetStyle)
{
    const QVariantMap active = colorGroup(palette, QPalette::Active);
    const QVariantMap inactive = colorGroup(palette, QPalette::Inactive);
    const QVariantMap disabled = colorGroup(palette, QPalette::Disabled);
    if (m_Ready && active == m_Active && inactive == m_Inactive
            && disabled == m_Disabled && widgetStyle == m_WidgetStyle) {
        return;
    }

    m_Active = active;
    m_Inactive = inactive;
    m_Disabled = disabled;
    m_WidgetStyle = widgetStyle;
    m_Ready = true;
    ++m_Revision;
    emit snapshotChanged();
}
