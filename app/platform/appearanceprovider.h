#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariant>

#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
#include <QDBusVariant>
#endif

class AppearanceProvider final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ColorScheme colorScheme READ colorScheme NOTIFY colorSchemeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY colorSchemeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY highContrastChanged)
    Q_PROPERTY(qreal fontScale READ fontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(QString windowButtonLayout READ windowButtonLayout NOTIFY windowButtonLayoutChanged)
    Q_PROPERTY(QString interfaceFontName READ interfaceFontName NOTIFY interfaceFontNameChanged)
    Q_PROPERTY(int interfaceFontPixelSize READ interfaceFontPixelSize NOTIFY interfaceFontPixelSizeChanged)

public:
    // The values deliberately match Qt::ColorScheme in QML. Portal values are
    // translated at the ingestion boundary because the portal uses 1=dark and
    // 2=light, while Qt uses 1=light and 2=dark.
    enum ColorScheme {
        NoPreference = 0,
        Light = 1,
        Dark = 2,
    };
    Q_ENUM(ColorScheme)

    enum InitializationMode {
        Runtime,
        Isolated,
    };

    explicit AppearanceProvider(
            InitializationMode mode = Runtime,
            QObject* parent = nullptr);

    static InitializationMode initializationModeForAdapter(
            bool adwaitaSurfaceActive,
            bool adaptiveShellRequested);

    ColorScheme colorScheme() const;
    bool dark() const;
    QColor accentColor() const;
    bool highContrast() const;
    qreal fontScale() const;
    bool reducedMotion() const;
    QString windowButtonLayout() const;
    QString interfaceFontName() const;
    int interfaceFontPixelSize() const;

    // Production DBus replies and deterministic tests share this narrow
    // ingestion seam. Unknown namespaces and keys are intentionally ignored.
    void applyPortalSetting(
            const QString& nameSpace,
            const QString& key,
            const QVariant& value);

signals:
    void windowButtonLayoutChanged();
    void interfaceFontNameChanged();
    void interfaceFontPixelSizeChanged();
    void colorSchemeChanged();
    void accentColorChanged();
    void highContrastChanged();
    void fontScaleChanged();
    void reducedMotionChanged();

private slots:
#if defined(STARLIGHT_GNOME_DESKTOP_ADAPTER)
    void handleSettingChanged(
            const QString& nameSpace,
            const QString& key,
            const QDBusVariant& value);
#endif

private:
    void setWindowButtonLayout(const QString& value);
    void setInterfaceFontName(const QString& value);
    void applyInterfaceFont();

    void initializeRuntimeSources();
    void setColorScheme(ColorScheme value);
    void setAccentColor(const QColor& value);
    void setHighContrast(bool value);
    void setFontScale(qreal value);
    void setReducedMotion(bool value);

    ColorScheme m_ColorScheme = NoPreference;
    QColor m_AccentColor = QColor(QStringLiteral("#3584e4"));
    bool m_HighContrast = false;
    qreal m_FontScale = 1.0;
    bool m_ReducedMotion = false;
    // GNOME stock layout: window controls on the right, close only.
    QString m_WindowButtonLayout = QStringLiteral(":close");
    QString m_InterfaceFontName;
    bool m_RuntimeFontFollowsInterface = false;
    bool m_HaveStandardScheme = false;
    bool m_HaveStandardContrast = false;
    bool m_HaveStandardReducedMotion = false;
};
