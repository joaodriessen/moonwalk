#include <QtTest>

#include <QSettings>
#include <QDateTime>
#include "../../../app/streaming/streamplan.h"
#include <QTemporaryDir>

#include <memory>
#include <utility>

#include "../../../app/cli/commandlineparser.h"
#include "../../../app/settings/streamingpreferences.h"

class StreamingPreferencesTest : public QObject
{
    Q_OBJECT

private slots:
    void displayTargetLookupPreservesSelectedHostAndEffectiveMode();
    void init();
    void cleanup();

    void injectedStoreIsTheOnlyStoreUsed();
    void previewDoesNotReuseDecoderResultsFromAnotherMode();
    void lastSelectedHostPersists();
    void viewScaleIsPerHostAndOldRecoveryRecordsRemainUsable();
    void unknownHostFallsBackToTheFrozenTemplate();
    void hostsRemainIsolatedWhenSwitchingAToBToA();
    void migratesKnownHostsFromTheFlatTemplate();
    void savesConnectionFieldsUnderTheSelectedHost();
    void explicitDisplayFlagsRecomputeBitrate();
    void cliOverridesWinAfterTheHostOverlay();
    void workflowFacadeCommitsDisplayChangesExactlyOnce();
    void freshHostSeedsNativeModeBeforeAnyOtherSave();
    void automaticResolutionFollowsDisplayAndManualChoiceStopsIt();
    void existingHostDisplayModeIsNeverOverwritten();
    void localCursorIsOnForNewHostsAndSavedChoicesStay();
    void partialStoredModePreservesEveryExplicitKey();
    void refreshRateFollowsDisplayAndManualChoiceStopsIt();
    void refreshRateMatchesFastAndSlowDisplaysAndFallsBackTo60();
    void bitratePolicyBoundsEveryStoredAndRecommendedPath();
    void lastEnabledCaptureModeSurvivesReload();
    void sessionCloneIsTransientAndKeepTargetsCapturedHost();
    void namedPresetsRestoreAtomicallyAndRejectMalformedRecords();
    void successfulRetryUsesCapturedHostWithoutReplacingSavedPreferences();
    void removedSettingsMigrateToAutomaticAndManualChoicesStay();
    void presetsAndLastWorkingSettingsLoadSanitised();
    void freshHostStartsAutomaticWhateverTheTemplateSays();
    void displayAndInputMigrateToAutomaticUnlessChosen();
    void legacyDisplaySettingsDoNotSelectGamePointer_data();
    void legacyDisplaySettingsDoNotSelectGamePointer();
    void freshHostDisplayAndInputStartAutomatic();
    void automaticDisplayAndInputWinInSessionsAndManualValuesStay();
    void presetsAndLastWorkingSettingsDeriveDisplayAndInputModes();
    void cliPointerIsAManualInputChoice();
    void cliCanExplicitlyDisableFramePacing();
    void explicitCliChoicesSurviveAutomaticSessionPolicy();
    void relearningOnlyClearsThisHostsMeasurements();
    void bitrateLearningSurvivesAReversibleModeTrial();

private:
    QSettings& settings();
    StreamingPreferences* loadPreferences();

    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<QSettings> m_settings;
    QString m_settingsPath;
};

void StreamingPreferencesTest::bitrateLearningSurvivesAReversibleModeTrial()
{
    StreamingPreferences::rememberAutomaticBitrate("mode-trial", 9000, "native/60/h264");
    StreamingPreferences::rememberAutomaticBitrate("mode-trial", 6500, "smaller/60/h264");
    QCOMPARE(StreamingPreferences::rememberedAutomaticBitrate("mode-trial", "native/60/h264"), 9000);
    QCOMPARE(StreamingPreferences::rememberedAutomaticBitrate("mode-trial", "smaller/60/h264"), 6500);
    QCOMPARE(StreamingPreferences::rememberedAutomaticBitrate("mode-trial", "different-route"), 0);
}

void StreamingPreferencesTest::viewScaleIsPerHostAndOldRecoveryRecordsRemainUsable()
{
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("a");
    QCOMPARE(prefs->viewScale, QString("fit"));
    prefs->viewScale = "actual";
    prefs->save();
    prefs->rememberSuccessfulSettings();
    prefs->setCurrentHost("b");
    QCOMPARE(prefs->viewScale, QString("fit"));
    prefs->setCurrentHost("a");
    QCOMPARE(prefs->viewScale, QString("actual"));
    QSettings external(m_settingsPath, QSettings::IniFormat);
    auto previous = external.value("hostsettings/a/lastsuccessful").toMap();
    QVERIFY(prefs->lastConnectedAt() > 0);
    prefs->setCurrentHost("b");
    QCOMPARE(prefs->lastConnectedAt(), qint64(0));
    prefs->setCurrentHost("a");
    previous.remove("viewscale"); // A recovery record written before this option existed.
    external.setValue("hostsettings/a/lastsuccessful", previous);
    external.sync();
    auto retry = prefs->cloneWithLastSuccessfulSettings();
    QVERIFY(retry);
    QCOMPARE(retry->viewScale, QString("fit"));
}

void StreamingPreferencesTest::sessionCloneIsTransientAndKeepTargetsCapturedHost()
{
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("a");
    prefs->width = 2560; prefs->height = 1440; prefs->enableHdr = true;
    prefs->desktopScale = "2"; prefs->adaptiveBitrate = true; prefs->localTextInput = true;
    prefs->connectionAddress = "host.example:48001";
    prefs->save();
    auto trial = prefs->cloneForSession(true);
    QCOMPARE(trial->width, 1920);
    QCOMPARE(trial->height, 1080);
    QVERIFY(!trial->enableHdr);
    QCOMPARE(trial->connectionAddress, QString("host.example:48001"));
    QVERIFY(trial->localTextInput);
    QSignalSpy saves(prefs, &StreamingPreferences::saveCompleted);
    trial->save();
    QCOMPARE(saves.count(), 0);
    QCOMPARE(prefs->width, 2560);
    prefs->setCurrentHost("b");
    const int otherWidth = prefs->width;
    QVERIFY(prefs->keepSessionSettings(*trial));
    QCOMPARE(prefs->width, otherWidth);
    prefs->setCurrentHost("a");
    QCOMPARE(prefs->width, 1920);
    QVERIFY(!prefs->adaptiveBitrate);
    QCOMPARE(prefs->desktopScale, QString("auto"));
    trial->rememberSuccessfulSettings();
    prefs->width = 3840; prefs->save();
    QVERIFY(prefs->restoreLastSuccessfulSettings());
    QCOMPARE(prefs->width, 1920);
}

void StreamingPreferencesTest::successfulRetryUsesCapturedHostWithoutReplacingSavedPreferences()
{
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("a");
    QVERIFY(!prefs->cloneWithLastSuccessfulSettings());
    prefs->width = 1920;
    prefs->height = 1080;
    prefs->save();
    prefs->rememberSuccessfulSettings();
    prefs->width = 3840;
    prefs->save();
    auto failed = prefs->cloneForSession();
    prefs->setCurrentHost("b");
    prefs->width = 2560;
    prefs->save();
    QSignalSpy saves(prefs, &StreamingPreferences::saveCompleted);
    auto retry = failed->cloneWithLastSuccessfulSettings();
    QVERIFY(retry);
    QCOMPARE(retry->width, 1920);
    QCOMPARE(retry->height, 1080);
    QCOMPARE(failed->width, 3840);
    QCOMPARE(prefs->width, 2560);
    retry->save();
    QCOMPARE(saves.count(), 0);
    prefs->setCurrentHost("a");
    QCOMPARE(prefs->width, 3840);
    QSettings external(m_settingsPath, QSettings::IniFormat);
    external.setValue("hostsettings/a/lastsuccessful", QVariantMap{{"width", 1920}});
    external.sync();
    QVERIFY(!failed->cloneWithLastSuccessfulSettings());
}

void StreamingPreferencesTest::namedPresetsRestoreAtomicallyAndRejectMalformedRecords()
{
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("a");
    prefs->width = 2560; prefs->height = 1440; prefs->bitrateKbps = 42000;
    prefs->autoAdjustBitrate = false; prefs->desktopScale = "2";
    prefs->adaptiveBitrate = true; prefs->localTextInput = true;
    prefs->save();
    QVERIFY(prefs->saveHostPreset("LAN"));
    const QString id = prefs->hostPresets().first().toMap().value("id").toString();
    QVERIFY(prefs->duplicateHostPreset(id, "Work"));
    QVERIFY(prefs->renameHostPreset(id, "Home"));
    prefs->width = 1280; prefs->height = 720; prefs->desktopScale = "auto";
    prefs->adaptiveBitrate = false; prefs->localTextInput = false;
    QVERIFY(prefs->applyHostPreset(id));
    QCOMPARE(prefs->width, 2560);
    QCOMPARE(prefs->bitrateKbps, 42000);
    QVERIFY(!prefs->autoAdjustBitrate);
    QVERIFY(prefs->adaptiveBitrate && prefs->localTextInput);
    QCOMPARE(prefs->activePresetName(), QString("Home"));
    QSettings external(m_settingsPath, QSettings::IniFormat);
    external.setValue("hostsettings/a/presets/broken/values", QVariantMap{{"width", 3840}});
    external.sync();
    QVERIFY(!prefs->applyHostPreset("broken"));
    QCOMPARE(prefs->width, 2560);
    prefs->setCurrentHost("b");
    QVERIFY(prefs->hostPresets().isEmpty());
    prefs->setCurrentHost("a");
    QVERIFY(prefs->removeHostPreset(id));
    QVERIFY(!prefs->applyHostPreset(id));
    QCOMPARE(prefs->width, 2560);
}

void StreamingPreferencesTest::init()
{
    StreamingPreferences::resetForTesting();

    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());

    m_settingsPath = m_directory->filePath(QStringLiteral("preferences.ini"));
    m_settings = std::make_unique<QSettings>(m_settingsPath, QSettings::IniFormat);
    m_settings->setFallbacksEnabled(false);
}

void StreamingPreferencesTest::cleanup()
{
    StreamingPreferences::resetForTesting();
    m_settings.reset();
    m_directory.reset();
}

QSettings& StreamingPreferencesTest::settings()
{
    Q_ASSERT(m_settings);
    return *m_settings;
}

StreamingPreferences* StreamingPreferencesTest::loadPreferences()
{
    settings().sync();
    StreamingPreferences* preferences = StreamingPreferences::getForTesting(std::move(m_settings));
    Q_ASSERT(preferences != nullptr);
    return preferences;
}

void StreamingPreferencesTest::lastSelectedHostPersists()
{
    StreamingPreferences* preferences = loadPreferences();
    QCOMPARE(preferences->lastSelectedHost(), QString());
    preferences->setLastSelectedHost(QStringLiteral("host-b"));
    QCOMPARE(preferences->lastSelectedHost(), QStringLiteral("host-b"));
}

void StreamingPreferencesTest::injectedStoreIsTheOnlyStoreUsed()
{
    settings().setValue(QStringLiteral("width"), 1600);
    settings().setValue(QStringLiteral("height"), 900);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();

    QCOMPARE(preferences->width, 1600);
    QCOMPARE(preferences->height, 900);

    preferences->width = 1920;
    preferences->height = 1080;
    preferences->save();

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(persisted.value(QStringLiteral("height")).toInt(), 1080);
}

void StreamingPreferencesTest::unknownHostFallsBackToTheFrozenTemplate()
{
    settings().setValue(QStringLiteral("width"), 1600);
    settings().setValue(QStringLiteral("height"), 900);
    settings().setValue(QStringLiteral("fps"), 30);
    settings().setValue(QStringLiteral("bitrate"), 12345);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();
    preferences->setCurrentHost(QStringLiteral("new-host"));

    QCOMPARE(preferences->width, 1600);
    QCOMPARE(preferences->height, 900);
    QCOMPARE(preferences->fps, 30);
    QCOMPARE(preferences->bitrateKbps, 12345);
}

void StreamingPreferencesTest::hostsRemainIsolatedWhenSwitchingAToBToA()
{
    settings().setValue(QStringLiteral("width"), 1280);
    settings().setValue(QStringLiteral("height"), 720);
    settings().setValue(QStringLiteral("fps"), 60);
    settings().setValue(QStringLiteral("bitrate"), 5000);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();

    preferences->setCurrentHost(QStringLiteral("host-a"));
    preferences->width = 1920;
    preferences->height = 1080;
    preferences->fps = 30;
    preferences->bitrateKbps = 11111;
    preferences->save();

    preferences->setCurrentHost(QStringLiteral("host-b"));
    QCOMPARE(preferences->width, 1280);
    QCOMPARE(preferences->height, 720);
    QCOMPARE(preferences->fps, 60);
    QCOMPARE(preferences->bitrateKbps, 5000);

    preferences->width = 2560;
    preferences->height = 1440;
    preferences->fps = 60;
    preferences->bitrateKbps = 22222;
    preferences->save();

    preferences->setCurrentHost(QStringLiteral("host-a"));
    QCOMPARE(preferences->width, 1920);
    QCOMPARE(preferences->height, 1080);
    QCOMPARE(preferences->fps, 30);
    QCOMPARE(preferences->bitrateKbps, 11111);
}

void StreamingPreferencesTest::migratesKnownHostsFromTheFlatTemplate()
{
    settings().setValue(QStringLiteral("width"), 2560);
    settings().setValue(QStringLiteral("height"), 1440);
    settings().setValue(QStringLiteral("fps"), 60);
    settings().setValue(QStringLiteral("bitrate"), 40000);
    settings().setValue(QStringLiteral("yuv444"), true);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 1);
    settings().beginWriteArray(QStringLiteral("hosts"));
    settings().setArrayIndex(0);
    settings().setValue(QStringLiteral("uuid"), QStringLiteral("known-host"));
    settings().endArray();
    settings().setValue(QStringLiteral("hostsettings/manual-host/autoadjustbitrate"), false);
    settings().setValue(QStringLiteral("hostsettings/manual-host/adaptivebitrate"), false);
    settings().setValue(QStringLiteral("hostsettings/recommended-host/autoadjustbitrate"), true);
    settings().setValue(QStringLiteral("hostsettings/recommended-host/adaptivebitrate"), false);

    loadPreferences();
    StreamingPreferences::resetForTesting();

    QSettings migrated(m_settingsPath, QSettings::IniFormat);
    migrated.setFallbacksEnabled(false);
    QCOMPARE(migrated.value(QStringLiteral("starlightPrefsVersion")).toInt(), 6);
    // Automatic bitrate becomes the mode for anyone who hadn't chosen a bitrate of their own.
    QVERIFY(migrated.value(QStringLiteral("hostsettings/recommended-host/adaptivebitrate")).toBool());
    QVERIFY(!migrated.value(QStringLiteral("hostsettings/manual-host/adaptivebitrate")).toBool());
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/known-host/width")).toInt(), 2560);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/known-host/height")).toInt(), 1440);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/known-host/fps")).toInt(), 60);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/known-host/bitrate")).toInt(), 40000);
    // 4:4:4 colour is no longer a choice; a fixed 2560×1440 makes the host Manual.
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/known-host/yuv444")).toBool(), false);
    QVERIFY(migrated.value(QStringLiteral("hostsettings/known-host/manualstream")).toBool());
}

void StreamingPreferencesTest::removedSettingsMigrateToAutomaticAndManualChoicesStay()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 3);
    auto store = [&](const QString& host, const QVariantMap& values) {
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            settings().setValue(QStringLiteral("hostsettings/") + host + '/' + it.key(), it.value());
    };
    const QVariantMap removed{{"vsync", false}, {"videodec", 2}, {"unlockbitrate", true},
                              {"framepacing", true}, {"yuv444", true}, {"hdr", true}, {"audiocfg", 2}};
    QVariantMap automatic = removed;
    automatic.insert({{"automaticresolution", true}, {"automaticframerate", true},
                      {"autoadjustbitrate", true}, {"videocfg", 0}, {"width", 2880}, {"height", 1800}, {"fps", 60}});
    store("automatic", automatic);
    store("manual", QVariantMap{{"automaticresolution", false}, {"automaticframerate", true},
                                {"width", 1920}, {"height", 1200}, {"fps", 60},
                                {"autoadjustbitrate", false}, {"bitrate", 400000}, {"unlockbitrate", true},
                                {"videocfg", 2}, {"vsync", false}});

    // An older version saved 60 fps as a fixed value and left the frame-rate flag out
    // entirely; that is still Automatic. (A saved size equal to the display's own counts
    // the same way, but this test has no screen to compare with.)
    store("saved-native", QVariantMap{{"automaticresolution", true}, {"width", 2880},
                                      {"height", 1800}, {"fps", 60}, {"bitrate", 53000}});

    auto* prefs = loadPreferences();
    prefs->setCurrentHost("saved-native");
    QVERIFY(!prefs->manualStream);
    prefs->setCurrentHost("automatic");
    QVERIFY(!prefs->manualStream);
    QVERIFY(prefs->enableVsync);
    QCOMPARE(prefs->videoDecoderSelection, StreamingPreferences::VDS_AUTO);
    QVERIFY(!prefs->unlockBitrate && !prefs->framePacing && !prefs->enableYUV444 && !prefs->enableHdr);
    QCOMPARE(prefs->audioConfig, StreamingPreferences::AC_STEREO);
    prefs->setCurrentHost("manual");
    QVERIFY(prefs->manualStream);
    QCOMPARE(prefs->width, 1920);
    QCOMPARE(prefs->height, 1200);
    QVERIFY(!prefs->autoAdjustBitrate);
    QCOMPARE(prefs->bitrateKbps, 150000); // The unlock is gone, so its range is too.
    QCOMPARE(prefs->videoCodecConfig, StreamingPreferences::VCC_FORCE_HEVC);
    QVERIFY(prefs->enableVsync);

    StreamingPreferences::resetForTesting();
    QSettings migrated(m_settingsPath, QSettings::IniFormat);
    migrated.setFallbacksEnabled(false);
    QCOMPARE(migrated.value(QStringLiteral("starlightPrefsVersion")).toInt(), 6);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/automatic/manualstream")).toBool(), false);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/automatic/vsync")).toBool(), true);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/automatic/videodec")).toInt(), 0);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/automatic/audiocfg")).toInt(), 0);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/manual/manualstream")).toBool(), true);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/manual/videocfg")).toInt(), 2);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/manual/width")).toInt(), 1920);
    // Only keys that were stored are rewritten.
    QVERIFY(!migrated.contains(QStringLiteral("hostsettings/manual/hdr")));
}

void StreamingPreferencesTest::presetsAndLastWorkingSettingsLoadSanitised()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("a", 2880, 1800, 60));
    prefs->save();
    QVERIFY(prefs->saveHostPreset("Old"));
    const QString id = prefs->hostPresets().first().toMap().value("id").toString();
    prefs->rememberSuccessfulSettings();
    QCOMPARE(prefs->activePresetName(), QString("Old"));

    // Records written by an older version: V-sync off, forced software decoding,
    // 4:4:4 and no Automatic/Manual field at all.
    QSettings external(m_settingsPath, QSettings::IniFormat);
    for (const QString& key : {QStringLiteral("hostsettings/a/presets/") + id + "/values",
                               QStringLiteral("hostsettings/a/lastsuccessful")}) {
        QVariantMap old = external.value(key).toMap();
        old.remove("manualstream");
        old.insert("vsync", false); old.insert("videodec", 2); old.insert("yuv444", true);
        old.insert("framepacing", true); old.insert("unlockbitrate", true);
        external.setValue(key, old);
    }
    external.sync();

    QCOMPARE(prefs->activePresetName(), QString("Old")); // Sanitised, it is the same setup.
    QVERIFY(prefs->applyHostPreset(id));
    QVERIFY(prefs->enableVsync && !prefs->enableYUV444 && !prefs->framePacing && !prefs->unlockBitrate);
    QCOMPARE(prefs->videoDecoderSelection, StreamingPreferences::VDS_AUTO);
    QVERIFY(!prefs->manualStream); // Automatic resolution, frame rate and bitrate.
    QVERIFY(prefs->bitrateKbps <= 150000);

    prefs->enableVsync = false; prefs->save();
    QVERIFY(prefs->restoreLastSuccessfulSettings());
    QVERIFY(prefs->enableVsync);
    QCOMPARE(prefs->videoDecoderSelection, StreamingPreferences::VDS_AUTO);
    auto retry = prefs->cloneWithLastSuccessfulSettings();
    QVERIFY(retry);
    QVERIFY(retry->enableVsync && !retry->enableYUV444);

    // A record that is malformed rather than old is still refused.
    external.setValue("hostsettings/a/lastsuccessful",
                      QVariantMap(external.value("hostsettings/a/lastsuccessful").toMap()).insert("videodec", 7).value());
    external.sync();
    QVERIFY(!prefs->restoreLastSuccessfulSettings());
}

void StreamingPreferencesTest::freshHostStartsAutomaticWhateverTheTemplateSays()
{
    settings().setValue(QStringLiteral("autoadjustbitrate"), false);
    settings().setValue(QStringLiteral("videocfg"), 2);
    settings().setValue(QStringLiteral("vsync"), false);
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("new", 2880, 1800, 60));
    QVERIFY(!prefs->manualStream);
    QVERIFY(prefs->autoAdjustBitrate && prefs->adaptiveBitrate && prefs->enableVsync);
    QCOMPARE(prefs->videoCodecConfig, StreamingPreferences::VCC_AUTO);
    prefs->setCurrentHost("other");
    prefs->setCurrentHost("new");
    QVERIFY(!prefs->manualStream);
    QVERIFY(prefs->autoAdjustBitrate);
}

void StreamingPreferencesTest::savesConnectionFieldsUnderTheSelectedHost()
{
    settings().setValue(QStringLiteral("width"), 1280);
    settings().setValue(QStringLiteral("height"), 720);
    settings().setValue(QStringLiteral("gameopts"), true);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();
    preferences->setCurrentHost(QStringLiteral("host-a"));
    preferences->width = 1920;
    preferences->height = 1080;
    preferences->gameOptimizations = false;
    preferences->save();

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/width")).toInt(), 1920);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/height")).toInt(), 1080);
    QCOMPARE(persisted.value(QStringLiteral("width")).toInt(), 1280);
    QCOMPARE(persisted.value(QStringLiteral("height")).toInt(), 720);
    QCOMPARE(persisted.value(QStringLiteral("gameopts")).toBool(), false);
    QVERIFY(!persisted.contains(QStringLiteral("hostsettings/host-a/gameopts")));
}

void StreamingPreferencesTest::explicitDisplayFlagsRecomputeBitrate()
{
    settings().setValue(QStringLiteral("width"), 1280);
    settings().setValue(QStringLiteral("height"), 720);
    settings().setValue(QStringLiteral("fps"), 60);
    settings().setValue(QStringLiteral("bitrate"), 5000);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();
    StreamCommandLineParser parser;
    parser.parse({ QStringLiteral("starlight"), QStringLiteral("stream"),
                   QStringLiteral("--resolution"), QStringLiteral("1920x1080"),
                   QStringLiteral("--fps"), QStringLiteral("30"),
                   QStringLiteral("host-a"), QStringLiteral("Desktop") }, preferences);

    QCOMPARE(preferences->width, 1920);
    QCOMPARE(preferences->height, 1080);
    QCOMPARE(preferences->fps, 30);
    QCOMPARE(preferences->bitrateKbps, 10000);
}

void StreamingPreferencesTest::cliOverridesWinAfterTheHostOverlay()
{
    settings().setValue(QStringLiteral("width"), 1280);
    settings().setValue(QStringLiteral("height"), 720);
    settings().setValue(QStringLiteral("fps"), 60);
    settings().setValue(QStringLiteral("bitrate"), 5000);
    settings().setValue(QStringLiteral("hostsettings/host-a/width"), 2560);
    settings().setValue(QStringLiteral("hostsettings/host-a/height"), 1440);
    settings().setValue(QStringLiteral("hostsettings/host-a/fps"), 60);
    settings().setValue(QStringLiteral("hostsettings/host-a/bitrate"), 40000);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();
    StreamCommandLineParser parser;
    parser.parse({ QStringLiteral("starlight"), QStringLiteral("stream"),
                   QStringLiteral("--resolution"), QStringLiteral("1920x1080"),
                   QStringLiteral("--fps"), QStringLiteral("30"),
                   QStringLiteral("host-a"), QStringLiteral("Desktop") }, preferences);

    preferences->setCurrentHost(QStringLiteral("host-a"));
    QCOMPARE(preferences->width, 2560);
    QCOMPARE(preferences->height, 1440);
    QCOMPARE(preferences->fps, 60);
    QCOMPARE(preferences->bitrateKbps, 40000);

    preferences->reapplyCliOverrides();
    QCOMPARE(preferences->width, 1920);
    QCOMPARE(preferences->height, 1080);
    QCOMPARE(preferences->fps, 30);
    QCOMPARE(preferences->bitrateKbps, 10000);
}

void StreamingPreferencesTest::workflowFacadeCommitsDisplayChangesExactlyOnce()
{
    settings().setValue(QStringLiteral("width"), 1280);
    settings().setValue(QStringLiteral("height"), 720);
    settings().setValue(QStringLiteral("fps"), 60);
    settings().setValue(QStringLiteral("bitrate"), 5000);
    settings().setValue(QStringLiteral("autoadjustbitrate"), true);
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();
    preferences->setCurrentHost(QStringLiteral("host-a"));

    QSignalSpy displayChanges(preferences, &StreamingPreferences::displayModeChanged);
    QSignalSpy bitrateChanges(preferences, &StreamingPreferences::bitrateChanged);
    QSignalSpy saves(preferences, &StreamingPreferences::saveCompleted);

    QVERIFY(preferences->setResolution(1920, 1080));
    QCOMPARE(displayChanges.count(), 1);
    QCOMPARE(bitrateChanges.count(), 1);
    QCOMPARE(saves.count(), 1);

    QVERIFY(!preferences->setResolution(1920, 1080));
    QCOMPARE(displayChanges.count(), 1);
    QCOMPARE(bitrateChanges.count(), 1);
    QCOMPARE(saves.count(), 1);

    QVERIFY(preferences->setFrameRate(30));
    QCOMPARE(displayChanges.count(), 2);
    QCOMPARE(bitrateChanges.count(), 2);
    QCOMPARE(saves.count(), 2);

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/width")).toInt(), 1920);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/height")).toInt(), 1080);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/fps")).toInt(), 30);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/host-a/bitrate")).toInt(), 10000);
    QCOMPARE(persisted.value(QStringLiteral("width")).toInt(), 1280);
    QCOMPARE(persisted.value(QStringLiteral("height")).toInt(), 720);
}

void StreamingPreferencesTest::freshHostSeedsNativeModeBeforeAnyOtherSave()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();

    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("fresh"), 2880, 1800, 60));
    QVERIFY(preferences->automaticFrameRate);
    QCOMPARE(preferences->width, 2880);
    QCOMPARE(preferences->height, 1800);
    QCOMPARE(preferences->fps, 60);

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/fresh/width")).toInt(), 2880);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/fresh/height")).toInt(), 1800);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/fresh/fps")).toInt(), 60);
}

void StreamingPreferencesTest::automaticResolutionFollowsDisplayAndManualChoiceStopsIt()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("fresh", 2880, 1800, 60));
    QVERIFY(prefs->automaticResolution);
    QVERIFY(prefs->automaticPointer);
    QVERIFY(prefs->absoluteMouseMode);
    QVERIFY(prefs->initializeHostDisplayMode("fresh", 2560, 1600, 60));
    QCOMPARE(prefs->width, 2560); QCOMPARE(prefs->height, 1600);
    QVERIFY(prefs->setResolution(2560, 1600)); // Same dimensions, deliberate manual choice.
    QVERIFY(!prefs->automaticResolution);
    QVERIFY(!prefs->initializeHostDisplayMode("fresh", 2880, 1800, 60));
    QCOMPARE(prefs->width, 2560);
    QVERIFY(prefs->setAutomaticResolution(2880, 1800));
    QVERIFY(prefs->automaticResolution);
    prefs->setCurrentHost("other");
    QVERIFY(!prefs->automaticResolution);
    prefs->setCurrentHost("fresh");
    QVERIFY(prefs->automaticResolution);
    auto trial = prefs->cloneForSession(true);
    QVERIFY(!trial->automaticResolution);
    QCOMPARE(trial->width, 1920);
}

void StreamingPreferencesTest::existingHostDisplayModeIsNeverOverwritten()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);
    settings().setValue(QStringLiteral("hostsettings/existing/width"), 1920);
    settings().setValue(QStringLiteral("hostsettings/existing/height"), 1080);
    settings().setValue(QStringLiteral("hostsettings/existing/fps"), 30);

    StreamingPreferences* preferences = loadPreferences();

    QVERIFY(!preferences->initializeHostDisplayMode(QStringLiteral("existing"), 2880, 1800, 60));
    QCOMPARE(preferences->width, 1920);
    QCOMPARE(preferences->height, 1080);
    QCOMPARE(preferences->fps, 30);

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/existing/width")).toInt(), 1920);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/existing/height")).toInt(), 1080);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/existing/fps")).toInt(), 30);
}

void StreamingPreferencesTest::partialStoredModePreservesEveryExplicitKey()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);
    settings().setValue(QStringLiteral("hostsettings/partial/width"), 1600);

    StreamingPreferences* preferences = loadPreferences();

    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("partial"), 2880, 1800, 75));
    QCOMPARE(preferences->width, 1600);
    QCOMPARE(preferences->fps, 75);

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/partial/width")).toInt(), 1600);
    QVERIFY(!persisted.contains(QStringLiteral("hostsettings/partial/height")));
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/partial/fps")).toInt(), 75);
}

void StreamingPreferencesTest::refreshRateFollowsDisplayAndManualChoiceStopsIt()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("fresh", 3840, 2160, 144));
    QVERIFY(prefs->automaticFrameRate);
    QVERIFY(prefs->initializeHostDisplayMode("fresh", 2880, 1800, 60));
    QCOMPARE(prefs->fps, 60);
    QVERIFY(prefs->setFrameRate(60)); // An explicit same-value choice is manual too.
    QVERIFY(!prefs->automaticFrameRate);
    QVERIFY(!prefs->initializeHostDisplayMode("fresh", 2880, 1800, 144));
    QCOMPARE(prefs->fps, 60);
    QVERIFY(prefs->setAutomaticFrameRate(144));
    QVERIFY(prefs->automaticFrameRate);
    QCOMPARE(prefs->fps, 144);
    prefs->setCurrentHost("manual-host");
    QVERIFY(!prefs->automaticFrameRate);
    prefs->setCurrentHost("fresh");
    QVERIFY(prefs->automaticFrameRate);
    QCOMPARE(prefs->fps, 144);
    auto trial = prefs->cloneForSession(true);
    QVERIFY(!trial->automaticFrameRate);
    QCOMPARE(trial->fps, 60);
    trial.reset();
    StreamingPreferences::resetForTesting();
    m_settings = std::make_unique<QSettings>(m_settingsPath, QSettings::IniFormat);
    m_settings->setFallbacksEnabled(false);
    prefs = loadPreferences();
    prefs->setCurrentHost("fresh");
    QVERIFY(prefs->automaticFrameRate);
    QVERIFY(prefs->initializeHostDisplayMode("fresh", 2880, 1800, 75));
    QCOMPARE(prefs->fps, 75);
}

void StreamingPreferencesTest::refreshRateMatchesFastAndSlowDisplaysAndFallsBackTo60()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);

    StreamingPreferences* preferences = loadPreferences();

    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("fast"), 2880, 1800, 240));
    QCOMPARE(preferences->fps, 240);
    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("slow"), 2880, 1800, 24));
    QCOMPARE(preferences->fps, 24);
    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("unknown-refresh"), 2880, 1800, 0));
    QCOMPARE(preferences->fps, 60);
}

void StreamingPreferencesTest::bitratePolicyBoundsEveryStoredAndRecommendedPath()
{
    settings().setValue(QStringLiteral("bitrate"), 900000);
    settings().setValue(QStringLiteral("hostsettings/high/bitrate"), 900000);
    auto p = loadPreferences();
    QCOMPARE(p->bitrateKbps, 150000);
    p->setCurrentHost(QStringLiteral("high"));
    QCOMPARE(p->bitrateKbps, 150000);
    p->enableYUV444 = true;
    p->autoAdjustBitrate = true;
    p->setResolution(3840, 2160);
    QCOMPARE(p->bitrateKbps, 150000);
    p->unlockBitrate = true;
    p->setFrameRate(9999);
    QCOMPARE(p->bitrateKbps, 500000);
    p->bitrateKbps = 900000;
    p->save();
    QCOMPARE(p->bitrateKbps, 500000);
    p->unlockBitrate = false;
    p->save();
    QCOMPARE(p->bitrateKbps, 150000);
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/high/bitrate")).toInt(), 150000);
}

void StreamingPreferencesTest::lastEnabledCaptureModeSurvivesReload()
{
    auto p = loadPreferences();
    for (const auto mode : {StreamingPreferences::CSK_ALWAYS, StreamingPreferences::CSK_FULLSCREEN}) {
        p->captureSysKeysMode = mode;
        p->save();
        p->captureSysKeysMode = StreamingPreferences::CSK_OFF;
        p->save();
        p->reload();
        QCOMPARE(p->captureSysKeysMode, StreamingPreferences::CSK_OFF);
        QCOMPARE(p->lastEnabledCaptureSysKeysMode, mode);
    }
}

void StreamingPreferencesTest::localCursorIsOnForNewHostsAndSavedChoicesStay()
{
    // A profile from before the change: the template still says "off", one
    // known host chose "off" and another chose "on".
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 2);
    settings().setValue(QStringLiteral("localcursor"), false);
    for (const auto& [host, cursor] : {std::pair{"off-host", false}, std::pair{"on-host", true}}) {
        const QString prefix = QStringLiteral("hostsettings/") + QString::fromLatin1(host) + QLatin1Char('/');
        settings().setValue(prefix + QStringLiteral("width"), 1920);
        settings().setValue(prefix + QStringLiteral("height"), 1080);
        settings().setValue(prefix + QStringLiteral("fps"), 30);
        settings().setValue(prefix + QStringLiteral("localcursor"), cursor);
    }

    StreamingPreferences* preferences = loadPreferences();
    QVERIFY(!preferences->initializeHostDisplayMode(QStringLiteral("off-host"), 2880, 1800, 60));
    QVERIFY(!preferences->localCursor);
    QVERIFY(!preferences->initializeHostDisplayMode(QStringLiteral("on-host"), 2880, 1800, 60));
    QVERIFY(preferences->localCursor);
    QVERIFY(preferences->initializeHostDisplayMode(QStringLiteral("new-host"), 2880, 1800, 60));
    QVERIFY(preferences->localCursor);
    QVERIFY(preferences->absoluteMouseMode);

    StreamingPreferences::resetForTesting();
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/off-host/localcursor")).toBool(), false);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/on-host/localcursor")).toBool(), true);
    QCOMPARE(persisted.value(QStringLiteral("hostsettings/new-host/localcursor")).toBool(), true);
    QCOMPARE(persisted.value(QStringLiteral("localcursor")).toBool(), false);

    // A fresh install has nothing stored at all.
    StreamingPreferences::resetForTesting();
    QTemporaryDir fresh;
    auto store = std::make_unique<QSettings>(fresh.filePath(QStringLiteral("fresh.ini")), QSettings::IniFormat);
    store->setFallbacksEnabled(false);
    QVERIFY(StreamingPreferences::getForTesting(std::move(store))->localCursor);
}

void StreamingPreferencesTest::displayAndInputMigrateToAutomaticUnlessChosen()
{
    settings().setValue(QStringLiteral("starlightPrefsVersion"), 4);
    auto store = [&](const QString& host, const QVariantMap& values) {
        QVariantMap record{{"desktopscale", "auto"}, {"viewscale", "fit"}, {"displaytarget", "host"},
                           {"automaticpointer", true}, {"mouseacceleration", true}, {"localcursor", true},
                           {"manualstream", false}};
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) record.insert(it.key(), it.value());
        for (auto it = record.constBegin(); it != record.constEnd(); ++it)
            settings().setValue(QStringLiteral("hostsettings/") + host + '/' + it.key(), it.value());
    };
    // "Use host setting" was every host's default; it is not a choice of the person's own.
    store("plain", {});
    store("scaled", {{"desktopscale", "2"}});
    store("actual", {{"viewscale", "actual"}});
    store("physical", {{"displaytarget", "physical"}});
    store("game", {{"automaticpointer", false}, {"mouseacceleration", false}});
    // A desktop pointer saved before Automatic existed, as the Mac host had.
    store("desktop-pointer", {{"automaticpointer", false}, {"mouseacceleration", true}});
    store("no-cursor", {{"localcursor", false}});

    auto* prefs = loadPreferences();
    const QHash<QString, QPair<bool, bool>> expected{
        {"plain", {false, false}}, {"scaled", {true, false}}, {"actual", {true, false}},
        {"physical", {true, false}}, {"game", {false, true}}, {"no-cursor", {false, true}},
        {"desktop-pointer", {false, false}}};
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        prefs->setCurrentHost(it.key());
        QCOMPARE(prefs->manualDisplay, it.value().first);
        QCOMPARE(prefs->manualInput, it.value().second);
    }
    // The values themselves are kept, for Manual.
    prefs->setCurrentHost("scaled");
    QCOMPARE(prefs->desktopScale, QString("2"));
    prefs->setCurrentHost("no-cursor");
    QVERIFY(!prefs->localCursor);

    StreamingPreferences::resetForTesting();
    QSettings migrated(m_settingsPath, QSettings::IniFormat);
    migrated.setFallbacksEnabled(false);
    QCOMPARE(migrated.value(QStringLiteral("starlightPrefsVersion")).toInt(), 6);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/plain/manualdisplay")).toBool(), false);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/plain/manualinput")).toBool(), false);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/physical/manualdisplay")).toBool(), true);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/game/manualinput")).toBool(), true);
    QCOMPARE(migrated.value(QStringLiteral("hostsettings/game/mouseacceleration")).toBool(), false);
}

void StreamingPreferencesTest::legacyDisplaySettingsDoNotSelectGamePointer_data()
{
    QTest::addColumn<int>("version");
    QTest::newRow("old-display-only") << 2;
    QTest::newRow("previously-migrated-display-only") << 5;
}

void StreamingPreferencesTest::legacyDisplaySettingsDoNotSelectGamePointer()
{
    QFETCH(int, version);
    settings().setValue("starlightPrefsVersion", version);
    settings().setValue("hostsettings/display-only/width", 1600);
    settings().setValue("hostsettings/display-only/height", 1000);
    if (version == 5) settings().setValue("hostsettings/display-only/manualinput", true);
    settings().setValue("hostsettings/explicit-game/mouseacceleration", false);
    if (version == 5) settings().setValue("hostsettings/explicit-game/manualinput", true);

    auto* prefs = loadPreferences();
    prefs->setCurrentHost("display-only");
    QCOMPARE(prefs->width, 1600);
    QVERIFY(!prefs->manualInput);
    QVERIFY(prefs->cloneForSession()->automaticPointer);
    prefs->setCurrentHost("explicit-game");
    QVERIFY(prefs->manualInput);
    const auto game = prefs->cloneForSession();
    QVERIFY(!game->automaticPointer);
    QVERIFY(!game->absoluteMouseMode);
}

void StreamingPreferencesTest::freshHostDisplayAndInputStartAutomatic()
{
    settings().setValue(QStringLiteral("desktopscale"), "2");
    settings().setValue(QStringLiteral("viewscale"), "actual");
    settings().setValue(QStringLiteral("localcursor"), false);
    settings().setValue(QStringLiteral("mouseacceleration"), false);
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("new", 2880, 1800, 60));
    QVERIFY(!prefs->manualDisplay);
    QVERIFY(!prefs->manualInput);
    QCOMPARE(prefs->desktopScale, QString("auto"));
    QCOMPARE(prefs->viewScale, QString("fit"));
    QCOMPARE(prefs->displayTarget, QString("auto"));
    QVERIFY(prefs->automaticPointer && prefs->localCursor);
    prefs->setCurrentHost("other");
    prefs->setCurrentHost("new");
    QVERIFY(!prefs->manualDisplay && !prefs->manualInput);
}

void StreamingPreferencesTest::automaticDisplayAndInputWinInSessionsAndManualValuesStay()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("a", 2880, 1800, 60));
    // Choices made under Manual, still saved after going back to Automatic.
    prefs->desktopScale = "2"; prefs->viewScale = "actual"; prefs->displayTarget = "physical";
    prefs->gameOptimizations = false; // Hidden legacy preference must not disable Automatic display setup.
    prefs->automaticPointer = false; prefs->absoluteMouseMode = false; prefs->localCursor = false;
    prefs->localTextInput = true;
    prefs->manualDisplay = false; prefs->manualInput = false;
    prefs->save();

    auto session = prefs->cloneForSession();
    QCOMPARE(session->desktopScale, QString("auto"));
    QCOMPARE(session->viewScale, QString("fit"));
    QCOMPARE(session->displayTarget, QString("auto"));
    QVERIFY(session->gameOptimizations);
    QVERIFY(!prefs->gameOptimizations); // Session policy does not rewrite the saved choice.
    QVERIFY(session->automaticPointer && session->localCursor);
    QVERIFY(session->localTextInput); // Not part of Automatic: a choice of the person's own.

    prefs->setCurrentHost("b");
    prefs->setCurrentHost("a");
    QCOMPARE(prefs->desktopScale, QString("2"));
    QCOMPARE(prefs->displayTarget, QString("physical"));
    QVERIFY(!prefs->automaticPointer && !prefs->localCursor);

    prefs->manualDisplay = true; prefs->manualInput = true;
    prefs->save();
    auto manual = prefs->cloneForSession();
    QCOMPARE(manual->desktopScale, QString("2"));
    QCOMPARE(manual->viewScale, QString("actual"));
    QCOMPARE(manual->displayTarget, QString("physical"));
    QVERIFY(!manual->gameOptimizations);
    QVERIFY(!manual->automaticPointer && !manual->absoluteMouseMode && !manual->localCursor);
}

void StreamingPreferencesTest::presetsAndLastWorkingSettingsDeriveDisplayAndInputModes()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("a", 2880, 1800, 60));
    prefs->save();
    QVERIFY(prefs->saveHostPreset("Old"));
    const QString id = prefs->hostPresets().first().toMap().value("id").toString();
    prefs->rememberSuccessfulSettings();

    // Written before Display and Input had Automatic and Manual.
    QSettings external(m_settingsPath, QSettings::IniFormat);
    const QString presetKey = QStringLiteral("hostsettings/a/presets/") + id + "/values";
    QVariantMap preset = external.value(presetKey).toMap();
    preset.remove("manualdisplay"); preset.remove("manualinput");
    preset.insert("viewscale", "actual");
    external.setValue(presetKey, preset);
    QVariantMap working = external.value("hostsettings/a/lastsuccessful").toMap();
    working.remove("manualdisplay"); working.remove("manualinput");
    working.insert("localcursor", false);
    external.setValue("hostsettings/a/lastsuccessful", working);
    external.sync();

    QVERIFY(prefs->applyHostPreset(id));
    QVERIFY(prefs->manualDisplay);
    QVERIFY(!prefs->manualInput);
    QCOMPARE(prefs->viewScale, QString("actual"));
    QVERIFY(prefs->restoreLastSuccessfulSettings());
    QVERIFY(!prefs->manualDisplay);
    QVERIFY(prefs->manualInput);
    QVERIFY(!prefs->localCursor);
}

void StreamingPreferencesTest::cliPointerIsAManualInputChoice()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("host-a", 2880, 1800, 60));
    prefs->save();
    StreamCommandLineParser parser;
    parser.parse({ QStringLiteral("starlight"), QStringLiteral("stream"), QStringLiteral("--no-absolute-mouse"),
                   QStringLiteral("host-a"), QStringLiteral("Desktop") }, prefs);
    prefs->setCurrentHost(QStringLiteral("host-a"));
    QVERIFY(!prefs->manualInput);
    prefs->reapplyCliOverrides();
    auto session = prefs->cloneForSession();
    QVERIFY(!session->automaticPointer);
    QVERIFY(!session->absoluteMouseMode);
}

void StreamingPreferencesTest::cliCanExplicitlyDisableFramePacing()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("host-a", 1920, 1080, 60));
    prefs->framePacing = true;
    prefs->save();
    QVERIFY(!prefs->cliFramePacingOverride().has_value());

    StreamCommandLineParser parser;
    parser.parse({"starlight", "stream", "--no-frame-pacing", "host-a", "Desktop"}, prefs);
    prefs->setCurrentHost(QStringLiteral("host-a"));
    const auto session = prefs->cloneForSession();
    QVERIFY(session->cliFramePacingOverride().has_value());
    QVERIFY(!session->cliFramePacingOverride().value());
    QVERIFY(!session->framePacing);
    QVERIFY(!prefs->cloneForSession(true)->cliFramePacingOverride().has_value());
}

void StreamingPreferencesTest::explicitCliChoicesSurviveAutomaticSessionPolicy()
{
    auto* prefs = loadPreferences();
    QVERIFY(prefs->initializeHostDisplayMode("host-a", 2880, 1800, 60));
    prefs->save();
    StreamCommandLineParser parser;
    parser.parse({"starlight", "stream", "--resolution", "1920x1080", "--fps", "30",
                  "--bitrate", "12000", "--no-game-optimization", "host-a", "Desktop"}, prefs);
    prefs->reapplyCliOverrides();
    QVERIFY(!prefs->manualStream && !prefs->manualDisplay);
    auto session = prefs->cloneForSession();
    QCOMPARE(session->width, 1920);
    QCOMPARE(session->height, 1080);
    QCOMPARE(session->fps, 30);
    QVERIFY(!session->automaticResolution && !session->automaticFrameRate);
    QCOMPARE(session->bitrateKbps, 12000);
    QVERIFY(!session->autoAdjustBitrate && !session->adaptiveBitrate);
    QVERIFY(!session->gameOptimizations);
    // A requested safe retry still supersedes the options of the failed start.
    auto safe = prefs->cloneForSession(true);
    QCOMPARE(safe->fps, 60);
    QCOMPARE(safe->bitrateKbps, 10000);
    QSettings persisted(m_settingsPath, QSettings::IniFormat);
    persisted.setFallbacksEnabled(false);
    QCOMPARE(persisted.value("hostsettings/host-a/fps").toInt(), 60);
}

void StreamingPreferencesTest::relearningOnlyClearsThisHostsMeasurements()
{
    auto& store = settings();
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("a");
    prefs->width = 2560; prefs->manualStream = true; prefs->save();
    store.setValue("pairing-proof", "preserve");
    store.setValue("hostcapacity/a/width", 1280);
    store.setValue("hostcapacity/b/width", 1920);
    store.setValue("automaticbitrate/a", 12000);
    store.setValue("automaticpacing/a", true);
    QVERIFY(prefs->resetLearnedPerformance());
    QVERIFY(!store.contains("hostcapacity/a/width"));
    QVERIFY(!store.contains("automaticbitrate/a"));
    QVERIFY(!store.contains("automaticpacing/a"));
    QCOMPARE(store.value("hostcapacity/b/width").toInt(), 1920);
    QCOMPARE(store.value("pairing-proof").toString(), QString("preserve"));
    QCOMPARE(prefs->width, 2560);
    QVERIFY(prefs->manualStream);
}

QTEST_GUILESS_MAIN(StreamingPreferencesTest)

#include "tst_streamingpreferences.moc"

void StreamingPreferencesTest::previewDoesNotReuseDecoderResultsFromAnotherMode()
{
    auto* prefs=loadPreferences();
    QSettings cached;
    cached.setValue("clientdecode/hevc",true);
    cached.setValue("clientdecode/context","a different screen or mode");
    cached.setValue("clientdecode/measuredAt",QDateTime::currentSecsSinceEpoch());
    QCOMPARE(prefs->streamPlan("Host",1920,1080,60,SCM_H264|SCM_HEVC).value("codec").toString(),QString("H.264"));
    cached.setValue("clientdecode/context",StreamPlanning::decoderContext("test-screen",1920,1080,60,1,int(prefs->videoDecoderSelection)));
    QCOMPARE(prefs->streamPlan("Host",1920,1080,60,SCM_H264|SCM_HEVC,0,0,0,0,QString(),1,false,QString(),"test-screen").value("codec").toString(),QString("HEVC"));
    QCOMPARE(prefs->streamPlan("Host",1920,1080,144,SCM_H264|SCM_HEVC,0,0,0,0,QString(),1,false,QString(),"test-screen").value("codec").toString(),QString("H.264"));
    cached.setValue("clientdecode/measuredAt",QDateTime::currentSecsSinceEpoch()-2*86400);
    QCOMPARE(prefs->streamPlan("Host",1920,1080,60,SCM_H264|SCM_HEVC,0,0,0,0,QString(),1,false,QString(),"test-screen").value("codec").toString(),QString("H.264"));
    cached.remove("clientdecode");
}

void StreamingPreferencesTest::displayTargetLookupPreservesSelectedHostAndEffectiveMode()
{
    auto* prefs = loadPreferences();
    prefs->setCurrentHost("physical");
    prefs->manualDisplay = true; prefs->displayTarget = "physical"; prefs->save();
    prefs->setCurrentHost("automatic");
    prefs->manualDisplay = false; prefs->displayTarget = "physical"; prefs->save();
    prefs->setCurrentHost("selected");
    prefs->manualDisplay = true; prefs->displayTarget = "virtual";
    QCOMPARE(prefs->displayTargetForHost("physical"), QString("physical"));
    QCOMPARE(prefs->displayTargetForHost("automatic"), QString("auto"));
    QCOMPARE(prefs->displayTargetForHost("selected"), QString("virtual"));
    QCOMPARE(prefs->displayTarget, QString("virtual"));
    prefs->manualDisplay = false;
    QCOMPARE(prefs->displayTargetForHost("selected"), QString("auto"));
    QCOMPARE(prefs->displayTargetForHost("new"), QString("auto"));
}
