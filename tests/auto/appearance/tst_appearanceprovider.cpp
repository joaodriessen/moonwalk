#include <QtTest>

#include <QSignalSpy>

#include "../../../app/gui/adaptive/gnome/gnomechromeselector.h"
#include "../../../app/platform/appearanceprovider.h"

class AppearanceProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWithSafeIsolatedDefaults();
    void standardAppearanceSettingsUpdateLiveProperties();
    void invalidPortalValuesReturnToSafeDefaults();
    void standardizedSettingsOverrideDesktopFallbacks();
    void unrelatedPortalSettingsCannotMutateAppearance();
    void unchangedValuesDoNotEmitDuplicateSignals();
    void runtimeSourcesRequireRequestedGnomeRoute();
    void chromeSelectionIsOptionalAndFailSafe_data();
    void chromeSelectionIsOptionalAndFailSafe();
};

void AppearanceProviderTest::startsWithSafeIsolatedDefaults()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);

    QCOMPARE(provider.colorScheme(), AppearanceProvider::NoPreference);
    QCOMPARE(provider.accentColor(), QColor(QStringLiteral("#3584e4")));
    QCOMPARE(provider.highContrast(), false);
    QCOMPARE(provider.fontScale(), 1.0);
    QCOMPARE(provider.reducedMotion(), false);
}

void AppearanceProviderTest::standardAppearanceSettingsUpdateLiveProperties()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);
    QSignalSpy schemeSpy(&provider, &AppearanceProvider::colorSchemeChanged);
    QSignalSpy accentSpy(&provider, &AppearanceProvider::accentColorChanged);
    QSignalSpy contrastSpy(&provider, &AppearanceProvider::highContrastChanged);
    QSignalSpy scaleSpy(&provider, &AppearanceProvider::fontScaleChanged);
    QSignalSpy motionSpy(&provider, &AppearanceProvider::reducedMotionChanged);

    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("color-scheme"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("accent-color"),
            QVariantList { 0.2, 0.4, 0.6 });
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("contrast"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.interface"),
            QStringLiteral("text-scaling-factor"),
            1.75);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            1u);

    QCOMPARE(provider.colorScheme(), AppearanceProvider::Dark);
    QCOMPARE(provider.accentColor(), QColor::fromRgbF(0.2, 0.4, 0.6));
    QCOMPARE(provider.highContrast(), true);
    QCOMPARE(provider.fontScale(), 1.75);
    QCOMPARE(provider.reducedMotion(), true);
    QCOMPARE(schemeSpy.count(), 1);
    QCOMPARE(accentSpy.count(), 1);
    QCOMPARE(contrastSpy.count(), 1);
    QCOMPARE(scaleSpy.count(), 1);
    QCOMPARE(motionSpy.count(), 1);
}

void AppearanceProviderTest::invalidPortalValuesReturnToSafeDefaults()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);

    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.a11y.interface"),
            QStringLiteral("high-contrast"),
            2u);
    QCOMPARE(provider.highContrast(), false);

    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("color-scheme"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("accent-color"),
            QVariantList { 0.1, 0.2, 0.3 });
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("contrast"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.interface"),
            QStringLiteral("text-scaling-factor"),
            2.0);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            1u);

    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("color-scheme"),
            99u);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("accent-color"),
            QVariantList { -0.1, 0.5, 1.1 });
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("contrast"),
            99u);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.interface"),
            QStringLiteral("text-scaling-factor"),
            9.0);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            99u);

    QCOMPARE(provider.colorScheme(), AppearanceProvider::NoPreference);
    QCOMPARE(provider.accentColor(), QColor(QStringLiteral("#3584e4")));
    QCOMPARE(provider.highContrast(), false);
    QCOMPARE(provider.fontScale(), 1.0);
    QCOMPARE(provider.reducedMotion(), false);
}

void AppearanceProviderTest::standardizedSettingsOverrideDesktopFallbacks()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);

    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.a11y.interface"),
            QStringLiteral("high-contrast"),
            true);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.a11y.interface"),
            QStringLiteral("reduced-motion"),
            QStringLiteral("reduce"));
    QCOMPARE(provider.highContrast(), true);
    QCOMPARE(provider.reducedMotion(), true);

    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("contrast"),
            0u);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            0u);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.a11y.interface"),
            QStringLiteral("high-contrast"),
            true);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.a11y.interface"),
            QStringLiteral("reduced-motion"),
            QStringLiteral("reduce"));

    QCOMPARE(provider.highContrast(), false);
    QCOMPARE(provider.reducedMotion(), false);
}

void AppearanceProviderTest::unrelatedPortalSettingsCannotMutateAppearance()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);

    provider.applyPortalSetting(
            QStringLiteral("org.example.private"),
            QStringLiteral("color-scheme"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.gnome.desktop.interface"),
            QStringLiteral("icon-theme"),
            QStringLiteral("PrivateThemeName"));

    QCOMPARE(provider.colorScheme(), AppearanceProvider::NoPreference);
    QCOMPARE(provider.accentColor(), QColor(QStringLiteral("#3584e4")));
    QCOMPARE(provider.highContrast(), false);
    QCOMPARE(provider.fontScale(), 1.0);
    QCOMPARE(provider.reducedMotion(), false);
}

void AppearanceProviderTest::unchangedValuesDoNotEmitDuplicateSignals()
{
    AppearanceProvider provider(AppearanceProvider::Isolated);
    QSignalSpy motionSpy(&provider, &AppearanceProvider::reducedMotionChanged);

    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            1u);
    provider.applyPortalSetting(
            QStringLiteral("org.freedesktop.appearance"),
            QStringLiteral("reduced-motion"),
            1u);

    QCOMPARE(provider.reducedMotion(), true);
    QCOMPARE(motionSpy.count(), 1);
}

void AppearanceProviderTest::runtimeSourcesRequireRequestedGnomeRoute()
{
    QCOMPARE(
            AppearanceProvider::initializationModeForAdapter(false, false),
            AppearanceProvider::Isolated);
    QCOMPARE(
            AppearanceProvider::initializationModeForAdapter(false, true),
            AppearanceProvider::Isolated);
    QCOMPARE(
            AppearanceProvider::initializationModeForAdapter(true, false),
            AppearanceProvider::Isolated);
    QCOMPARE(
            AppearanceProvider::initializationModeForAdapter(true, true),
            AppearanceProvider::Runtime);
}

void AppearanceProviderTest::chromeSelectionIsOptionalAndFailSafe_data()
{
    QTest::addColumn<GnomeChromeInputs>("inputs");
    QTest::addColumn<GnomeChromeDecision::Kind>("expectedKind");
    QTest::addColumn<QByteArray>("expectedEnvironmentValue");

    QTest::newRow("standalone QAdwaita is preferred when loadable")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME"),
                   {
                       { true, true, true },
                       { true, false, true },
                   },
               }
            << GnomeChromeDecision::QAdwaita
            << QByteArrayLiteral("qadwaitadecorations");
    QTest::newRow("in-tree Adwaita is the functional fallback")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME"),
                   { { true, false, true } },
               }
            << GnomeChromeDecision::Adwaita
            << QByteArrayLiteral("adwaita");
    QTest::newRow("absent plugin leaves Qt functional default")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME"),
                   {},
               }
            << GnomeChromeDecision::QtDefault
            << QByteArray();
    QTest::newRow("standalone load failure falls back to in-tree Adwaita")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME"),
                   {
                       { true, true, false },
                       { true, false, true },
                   },
               }
            << GnomeChromeDecision::Adwaita
            << QByteArrayLiteral("adwaita");
    QTest::newRow("all plugin loads failing leaves Qt functional default")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME"),
                   { { true, true, false } },
               }
            << GnomeChromeDecision::QtDefault
            << QByteArray();
    QTest::newRow("explicit user decoration is never replaced")
            << GnomeChromeInputs {
                   true,
                   true,
                   QStringLiteral("GNOME"),
                   { { true, true, true } },
               }
            << GnomeChromeDecision::PreserveExplicit
            << QByteArray();
    QTest::newRow("mixed GNOME KDE session does not force GNOME chrome")
            << GnomeChromeInputs {
                   false,
                   true,
                   QStringLiteral("GNOME:KDE"),
                   { { true, true, true } },
               }
            << GnomeChromeDecision::QtDefault
            << QByteArray();
    QTest::newRow("GNOME X11 never selects Wayland chrome")
            << GnomeChromeInputs {
                   false,
                   false,
                   QStringLiteral("GNOME"),
                   { { true, true, true } },
               }
            << GnomeChromeDecision::QtDefault
            << QByteArray();
}

void AppearanceProviderTest::chromeSelectionIsOptionalAndFailSafe()
{
    QFETCH(GnomeChromeInputs, inputs);
    QFETCH(GnomeChromeDecision::Kind, expectedKind);
    QFETCH(QByteArray, expectedEnvironmentValue);

    const GnomeChromeDecision decision =
            GnomeChromeSelector::select(inputs);

    QCOMPARE(decision.kind, expectedKind);
    QCOMPARE(decision.environmentValue, expectedEnvironmentValue);
}

QTEST_GUILESS_MAIN(AppearanceProviderTest)

#include "tst_appearanceprovider.moc"
