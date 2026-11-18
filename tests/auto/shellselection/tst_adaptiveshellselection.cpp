#include <QtTest>

#include "../../../app/application/adaptiveshellselection.h"
#include "../../../app/application/materialstylebootstrap.h"

class AdaptiveShellSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void parsesExactOverrideAndEligibility_data();
    void parsesExactOverrideAndEligibility();
    void legacyMaterialDefaultsRestoreBaseline();
    void legacyMaterialDefaultsPreserveCallerOverrides();
    void nonMaterialStyleLeavesEnvironmentAlone();

private:
    struct EnvironmentValue {
        bool wasSet = false;
        QByteArray value;
    };

    QHash<QByteArray, EnvironmentValue> m_OriginalEnvironment;
};

namespace {

const QList<QByteArray> materialEnvironmentNames {
    QByteArrayLiteral("QT_QUICK_CONTROLS_MATERIAL_THEME"),
    QByteArrayLiteral("QT_QUICK_CONTROLS_MATERIAL_ACCENT"),
    QByteArrayLiteral("QT_QUICK_CONTROLS_MATERIAL_VARIANT"),
    QByteArrayLiteral("QT_QUICK_CONTROLS_MATERIAL_PRIMARY"),
};

}

void AdaptiveShellSelectionTest::init()
{
    m_OriginalEnvironment.clear();
    for (const QByteArray& name : materialEnvironmentNames) {
        m_OriginalEnvironment.insert(
                name,
                { qEnvironmentVariableIsSet(name.constData()), qgetenv(name.constData()) });
        qunsetenv(name.constData());
    }
}

void AdaptiveShellSelectionTest::cleanup()
{
    for (auto it = m_OriginalEnvironment.cbegin();
         it != m_OriginalEnvironment.cend();
         ++it) {
        if (it.value().wasSet) {
            qputenv(it.key().constData(), it.value().value);
        }
        else {
            qunsetenv(it.key().constData());
        }
    }
}

void AdaptiveShellSelectionTest::parsesExactOverrideAndEligibility_data()
{
    QTest::addColumn<QByteArray>("overrideValue");
    QTest::addColumn<bool>("eligible");
    QTest::addColumn<bool>("expectedValid");
    QTest::addColumn<bool>("expectedEnabled");
    QTest::addColumn<bool>("expectedAdaptive");

    QTest::newRow("unset defaults to adaptive")
            << QByteArray() << true << true << true << true;
    QTest::newRow("zero opts out to legacy")
            << QByteArrayLiteral("0") << true << true << false << false;
    QTest::newRow("one selects adaptive")
            << QByteArrayLiteral("1") << true << true << true << true;
    QTest::newRow("one cannot replace a CLI GUI route")
            << QByteArrayLiteral("1") << false << true << true << false;
    QTest::newRow("the default cannot replace a CLI GUI route")
            << QByteArray() << false << true << true << false;
    QTest::newRow("invalid text fails closed")
            << QByteArrayLiteral("true") << true << false << false << false;
    QTest::newRow("multi-digit value fails closed")
            << QByteArrayLiteral("01") << true << false << false << false;
}

void AdaptiveShellSelectionTest::parsesExactOverrideAndEligibility()
{
    QFETCH(QByteArray, overrideValue);
    QFETCH(bool, eligible);
    QFETCH(bool, expectedValid);
    QFETCH(bool, expectedEnabled);
    QFETCH(bool, expectedAdaptive);

    const AdaptiveShellSelection selection =
            AdaptiveShellSelection::fromOverride(overrideValue);

    QCOMPARE(selection.isValidOverride(), expectedValid);
    QCOMPARE(selection.isEnabled(), expectedEnabled);
    QCOMPARE(selection.shouldUseAdaptiveShell(eligible), expectedAdaptive);
}

void AdaptiveShellSelectionTest::legacyMaterialDefaultsRestoreBaseline()
{
    MaterialStyleBootstrap::applyLegacyDefaults(true);

    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_THEME"),
             QByteArrayLiteral("Dark"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT"),
             QByteArrayLiteral("Purple"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT"),
             QByteArrayLiteral("Dense"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY"),
             QByteArrayLiteral("#3F51B5"));
}

void AdaptiveShellSelectionTest::legacyMaterialDefaultsPreserveCallerOverrides()
{
    qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Light");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "Teal");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Normal");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", "#123456");

    MaterialStyleBootstrap::applyLegacyDefaults(true);

    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_THEME"),
             QByteArrayLiteral("Dark"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT"),
             QByteArrayLiteral("Teal"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT"),
             QByteArrayLiteral("Normal"));
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY"),
             QByteArrayLiteral("#123456"));
}

void AdaptiveShellSelectionTest::nonMaterialStyleLeavesEnvironmentAlone()
{
    MaterialStyleBootstrap::applyLegacyDefaults(false);

    for (const QByteArray& name : materialEnvironmentNames) {
        QVERIFY2(!qEnvironmentVariableIsSet(name.constData()),
                 name.constData());
    }
}

QTEST_MAIN(AdaptiveShellSelectionTest)

#include "tst_adaptiveshellselection.moc"
