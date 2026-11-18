#include <QtTest>

#include "../../../app/platform/platformprofileresolver.h"

class PlatformProfileTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesDeterministicProfile_data();
    void resolvesDeterministicProfile();
};

void PlatformProfileTest::resolvesDeterministicProfile_data()
{
    QTest::addColumn<ProfileInputs>("inputs");
    QTest::addColumn<PlatformProfile::Kind>("expectedKind");
    QTest::addColumn<QString>("expectedReason");
    QTest::addColumn<QStringList>("expectedCapabilities");

    QTest::newRow("valid override wins over the active session")
            << ProfileInputs { "plasma", "GNOME", false }
            << PlatformProfile::Plasma
            << QStringLiteral("explicit override")
            << QStringList {};
    QTest::newRow("GNOME token selects GNOME even with KDE support installed")
            << ProfileInputs { QString(), "GNOME:GNOME", true }
            << PlatformProfile::Gnome
            << QStringLiteral("active GNOME session")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
    QTest::newRow("mixed GNOME and Plasma session fails closed")
            << ProfileInputs { QString(), "GNOME:KDE", true }
            << PlatformProfile::Generic
            << QStringLiteral("mixed desktop session")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
    QTest::newRow("KDE session selects Plasma only with its confirmed capability")
            << ProfileInputs { QString(), "KDE", true }
            << PlatformProfile::Plasma
            << QStringLiteral("active Plasma session")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
    QTest::newRow("malformed override fails closed without leaking its value")
            << ProfileInputs { "plasma; rm -rf /", "GNOME", true }
            << PlatformProfile::Generic
            << QStringLiteral("invalid explicit override")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
    QTest::newRow("missing Plasma capability falls back to generic")
            << ProfileInputs { QString(), "KDE", false }
            << PlatformProfile::Generic
            << QStringLiteral("generic fallback")
            << QStringList {};
    QTest::newRow("substring is not an active session token")
            << ProfileInputs { QString(), "NOTGNOME", true }
            << PlatformProfile::Generic
            << QStringLiteral("generic fallback")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
    QTest::newRow("unknown session uses generic fallback")
            << ProfileInputs { QString(), "Hyprland", true }
            << PlatformProfile::Generic
            << QStringLiteral("generic fallback")
            << QStringList { QStringLiteral("kde-quick-controls-style") };
}

void PlatformProfileTest::resolvesDeterministicProfile()
{
    QFETCH(ProfileInputs, inputs);
    QFETCH(PlatformProfile::Kind, expectedKind);
    QFETCH(QString, expectedReason);
    QFETCH(QStringList, expectedCapabilities);

    const std::unique_ptr<PlatformProfile> profile = PlatformProfileResolver::resolve(inputs);

    QVERIFY(profile);
    QCOMPARE(profile->kind(), expectedKind);
    QCOMPARE(profile->selectionReason(), expectedReason);
    QCOMPARE(profile->confirmedCapabilities(), expectedCapabilities);
}

QTEST_MAIN(PlatformProfileTest)

#include "tst_platformprofile.moc"
