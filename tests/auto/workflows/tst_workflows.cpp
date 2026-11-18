#include <QtTest>
#include "../../../app/backend/nvapp.h"
#include "../../../app/backend/hostcapabilities.h"
#include "../../../app/backend/hostsoftware.h"

#include "../../../app/application/controllerintent.h"
#include "../../../app/application/hostworkflow.h"
#include "../../../app/application/languageworkflow.h"
#include "../../../app/application/pollinglifecycle.h"
#include "../../../app/application/preferencesworkflow.h"
#include "../../../app/backend/displaysnapshot.h"

class RecordingLanguageState final : public LanguageWorkflow::StatePort
{
public:
    RecordingLanguageState(LanguageWorkflow& workflow, int language)
        : workflow(workflow),
          language(language)
    {
    }

    int currentLanguage() const override
    {
        return language;
    }

    void commitLanguage(int newLanguage) override
    {
        translationWasLoadedAtCommit = workflow.translationLoaded();
        language = newLanguage;
        ++commitCount;
    }

    LanguageWorkflow& workflow;
    int language;
    int commitCount = 0;
    bool translationWasLoadedAtCommit = false;
};

class WorkflowsTest : public QObject
{
    Q_OBJECT

private slots:
    void runningTargetUsesUuidAcrossNumericAliasesAndRejectsReusedIds() {
        NvApp app;app.id=101;app.uuid="abcdefab-1234-4321-abcd-0123456789ab";
        QVERIFY(app.matchesRunningTarget(202,app.uuid));
        QVERIFY(!app.matchesRunningTarget(101,"abcdefab-9999-4321-abcd-0123456789ab"));
        QVERIFY(!app.matchesRunningTarget(0,app.uuid));
        QVERIFY(app.matchesRunningTarget(101));
        QVERIFY(!app.matchesRunningTarget(202));
    }

    void foreignDesktopDetectionUsesExplicitIdentitiesAndRejectsGamesOrDuplicates() {
        NvApp desktop; desktop.id=101;desktop.name="Desktop";desktop.uuid="abcdefab-1234-4321-abcd-0123456789ab";
        NvApp running;running.id=202;running.name="     Desktop";running.uuid="9a1c5a25-58fe-40e0-b9aa-7d3f00000007";
        QVERIFY(NvApp::hasForeignDesktop({desktop,running}));
        running.name="Game";QVERIFY(!NvApp::hasForeignDesktop({desktop,running}));
        running.name="Desktop";running.uuid.clear();QVERIFY(!NvApp::hasForeignDesktop({desktop,running}));
        running.uuid="9a1c5a25-58fe-40e0-b9aa-7d3f00000007";
        QVERIFY(!NvApp::hasForeignDesktop({desktop,desktop,running}));
        QVERIFY(!NvApp::hasForeignDesktop({desktop,running,running}));
    }

    void desktopSelectionRespectsHostRolesAndLegacyFallback() {
        NvApp desktop; desktop.id = 101; desktop.name = "Desktop";
        NvApp monitor; monitor.id = 2147483515; monitor.name = "    Remote Monitor";
        monitor.uuid = "9a1c5a25-58fe-40e0-b9aa-7d3f00000005";
        QCOMPARE(NvApp::desktopTarget({desktop}, true).id, 101);
        QCOMPARE(NvApp::desktopTarget({desktop, monitor}, false).id, 101);
        QCOMPARE(NvApp::desktopTarget({desktop, monitor}, true).id, monitor.id);
        QCOMPARE(NvApp::mainDesktopTarget({desktop, monitor}).id, desktop.id);
        NvApp resume; resume.id = 2147483501; resume.name = "Resume";
        resume.uuid = "9a1c5a25-58fe-40e0-b9aa-7d3f00000001";
        NvApp disconnect; disconnect.id = 2147483502; disconnect.name = "Disconnect Monitor";
        disconnect.uuid = "9a1c5a25-58fe-40e0-b9aa-7d3f00000002";
        QVERIFY(NvApp::desktopTarget({resume, disconnect}, true).isRemoteMonitor());
        QVERIFY(!NvApp::desktopTarget({resume}, true).isInitialized());
        monitor.uuid.clear();
        QCOMPARE(NvApp::desktopTarget({desktop, monitor}, true).id, 101);
        QVERIFY(!NvApp::desktopTarget({resume, disconnect}, false).isInitialized());
        QVERIFY(NvApp::ownsRemoteMonitor({resume, disconnect}));
        QVERIFY(!NvApp::ownsRemoteMonitor({resume}));
        QVERIFY(NvApp::desktopLaunchTarget({resume, disconnect}, false).isRemoteMonitor());
        QVERIFY(NvApp::mainDesktopTarget({resume, disconnect}).isRemoteMonitor());
        QCOMPARE(NvApp::desktopLaunchTarget({desktop, monitor}, false).id, desktop.id);
    }

    void desktopTargetDistinguishesRunningAppDuplicate() {
        NvApp actual; actual.id = 101; actual.name = "     Desktop";
        actual.uuid = "f59d4ba4-883c-4940-a72c-b01d513bdcb5";
        NvApp resume; resume.id = 202; resume.name = "Desktop";
        resume.uuid = "9a1c5a25-58fe-40e0-b9aa-7d3f00000007";
        QVERIFY(actual.isDesktopApp());
        QVERIFY(!resume.isDesktopApp());
        QVERIFY(!resume.isDesktopTarget());
        resume.uuid = "{9A1C5A25-58FE-40E0-B9AA-7D3F00000007}";
        QVERIFY(!resume.isDesktopApp());
        QTemporaryDir dir;
        QSettings settings(dir.filePath("apps.ini"), QSettings::IniFormat);
        resume.serialize(settings);
        NvApp restored(settings);
        QCOMPARE(restored, resume);
        QVERIFY(!restored.isDesktopApp());
        NvApp changed = actual; changed.uuid = resume.uuid;
        QVERIFY(changed != actual);
        NvApp legacy; legacy.name = "Desktop";
        QVERIFY(legacy.isDesktopApp());
        legacy.name = "Steam Big Picture";
        QVERIFY(!legacy.isDesktopApp());
    }

    void hostCapabilitiesUsePairedPermissionsAndAcceptBooleanForms() {
        auto caps = HostCapabilities::parse("<root><PairStatus>0</PairStatus><Permission>0</Permission><VirtualDisplayCapable>true</VirtualDisplayCapable><VirtualDisplayDriverReady>1</VirtualDisplayDriverReady></root>");
        QVERIFY(caps.apollo); QVERIFY(caps.virtualDisplay); QVERIFY(caps.virtualDisplayReady);
        QCOMPARE(caps.displayRequest("auto", true), 1);
        QCOMPARE(caps.displayRequest("auto", false), -1);
        QCOMPARE(caps.displayRequest("host", true), -1);
        QCOMPARE(caps.displayRequest("physical", true), 0);
        caps.virtualDisplayReady = false;
        QCOMPARE(caps.displayRequest("auto", true), -1);
        QCOMPARE(caps.permissions, qint64(-1));
        QVERIFY(caps.allows(HostCapabilities::ClipboardRead));
        caps = HostCapabilities::parse("<root><PairStatus>1</PairStatus><Permission>131072</Permission></root>");
        QVERIFY(caps.allows(HostCapabilities::ClipboardRead));
        QVERIFY(!caps.allows(HostCapabilities::ClipboardWrite));
        QVERIFY(!caps.allows(HostCapabilities::Launch));
        caps = HostCapabilities::parse("<root><PairStatus>1</PairStatus><Permission>4294967296</Permission></root>");
        QCOMPARE(caps.permissions, qint64(-1));
        QVERIFY(!HostCapabilities::parse("<root><Permission>0</Permission>").apollo);
    }
    void hostSoftwareNamesTheProductAndOnlyARealVersion_data() {
        QTest::addColumn<QString>("xml");
        QTest::addColumn<QString>("label");
        // The family's shared GFE-compatible constants, as every one of them sends.
        const QString gfeCompat = "<appversion>7.1.431.-1</appversion><GfeVersion>3.23.0.74</GfeVersion>";
        QTest::newRow("moonwalk-host") << "<root><hostname>Studio Mac</hostname>" + gfeCompat +
            "<SkylightVersion>0.4.0-alpha.15</SkylightVersion><SkylightSource>fd7687e1</SkylightSource><HostOs>macos</HostOs>"
            "<state>SUNSHINE_SERVER_BUSY</state></root>" << "Moonwalk Host (0.4.0-alpha.15)";
        QTest::newRow("moonwalk-host-without-version") << "<root><HostOs>macos</HostOs><state>SUNSHINE_SERVER_FREE</state></root>"
            << "Moonwalk Host";
        QTest::newRow("sunshine") << "<root>" + gfeCompat + "<PairStatus>0</PairStatus><state>SUNSHINE_SERVER_FREE</state></root>"
            << "Sunshine";
        QTest::newRow("apollo-unpaired") << "<root>" + gfeCompat + "<Permission>0</Permission><state>SUNSHINE_SERVER_FREE</state></root>"
            << "Apollo";
        QTest::newRow("apollo-paired") << "<root>" + gfeCompat + "<Permission>201457663</Permission><VirtualDisplayCapable>true</VirtualDisplayCapable>"
            "<PairStatus>1</PairStatus><currentgameuuid></currentgameuuid><state>SUNSHINE_SERVER_FREE</state></root>" << "Apollo";
        QTest::newRow("vibeshine") << "<root>" + gfeCompat + "<HardwareMetricsVersion>0</HardwareMetricsVersion>"
            "<FrameLimiterSupported>0</FrameLimiterSupported><state>SUNSHINE_SERVER_FREE</state></root>" << "Vibeshine";
        QTest::newRow("vibepollo") << "<root>" + gfeCompat + "<HardwareMetricsVersion>1</HardwareMetricsVersion>"
            "<Permission>0</Permission><state>SUNSHINE_SERVER_BUSY</state></root>" << "Vibepollo";
        // Vibepollo 2.0.0-2.0.1 has no HardwareMetricsVersion yet: the fields the
        // Mini PC and Gaming PC hosts send.
        QTest::newRow("vibepollo-2.0.1") << "<root><hostname>Mini PC</hostname>" + gfeCompat + "<HttpsPort>47984</HttpsPort>"
            "<FrameLimiterSupported>1</FrameLimiterSupported><FrameLimiterEnabled>1</FrameLimiterEnabled>"
            "<VirtualDisplayFrameLimiterEnabled>1</VirtualDisplayFrameLimiterEnabled><FrameLimiterFpsLimitMilliHz>0</FrameLimiterFpsLimitMilliHz>"
            "<VirtualDisplayCapable>true</VirtualDisplayCapable><VirtualDisplayDriverReady>true</VirtualDisplayDriverReady>"
            "<VirtualDisplayHDRCapable>true</VirtualDisplayHDRCapable><Permission>0</Permission>"
            "<ServerCodecModeSupport>262913</ServerCodecModeSupport><state>SUNSHINE_SERVER_FREE</state></root>" << "Vibepollo";
        QTest::newRow("geforce-experience") << "<root><appversion>7.1.431.-1</appversion><GfeVersion>3.28.0.417</GfeVersion>"
            "<state>MJOLNIR_STATE_SERVER_AVAILABLE</state></root>" << "GeForce Experience (3.28.0.417)";
        QTest::newRow("unrecognised-os-tag-is-not-moonwalk") << "<root><HostOs>beos</HostOs><state>SUNSHINE_SERVER_FREE</state></root>"
            << "Sunshine";
        QTest::newRow("nothing-recognisable") << "<root><appversion>1</appversion><state>UNKNOWN</state></root>" << "";
        QTest::newRow("broken-xml") << "<root><state>SUNSHINE_SERVER_FREE</state>" << "";
    }
    void hostSoftwareNamesTheProductAndOnlyARealVersion() {
        QFETCH(QString, xml);
        QFETCH(QString, label);
        const HostSoftware software = HostSoftware::identify(xml);
        QCOMPARE(HostSoftware::label(software.product, software.version), label);
        // Sunshine-family hosts never get the GFE-compatible constant as a version.
        if (software.product != QLatin1String("GeForce Experience"))
            QVERIFY(software.version != QLatin1String("3.23.0.74"));
    }
    void hostWorkflowQueuesAttemptsInIssueOrder();
    void hostWorkflowSuppressesCanceledAndSupersededResults();
    void hostWorkflowMatchesOutOfOrderCompletionById();
    void hostWorkflowSuppressesCanceledCurrentFailure();
    void hostWorkflowSurfacesCurrentFailure();
    void hostWorkflowIgnoresCompletionWithoutPendingAttempt();

    void pollingLifecycleStartsOnceForVisibleActiveWindow();
    void pollingLifecycleDefersStopWhileInactive();
    void pollingLifecycleStopsAfterInactiveTimeout();
    void pollingLifecycleStopsImmediatelyWhenHidden();
    void pollingLifecycleRejectsTimeoutWhileActive();
    void pollingLifecycleKeepsTimerStoppedWhenHiddenAndInactive();

    void displaySnapshotReplacesRatesAndRemovedScreens();
    void displayIdentityMatchesIndependentEnumeration();
    void preferencesWorkflowBoundsRecommendations();
    void preferencesWorkflowAppliesResolutionAsOneTransaction();
    void preferencesWorkflowAppliesFrameRateAsOneTransaction();
    void preferencesWorkflowPreservesManualBitrate();
    void preferencesWorkflowRejectsUnchangedAndInvalidCommands();

    void languageWorkflowPlansDynamicChangeAndClearBackRecovery();
    void languageWorkflowRejectsUnchangedAndInvalidCommands();
    void languageWorkflowClearsPreviousTranslationWhenCatalogIsMissing();

    void controllerIntentMapsButtonsWithoutToolkitKeys();
    void controllerIntentMapsAxesWithVerticalPriority();
    void controllerIntentRejectsNeutralAndUnknownInput();
};

void WorkflowsTest::hostWorkflowQueuesAttemptsInIssueOrder()
{
    HostWorkflow workflow;

    QCOMPARE(workflow.beginPairingAttempt(0), 0);
    QCOMPARE(workflow.beginPairingAttempt(1), 1);
    QCOMPARE(workflow.currentPairingAttemptId(), 1);
    QCOMPARE(workflow.pendingPairingAttempts(), QVariantList({ 0, 1 }));
}

void WorkflowsTest::hostWorkflowSuppressesCanceledAndSupersededResults()
{
    HostWorkflow workflow;

    QCOMPARE(workflow.beginPairingAttempt(0), 0);
    workflow.cancelCurrentPairingAttempt();
    QCOMPARE(workflow.beginPairingAttempt(1), 1);

    const QVariantMap stale = workflow.completePairingAttempt(0, QStringLiteral("late failure"));
    QCOMPARE(stale.value(QStringLiteral("attemptId")).toInt(), 0);
    QCOMPARE(stale.value(QStringLiteral("closeCurrent")).toBool(), false);
    QCOMPARE(stale.value(QStringLiteral("showError")).toBool(), false);
    QCOMPARE(workflow.pendingPairingAttempts(), QVariantList({ 1 }));
    QCOMPARE(workflow.currentPairingAttemptId(), 1);
}

void WorkflowsTest::hostWorkflowMatchesOutOfOrderCompletionById()
{
    HostWorkflow workflow;
    QCOMPARE(workflow.beginPairingAttempt(41), 41);
    workflow.cancelCurrentPairingAttempt();
    QCOMPARE(workflow.beginPairingAttempt(42), 42);

    const QVariantMap current = workflow.completePairingAttempt(42, QStringLiteral("retry failed"));
    QCOMPARE(current.value(QStringLiteral("attemptId")).toInt(), 42);
    QCOMPARE(current.value(QStringLiteral("closeCurrent")).toBool(), true);
    QCOMPARE(current.value(QStringLiteral("showError")).toBool(), true);
    QCOMPARE(current.value(QStringLiteral("error")).toString(), QStringLiteral("retry failed"));

    const QVariantMap stale = workflow.completePairingAttempt(41, QStringLiteral("old failure"));
    QCOMPARE(stale.value(QStringLiteral("closeCurrent")).toBool(), false);
    QVERIFY(workflow.pendingPairingAttempts().isEmpty());
}

void WorkflowsTest::hostWorkflowSuppressesCanceledCurrentFailure()
{
    HostWorkflow workflow;

    QCOMPARE(workflow.beginPairingAttempt(0), 0);
    workflow.cancelCurrentPairingAttempt();

    const QVariantMap completion =
        workflow.completePairingAttempt(0, QStringLiteral("canceled failure"));
    QCOMPARE(completion.value(QStringLiteral("attemptId")).toInt(), 0);
    QCOMPARE(completion.value(QStringLiteral("closeCurrent")).toBool(), true);
    QCOMPARE(completion.value(QStringLiteral("showError")).toBool(), false);
    QCOMPARE(workflow.currentPairingAttemptId(), -1);
}

void WorkflowsTest::hostWorkflowSurfacesCurrentFailure()
{
    HostWorkflow workflow;

    QCOMPARE(workflow.beginPairingAttempt(0), 0);

    const QVariantMap completion =
            workflow.completePairingAttempt(0, QStringLiteral("pairing failed"));
    QCOMPARE(completion.value(QStringLiteral("attemptId")).toInt(), 0);
    QCOMPARE(completion.value(QStringLiteral("closeCurrent")).toBool(), true);
    QCOMPARE(completion.value(QStringLiteral("showError")).toBool(), true);
    QCOMPARE(completion.value(QStringLiteral("error")).toString(),
             QStringLiteral("pairing failed"));
    QCOMPARE(workflow.currentPairingAttemptId(), -1);
}

void WorkflowsTest::hostWorkflowIgnoresCompletionWithoutPendingAttempt()
{
    HostWorkflow workflow;

    workflow.cancelCurrentPairingAttempt();
    const QVariantMap completion = workflow.completePairingAttempt(0, QVariant());

    QCOMPARE(completion.value(QStringLiteral("attemptId")).toInt(), -1);
    QCOMPARE(completion.value(QStringLiteral("closeCurrent")).toBool(), false);
    QCOMPARE(completion.value(QStringLiteral("showError")).toBool(), false);
    QCOMPARE(workflow.currentPairingAttemptId(), -1);
    QVERIFY(workflow.pendingPairingAttempts().isEmpty());
}

void WorkflowsTest::pollingLifecycleStartsOnceForVisibleActiveWindow()
{
    PollingLifecycle lifecycle;
    QSignalSpy stateChanged(&lifecycle, &PollingLifecycle::pollingActiveChanged);

    const QVariantMap first = lifecycle.visibilityChanged(true, true);
    QVERIFY(first.value(QStringLiteral("startPolling")).toBool());
    QVERIFY(first.value(QStringLiteral("stopInactivityTimer")).toBool());
    QVERIFY(first.value(QStringLiteral("hasFocus")).toBool());
    QVERIFY(!lifecycle.pollingActive());
    QCOMPARE(stateChanged.count(), 0);

    lifecycle.commitPollingState(true);
    QVERIFY(lifecycle.pollingActive());
    QCOMPARE(stateChanged.count(), 1);

    const QVariantMap repeated = lifecycle.activityChanged(true, true);
    QVERIFY(!repeated.value(QStringLiteral("startPolling")).toBool());
    QVERIFY(!repeated.value(QStringLiteral("stopPolling")).toBool());
    QVERIFY(repeated.value(QStringLiteral("stopInactivityTimer")).toBool());
    QVERIFY(lifecycle.pollingActive());
}

void WorkflowsTest::pollingLifecycleDefersStopWhileInactive()
{
    PollingLifecycle lifecycle;
    lifecycle.visibilityChanged(true, true);
    lifecycle.commitPollingState(true);

    const QVariantMap decision = lifecycle.activityChanged(false, true);
    QVERIFY(decision.value(QStringLiteral("startInactivityTimer")).toBool());
    QVERIFY(!decision.value(QStringLiteral("stopPolling")).toBool());
    QVERIFY(!decision.value(QStringLiteral("hasFocus")).toBool());
    QVERIFY(lifecycle.pollingActive());
}

void WorkflowsTest::pollingLifecycleStopsAfterInactiveTimeout()
{
    PollingLifecycle lifecycle;
    lifecycle.visibilityChanged(true, true);
    lifecycle.commitPollingState(true);
    lifecycle.activityChanged(false, true);

    const QVariantMap decision = lifecycle.inactivityExpired(false);
    QVERIFY(decision.value(QStringLiteral("stopPolling")).toBool());
    QVERIFY(lifecycle.pollingActive());

    lifecycle.commitPollingState(false);
    QVERIFY(!lifecycle.pollingActive());
}

void WorkflowsTest::pollingLifecycleStopsImmediatelyWhenHidden()
{
    PollingLifecycle lifecycle;
    lifecycle.visibilityChanged(true, true);
    lifecycle.commitPollingState(true);

    const QVariantMap decision = lifecycle.visibilityChanged(false, true);
    QVERIFY(decision.value(QStringLiteral("stopPolling")).toBool());
    QVERIFY(decision.value(QStringLiteral("stopInactivityTimer")).toBool());
    QVERIFY(!decision.value(QStringLiteral("hasFocus")).toBool());
    QVERIFY(lifecycle.pollingActive());

    lifecycle.commitPollingState(false);
    QVERIFY(!lifecycle.pollingActive());
}

void WorkflowsTest::pollingLifecycleRejectsTimeoutWhileActive()
{
    PollingLifecycle lifecycle;
    lifecycle.visibilityChanged(true, true);
    lifecycle.commitPollingState(true);

    const QVariantMap decision = lifecycle.inactivityExpired(true);
    QVERIFY(!decision.value(QStringLiteral("stopPolling")).toBool());
    QVERIFY(lifecycle.pollingActive());
}

void WorkflowsTest::pollingLifecycleKeepsTimerStoppedWhenHiddenAndInactive()
{
    PollingLifecycle lifecycle;

    const QVariantMap decision = lifecycle.activityChanged(false, false);
    QVERIFY(!decision.value(QStringLiteral("startInactivityTimer")).toBool());
    QVERIFY(decision.value(QStringLiteral("stopInactivityTimer")).toBool());
    QVERIFY(!decision.value(QStringLiteral("hasFocus")).toBool());
}

void WorkflowsTest::displaySnapshotReplacesRatesAndRemovedScreens()
{
    auto snapshot = DisplaySnapshot::capture(2, [](int i) {
        return DisplayMetadata {QString::number(i), {}, {0, 0, 1920, 1080}, {0, 0, 1900, 1060}, i ? 120 : 60};
    });
    snapshot = DisplaySnapshot::capture(1, [](int) {
        return DisplayMetadata {"remaining", {}, {0, 0, 2560, 1440}, {0, 0, 2500, 1400}, 75};
    });
    QCOMPARE(snapshot.displays.size(), 1);
    QCOMPARE(snapshot.displays.first().refreshRate, 75);
    QCOMPARE(snapshot.displays.first().safeAreaResolution.size(), QSize(2500, 1400));
    snapshot = DisplaySnapshot::capture(2, [](int) { return DisplayMetadata {}; });
    QCOMPARE(snapshot.displays.size(), 2);
    QCOMPARE(snapshot.displays.at(1).refreshRate, 0);
    QVERIFY(snapshot.displays.first().nativeResolution.isNull());
    snapshot = DisplaySnapshot::capture(-1, [](int) { return DisplayMetadata {}; });
    QVERIFY(snapshot.displays.isEmpty());
}

void WorkflowsTest::displayIdentityMatchesIndependentEnumeration()
{
    DisplaySnapshot snapshot;
    snapshot.displays = {{"right", {1920, 0, 2560, 1440}, {}, {}, 120},
                         {"left", {0, 0, 1920, 1080}, {}, {}, 60}};
    QCOMPARE(snapshot.indexForScreen("left", {0, 0, 1920, 1080}), 1);
    QCOMPARE(snapshot.indexForScreen("Qt-name", {1920, 0, 2560, 1440}), 0);
    QCOMPARE(snapshot.indexForScreen("missing", {0, 0, 1, 1}), -1);
    snapshot.displays[0].name = "same";
    snapshot.displays[1].name = "same";
    QCOMPARE(snapshot.indexForScreen("same", {0, 0, 1920, 1080}), 1);
    QCOMPARE(snapshot.indexForScreen("same", {0, 0, 1, 1}), -1);

    // Wayland SDL2-compat uses a human-readable name and physical bounds,
    // while Qt exposes the connector name and logical screen geometry.
    snapshot.displays = {{"Built-in display", {0, 0, 2880, 1800}, {}, {}, 60}};
    QCOMPARE(snapshot.indexForScreen("eDP-1", {0, 0, 1440, 900}, 2), 0);
    snapshot.displays.append(snapshot.displays.first());
    QCOMPARE(snapshot.indexForScreen("eDP-1", {0, 0, 1440, 900}, 2), -1);
}

void WorkflowsTest::preferencesWorkflowBoundsRecommendations()
{
    QCOMPARE(PreferencesWorkflow::effectiveBitrate(160000, false), 150000);
    QCOMPARE(PreferencesWorkflow::effectiveBitrate(900000, true), 500000);
    PreferencesWorkflow::State state {{1920, 1080}, 60, 20000, true, true};
    QCOMPARE(PreferencesWorkflow::changeResolution(state, {3840, 2160}).state.bitrateKbps, 150000);
    state.unlockBitrate = true;
    state.frameRate = 9999;
    QCOMPARE(PreferencesWorkflow::changeResolution(state, {8192, 8192}).state.bitrateKbps, 500000);
}

void WorkflowsTest::preferencesWorkflowAppliesResolutionAsOneTransaction()
{
    const PreferencesWorkflow::State initial {
        { 1920, 1080 },
        30,
        10000,
        true,
        true,
    };

    const PreferencesWorkflow::Transaction transaction =
        PreferencesWorkflow::changeResolution(initial, { 2560, 1440 });

    QCOMPARE(transaction.status, PreferencesWorkflow::Status::Applied);
    QCOMPARE(transaction.state.resolution, QSize(2560, 1440));
    QCOMPARE(transaction.state.frameRate, 30);
    QCOMPARE(transaction.state.bitrateKbps, 40000);
    QVERIFY(transaction.saveRequired());
}

void WorkflowsTest::preferencesWorkflowAppliesFrameRateAsOneTransaction()
{
    const PreferencesWorkflow::State initial {
        { 1920, 1080 },
        30,
        10000,
        true,
        false,
    };

    const PreferencesWorkflow::Transaction transaction =
        PreferencesWorkflow::changeFrameRate(initial, 60);

    QCOMPARE(transaction.status, PreferencesWorkflow::Status::Applied);
    QCOMPARE(transaction.state.resolution, QSize(1920, 1080));
    QCOMPARE(transaction.state.frameRate, 60);
    QCOMPARE(transaction.state.bitrateKbps, 20000);
    QVERIFY(transaction.saveRequired());
}

void WorkflowsTest::preferencesWorkflowPreservesManualBitrate()
{
    const PreferencesWorkflow::State initial {
        { 1920, 1080 },
        60,
        47500,
        false,
        false,
    };

    const PreferencesWorkflow::Transaction transaction =
        PreferencesWorkflow::changeResolution(initial, { 3840, 2160 });

    QCOMPARE(transaction.status, PreferencesWorkflow::Status::Applied);
    QCOMPARE(transaction.state.bitrateKbps, 47500);
    QVERIFY(transaction.saveRequired());
}

void WorkflowsTest::preferencesWorkflowRejectsUnchangedAndInvalidCommands()
{
    const PreferencesWorkflow::State initial {
        { 1920, 1080 },
        60,
        20000,
        true,
        false,
    };

    const PreferencesWorkflow::Transaction unchangedResolution =
        PreferencesWorkflow::changeResolution(initial, { 1920, 1080 });
    QCOMPARE(unchangedResolution.status, PreferencesWorkflow::Status::Unchanged);
    QVERIFY(!unchangedResolution.saveRequired());

    const PreferencesWorkflow::Transaction invalidResolution =
        PreferencesWorkflow::changeResolution(initial, { 0, 1080 });
    QCOMPARE(invalidResolution.status, PreferencesWorkflow::Status::InvalidInput);
    QCOMPARE(invalidResolution.state.resolution, initial.resolution);
    QVERIFY(!invalidResolution.saveRequired());

    const PreferencesWorkflow::Transaction invalidFrameRate =
        PreferencesWorkflow::changeFrameRate(initial, -1);
    QCOMPARE(invalidFrameRate.status, PreferencesWorkflow::Status::InvalidInput);
    QCOMPARE(invalidFrameRate.state.frameRate, initial.frameRate);
    QVERIFY(!invalidFrameRate.saveRequired());
}

void WorkflowsTest::languageWorkflowPlansDynamicChangeAndClearBackRecovery()
{
    LanguageWorkflow workflow;
    RecordingLanguageState state(workflow, LanguageWorkflow::English);

    const LanguageWorkflow::Decision decision =
        workflow.changeLanguage(
            state,
            LanguageWorkflow::German,
            true);

    QCOMPARE(decision.status, LanguageWorkflow::Status::ApplyNow);
    QCOMPARE(decision.language, LanguageWorkflow::German);
    QVERIFY(decision.retranslate);
    QVERIFY(decision.clearOnBack);
    QVERIFY(!decision.showRestartNotice);
    QCOMPARE(state.language, LanguageWorkflow::German);
    QCOMPARE(state.commitCount, 1);
    QVERIFY(!state.translationWasLoadedAtCommit);
    QVERIFY(workflow.translationLoaded());

    state.translationWasLoadedAtCommit = false;
    const LanguageWorkflow::Decision deferred =
        workflow.changeLanguage(
            state,
            LanguageWorkflow::French,
            false);
    QCOMPARE(deferred.status, LanguageWorkflow::Status::ApplyAfterRestart);
    QVERIFY(!deferred.retranslate);
    QVERIFY(!deferred.clearOnBack);
    QVERIFY(deferred.showRestartNotice);
    QCOMPARE(state.language, LanguageWorkflow::French);
    QCOMPARE(state.commitCount, 2);
    QVERIFY(state.translationWasLoadedAtCommit);
}

void WorkflowsTest::languageWorkflowRejectsUnchangedAndInvalidCommands()
{
    const LanguageWorkflow::Decision unchanged =
        LanguageWorkflow::planChange(
            LanguageWorkflow::German,
            LanguageWorkflow::German,
            true);
    QCOMPARE(unchanged.status, LanguageWorkflow::Status::Unchanged);
    QVERIFY(!unchanged.retranslate);
    QVERIFY(!unchanged.clearOnBack);

    const LanguageWorkflow::Decision invalid =
        LanguageWorkflow::planChange(
            LanguageWorkflow::German,
            LanguageWorkflow::LastLanguage + 1,
            true);
    QCOMPARE(invalid.status, LanguageWorkflow::Status::InvalidInput);
    QCOMPARE(invalid.language, LanguageWorkflow::German);
    QVERIFY(!invalid.retranslate);
}

void WorkflowsTest::languageWorkflowClearsPreviousTranslationWhenCatalogIsMissing()
{
    LanguageWorkflow workflow;
    RecordingLanguageState state(workflow, LanguageWorkflow::English);

    const LanguageWorkflow::Decision german = workflow.changeLanguage(
        state,
        LanguageWorkflow::German,
        true);
    QCOMPARE(german.status, LanguageWorkflow::Status::ApplyNow);
    QVERIFY(workflow.translationLoaded());

    // English intentionally has no catalog. Moving to it must first remove the
    // old German translator so source strings recover to their English text.
    const LanguageWorkflow::Decision english = workflow.changeLanguage(
        state,
        LanguageWorkflow::English,
        true);
    QCOMPARE(english.status, LanguageWorkflow::Status::ApplyNow);
    QVERIFY(state.translationWasLoadedAtCommit);
    QVERIFY(!workflow.translationLoaded());
}

void WorkflowsTest::controllerIntentMapsButtonsWithoutToolkitKeys()
{
    const auto up =
        ControllerIntent::fromButton(SDL_CONTROLLER_BUTTON_DPAD_UP);
    QVERIFY(up.has_value());
    QCOMPARE(up->action, ControllerIntent::Action::NavigateUp);

    const auto confirm =
        ControllerIntent::fromButton(SDL_CONTROLLER_BUTTON_A);
    QVERIFY(confirm.has_value());
    QCOMPARE(confirm->action, ControllerIntent::Action::Confirm);

    const auto back =
        ControllerIntent::fromButton(SDL_CONTROLLER_BUTTON_B);
    QVERIFY(back.has_value());
    QCOMPARE(back->action, ControllerIntent::Action::Back);

    const auto context =
        ControllerIntent::fromButton(SDL_CONTROLLER_BUTTON_X);
    QVERIFY(context.has_value());
    QCOMPARE(context->action, ControllerIntent::Action::OpenContextMenu);

    const auto settings =
        ControllerIntent::fromButton(SDL_CONTROLLER_BUTTON_START);
    QVERIFY(settings.has_value());
    QCOMPARE(settings->action, ControllerIntent::Action::OpenSettings);
}

void WorkflowsTest::controllerIntentMapsAxesWithVerticalPriority()
{
    const auto diagonal = ControllerIntent::fromAxis(32000, -32000);
    QVERIFY(diagonal.has_value());
    QCOMPARE(diagonal->action, ControllerIntent::Action::NavigateUp);

    const auto left = ControllerIntent::fromAxis(-30001, 0);
    QVERIFY(left.has_value());
    QCOMPARE(left->action, ControllerIntent::Action::NavigateLeft);
}

void WorkflowsTest::controllerIntentRejectsNeutralAndUnknownInput()
{
    QVERIFY(!ControllerIntent::fromAxis(30000, 30000).has_value());
    QVERIFY(!ControllerIntent::fromButton(
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER).has_value());
    QVERIFY(!ControllerIntent::fromButton(0xff).has_value());
}

QTEST_GUILESS_MAIN(WorkflowsTest)

#include "tst_workflows.moc"
