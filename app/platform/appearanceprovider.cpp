#include "appearanceprovider.h"

#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <QtGlobal>

#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

namespace {

constexpr auto kAppearanceNamespace = "org.freedesktop.appearance";
constexpr auto kGnomeInterfaceNamespace = "org.gnome.desktop.interface";
constexpr auto kGnomeA11yNamespace = "org.gnome.desktop.a11y.interface";
constexpr auto kGnomeWmNamespace = "org.gnome.desktop.wm.preferences";

QColor accentFromVariant(const QVariant& value)
{
    QVariantList channels = value.toList();

#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
    if (channels.isEmpty()
            && value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument =
                qvariant_cast<QDBusArgument>(value);
        if (argument.currentType() == QDBusArgument::StructureType) {
            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;
            argument.beginStructure();
            argument >> red >> green >> blue;
            argument.endStructure();
            channels = { red, green, blue };
        }
    }
#endif

    if (channels.size() != 3) {
        return {};
    }

    bool redOk = false;
    bool greenOk = false;
    bool blueOk = false;
    const qreal red = channels.at(0).toDouble(&redOk);
    const qreal green = channels.at(1).toDouble(&greenOk);
    const qreal blue = channels.at(2).toDouble(&blueOk);
    if (!redOk || !greenOk || !blueOk
            || !qIsFinite(red) || !qIsFinite(green) || !qIsFinite(blue)
            || red < 0.0 || red > 1.0
            || green < 0.0 || green > 1.0
            || blue < 0.0 || blue > 1.0) {
        return {};
    }

    return QColor::fromRgbF(red, green, blue);
}

bool standardBoolean(const QVariant& value, bool* valid)
{
    bool conversionOk = false;
    const uint integer = value.toUInt(&conversionOk);
    *valid = conversionOk && integer <= 1;
    return *valid && integer == 1;
}

bool gnomeBoolean(const QVariant& value, bool* valid)
{
    *valid = value.metaType() == QMetaType::fromType<bool>();
    return *valid && value.toBool();
}

} // namespace

AppearanceProvider::AppearanceProvider(
        InitializationMode mode,
        QObject* parent)
    : QObject(parent)
{
    if (mode == Runtime) {
        initializeRuntimeSources();
    }
}

AppearanceProvider::InitializationMode
AppearanceProvider::initializationModeForAdapter(
        bool adwaitaSurfaceActive,
        bool adaptiveShellRequested)
{
    return adwaitaSurfaceActive && adaptiveShellRequested
            ? Runtime
            : Isolated;
}

AppearanceProvider::ColorScheme AppearanceProvider::colorScheme() const
{
    return m_ColorScheme;
}

bool AppearanceProvider::dark() const
{
    return m_ColorScheme == Dark;
}

QColor AppearanceProvider::accentColor() const
{
    return m_AccentColor;
}

bool AppearanceProvider::highContrast() const
{
    return m_HighContrast;
}

qreal AppearanceProvider::fontScale() const
{
    return m_FontScale;
}

bool AppearanceProvider::reducedMotion() const
{
    return m_ReducedMotion;
}

void AppearanceProvider::applyPortalSetting(
        const QString& nameSpace,
        const QString& key,
        const QVariant& value)
{
    if (nameSpace == QLatin1String(kAppearanceNamespace)) {
        if (key == QLatin1String("color-scheme")) {
            m_HaveStandardScheme = true;
            bool valid = false;
            const uint portalScheme = value.toUInt(&valid);
            if (!valid || portalScheme > 2) {
                setColorScheme(NoPreference);
            }
            else if (portalScheme == 1) {
                setColorScheme(Dark);
            }
            else if (portalScheme == 2) {
                setColorScheme(Light);
            }
            else {
                setColorScheme(NoPreference);
            }
        }
        else if (key == QLatin1String("accent-color")) {
            const QColor accent = accentFromVariant(value);
            setAccentColor(accent.isValid()
                           ? accent
                           : QColor(QStringLiteral("#3584e4")));
        }
        else if (key == QLatin1String("contrast")) {
            m_HaveStandardContrast = true;
            bool valid = false;
            const bool contrast = standardBoolean(value, &valid);
            setHighContrast(valid ? contrast : false);
        }
        else if (key == QLatin1String("reduced-motion")) {
            m_HaveStandardReducedMotion = true;
            bool valid = false;
            const bool reduced = standardBoolean(value, &valid);
            setReducedMotion(valid ? reduced : false);
        }
        return;
    }

    if (nameSpace == QLatin1String(kGnomeWmNamespace)
            && key == QLatin1String("button-layout")) {
        const QString layout = value.toString().trimmed();
        setWindowButtonLayout(layout.isEmpty()
                              ? QStringLiteral(":close") : layout);
        return;
    }

    if (nameSpace == QLatin1String(kGnomeInterfaceNamespace)
            && key == QLatin1String("font-name")) {
        setInterfaceFontName(value.toString().trimmed());
        return;
    }

    if (nameSpace == QLatin1String(kGnomeInterfaceNamespace)
            && key == QLatin1String("text-scaling-factor")) {
        bool valid = false;
        const qreal scale = value.toDouble(&valid);
        setFontScale(valid && qIsFinite(scale)
                     && scale >= 0.5 && scale <= 3.0
                     ? scale
                     : 1.0);
        return;
    }

    if (nameSpace != QLatin1String(kGnomeA11yNamespace)) {
        return;
    }

    if (key == QLatin1String("high-contrast")
            && !m_HaveStandardContrast) {
        bool valid = false;
        const bool contrast = gnomeBoolean(value, &valid);
        setHighContrast(valid ? contrast : false);
    }
    else if (key == QLatin1String("reduced-motion")
             && !m_HaveStandardReducedMotion) {
        setReducedMotion(
                value.toString().compare(
                    QStringLiteral("reduce"),
                    Qt::CaseInsensitive) == 0);
    }
}

void AppearanceProvider::initializeRuntimeSources()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        const auto applyQtScheme = [this](Qt::ColorScheme scheme) {
            if (m_HaveStandardScheme) {
                return;
            }
            if (scheme == Qt::ColorScheme::Dark) {
                setColorScheme(Dark);
            }
            else if (scheme == Qt::ColorScheme::Light) {
                setColorScheme(Light);
            }
            else {
                setColorScheme(NoPreference);
            }
        };
        applyQtScheme(QGuiApplication::styleHints()->colorScheme());
        connect(QGuiApplication::styleHints(),
                &QStyleHints::colorSchemeChanged,
                this,
                applyQtScheme);
    }
#endif

#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
    m_RuntimeFontFollowsInterface = true;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    bus.connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.Settings"),
            QStringLiteral("SettingChanged"),
            this,
            SLOT(handleSettingChanged(QString,QString,QDBusVariant)));

    const auto readOne = [this](
            const QString& nameSpace,
            const QString& key) {
        auto* interface = new QDBusInterface(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.portal.Settings"),
                QDBusConnection::sessionBus(),
                this);
        if (!interface->isValid()) {
            interface->deleteLater();
            return;
        }

        QDBusPendingCall call = interface->asyncCall(
                QStringLiteral("ReadOne"),
                nameSpace,
                key);
        auto* watcher = new QDBusPendingCallWatcher(call, interface);
        connect(watcher,
                &QDBusPendingCallWatcher::finished,
                this,
                [this, watcher, interface, nameSpace, key]() {
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            if (!reply.isError()) {
                applyPortalSetting(
                        nameSpace,
                        key,
                        reply.value().variant());
            }
            watcher->deleteLater();
            interface->deleteLater();
        });
    };

    readOne(QString::fromLatin1(kAppearanceNamespace),
            QStringLiteral("color-scheme"));
    readOne(QString::fromLatin1(kAppearanceNamespace),
            QStringLiteral("accent-color"));
    readOne(QString::fromLatin1(kAppearanceNamespace),
            QStringLiteral("contrast"));
    readOne(QString::fromLatin1(kAppearanceNamespace),
            QStringLiteral("reduced-motion"));
    readOne(QString::fromLatin1(kGnomeInterfaceNamespace),
            QStringLiteral("text-scaling-factor"));
    readOne(QString::fromLatin1(kGnomeWmNamespace),
            QStringLiteral("button-layout"));
    readOne(QString::fromLatin1(kGnomeInterfaceNamespace),
            QStringLiteral("font-name"));
    readOne(QString::fromLatin1(kGnomeA11yNamespace),
            QStringLiteral("high-contrast"));
    readOne(QString::fromLatin1(kGnomeA11yNamespace),
            QStringLiteral("reduced-motion"));
#endif
}

#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
void AppearanceProvider::handleSettingChanged(
        const QString& nameSpace,
        const QString& key,
        const QDBusVariant& value)
{
    applyPortalSetting(nameSpace, key, value.variant());
}
#endif

void AppearanceProvider::setColorScheme(ColorScheme value)
{
    if (m_ColorScheme == value) {
        return;
    }
    m_ColorScheme = value;
    emit colorSchemeChanged();
}

void AppearanceProvider::setAccentColor(const QColor& value)
{
    if (m_AccentColor == value) {
        return;
    }
    m_AccentColor = value;
    emit accentColorChanged();
}

void AppearanceProvider::setHighContrast(bool value)
{
    if (m_HighContrast == value) {
        return;
    }
    m_HighContrast = value;
    emit highContrastChanged();
}

void AppearanceProvider::setFontScale(qreal value)
{
    if (qFuzzyCompare(m_FontScale, value)) {
        return;
    }
    m_FontScale = value;
    emit fontScaleChanged();
}

QString AppearanceProvider::windowButtonLayout() const
{
    return m_WindowButtonLayout;
}

QString AppearanceProvider::interfaceFontName() const
{
    return m_InterfaceFontName;
}

void AppearanceProvider::setWindowButtonLayout(const QString& value)
{
    if (m_WindowButtonLayout == value) {
        return;
    }
    m_WindowButtonLayout = value;
    emit windowButtonLayoutChanged();
}

void AppearanceProvider::setInterfaceFontName(const QString& value)
{
    if (m_InterfaceFontName == value) {
        return;
    }
    m_InterfaceFontName = value;
    emit interfaceFontNameChanged();
    applyInterfaceFont();
}

void AppearanceProvider::applyInterfaceFont()
{
    // "Family Name 11" — trailing token is the point size. Applied only for
    // runtime GNOME-adaptive providers so the legacy shell and tests keep
    // their fonts untouched.
    if (!m_RuntimeFontFollowsInterface || m_InterfaceFontName.isEmpty()
            || !qGuiApp) {
        return;
    }
    QString family = m_InterfaceFontName;
    qreal pointSize = -1;
    const int lastSpace = family.lastIndexOf(QLatin1Char(' '));
    if (lastSpace > 0) {
        bool numeric = false;
        const qreal parsed =
                family.mid(lastSpace + 1).toDouble(&numeric);
        if (numeric && parsed >= 6 && parsed <= 72) {
            pointSize = parsed;
            family.truncate(lastSpace);
        }
    }
    QFont font(family);
    if (pointSize > 0) {
        font.setPointSizeF(pointSize);
    }
    QGuiApplication::setFont(font);
    emit interfaceFontPixelSizeChanged();
}

int AppearanceProvider::interfaceFontPixelSize() const
{
    // The desktop expresses its font in points; every GNOME metric derived
    // from it is in pixels. QFontInfo resolves whichever the application font
    // actually carries, so this works whether the size arrived as points or
    // pixels, and reports 0 when there is nothing to follow.
    if (!qGuiApp) {
        return 0;
    }
    const int resolved = QFontInfo(QGuiApplication::font()).pixelSize();
    return resolved > 0 ? resolved : 0;
}

void AppearanceProvider::setReducedMotion(bool value)
{
    if (m_ReducedMotion == value) {
        return;
    }
    m_ReducedMotion = value;
    emit reducedMotionChanged();
}
