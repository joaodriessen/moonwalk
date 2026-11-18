#include <QtTest>

class SmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void testTestInfrastructureRuns();
};

void SmokeTest::testTestInfrastructureRuns()
{
    QVERIFY(QCoreApplication::instance() != nullptr);
}

QTEST_MAIN(SmokeTest)

#include "tst_smoke.moc"
