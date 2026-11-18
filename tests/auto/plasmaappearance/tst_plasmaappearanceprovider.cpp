#include <QtTest>

#include <QApplication>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QStyleFactory>

#include "../../../app/platform/plasmaappearanceprovider.h"

namespace {

QPalette distinctivePalette()
{
    QPalette palette;
    const QPalette::ColorRole roles[] = {
        QPalette::Window,
        QPalette::WindowText,
        QPalette::Base,
        QPalette::AlternateBase,
        QPalette::Text,
        QPalette::PlaceholderText,
        QPalette::Button,
        QPalette::ButtonText,
        QPalette::Highlight,
        QPalette::HighlightedText,
        QPalette::ToolTipBase,
        QPalette::ToolTipText,
        QPalette::Light,
        QPalette::Midlight,
        QPalette::Mid,
        QPalette::Dark,
        QPalette::Shadow,
    };
    const QPalette::ColorGroup groups[] = {
        QPalette::Active,
        QPalette::Inactive,
        QPalette::Disabled,
    };

    int value = 1;
    for (const QPalette::ColorGroup group : groups) {
        for (const QPalette::ColorRole role : roles) {
            palette.setColor(group, role,
                             QColor(value, value + 1, value + 2));
            value += 3;
        }
    }
    return palette;
}

class FixedPaletteStyle final : public QProxyStyle
{
public:
    explicit FixedPaletteStyle(const QPalette& palette)
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
        , m_Palette(palette)
    {
    }

    QPalette standardPalette() const override
    {
        return m_Palette;
    }

private:
    QPalette m_Palette;
};

} // namespace

class PlasmaAppearanceProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void startsUnreadyWhenRuntimeIsInactive();
    void publishesEverySemanticRoleAsOneRevision();
    void identicalSnapshotDoesNotEmit();
    void queuedRefreshRequestsCoalesce();
    void runtimeRequiresAdaptivePlasmaRoute();
    void kdeInvalidationsCoalesceNativeStyleSnapshot();
    void kdeInvalidationReloadsNativeStylePalette();
};

void PlasmaAppearanceProviderTest::startsUnreadyWhenRuntimeIsInactive()
{
    PlasmaAppearanceProvider provider(false);

    QCOMPARE(provider.ready(), false);
    QCOMPARE(provider.revision(), quint64(0));
    QCOMPARE(provider.active(), QVariantMap());
    QCOMPARE(provider.inactive(), QVariantMap());
    QCOMPARE(provider.disabled(), QVariantMap());
    QCOMPARE(provider.widgetStyle(), QString());
}

void PlasmaAppearanceProviderTest::publishesEverySemanticRoleAsOneRevision()
{
    PlasmaAppearanceProvider provider(false);
    QSignalSpy spy(&provider, &PlasmaAppearanceProvider::snapshotChanged);
    const QPalette palette = distinctivePalette();

    provider.applySnapshotForTest(palette, QStringLiteral("custom-style"));

    QCOMPARE(provider.ready(), true);
    QCOMPARE(provider.revision(), quint64(1));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(provider.widgetStyle(), QStringLiteral("custom-style"));
    QCOMPARE(provider.active().value(QStringLiteral("window")).value<QColor>(),
             palette.color(QPalette::Active, QPalette::Window));
    QCOMPARE(provider.active().value(QStringLiteral("highlightedText")).value<QColor>(),
             palette.color(QPalette::Active, QPalette::HighlightedText));
    QCOMPARE(provider.inactive().value(QStringLiteral("alternateBase")).value<QColor>(),
             palette.color(QPalette::Inactive, QPalette::AlternateBase));
    QCOMPARE(provider.disabled().value(QStringLiteral("text")).value<QColor>(),
             palette.color(QPalette::Disabled, QPalette::Text));
    QCOMPARE(provider.disabled().value(QStringLiteral("buttonText")).value<QColor>(),
             palette.color(QPalette::Disabled, QPalette::ButtonText));
    QCOMPARE(provider.active().size(), 17);
    QCOMPARE(provider.inactive().size(), 17);
    QCOMPARE(provider.disabled().size(), 17);
}

void PlasmaAppearanceProviderTest::identicalSnapshotDoesNotEmit()
{
    PlasmaAppearanceProvider provider(false);
    QSignalSpy spy(&provider, &PlasmaAppearanceProvider::snapshotChanged);
    const QPalette palette = distinctivePalette();

    provider.applySnapshotForTest(palette, QStringLiteral("custom-style"));
    provider.applySnapshotForTest(palette, QStringLiteral("custom-style"));

    QCOMPARE(provider.revision(), quint64(1));
    QCOMPARE(spy.count(), 1);
}

void PlasmaAppearanceProviderTest::queuedRefreshRequestsCoalesce()
{
    PlasmaAppearanceProvider provider(false);
    QSignalSpy spy(&provider, &PlasmaAppearanceProvider::snapshotChanged);
    QApplication::setPalette(distinctivePalette());

    provider.requestRefresh();
    provider.requestRefresh();
    provider.requestRefresh();

    QTRY_COMPARE(provider.revision(), quint64(1));
    QCOMPARE(spy.count(), 1);
}

void PlasmaAppearanceProviderTest::runtimeRequiresAdaptivePlasmaRoute()
{
    QCOMPARE(PlasmaAppearanceProvider::runtimeActiveForAdapter(false, false),
             false);
    QCOMPARE(PlasmaAppearanceProvider::runtimeActiveForAdapter(false, true),
             false);
    QCOMPARE(PlasmaAppearanceProvider::runtimeActiveForAdapter(true, false),
             false);
    QCOMPARE(PlasmaAppearanceProvider::runtimeActiveForAdapter(true, true),
             true);
}

void PlasmaAppearanceProviderTest::kdeInvalidationsCoalesceNativeStyleSnapshot()
{
    PlasmaAppearanceProvider provider(false);
    QSignalSpy spy(&provider, &PlasmaAppearanceProvider::snapshotChanged);
    const QPalette palette = distinctivePalette();
    QApplication::setPalette(palette);

    provider.handleKdeGlobalSettingsChange(0, 0);
    provider.handleKdeGlobalSettingsChange(1, 9);
    provider.handleKdeGlobalSettingsChange(2, 42);

    QTRY_COMPARE(provider.revision(), quint64(1));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(provider.active().value(QStringLiteral("window")).value<QColor>(),
             QApplication::palette().color(QPalette::Active, QPalette::Window));
}

void PlasmaAppearanceProviderTest::kdeInvalidationReloadsNativeStylePalette()
{
    const QPalette oldPalette = distinctivePalette();
    QPalette lightPalette = distinctivePalette();
    lightPalette.setColor(QPalette::Active, QPalette::Window,
                          QColor(246, 246, 248));
    lightPalette.setColor(QPalette::Active, QPalette::WindowText,
                          QColor(35, 35, 38));
    QApplication::setPalette(oldPalette);
    QApplication::setStyle(new FixedPaletteStyle(lightPalette));

    PlasmaAppearanceProvider provider(false);
    provider.handleKdeGlobalSettingsChange(0, 0);

    QTRY_COMPARE(provider.revision(), quint64(1));
    QCOMPARE(provider.active().value(QStringLiteral("window")).value<QColor>(),
             lightPalette.color(QPalette::Active, QPalette::Window));
    QCOMPARE(QApplication::palette().color(QPalette::Active, QPalette::Window),
             lightPalette.color(QPalette::Active, QPalette::Window));
}

QTEST_MAIN(PlasmaAppearanceProviderTest)
#include "tst_plasmaappearanceprovider.moc"
