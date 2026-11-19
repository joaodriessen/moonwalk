#pragma once
#include "presentationhealth.h"

#include <QSemaphore>
#include <QWindow>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QHash>
#include <memory>
#include <mutex>

#include <Limelight.h>
#include <opus_multistream.h>
#include "settings/streamingpreferences.h"
#include "input/input.h"
#include "video/decoder.h"
#include "audio/renderers/renderer.h"
#include "streamstatus.h"
#include "video/overlaymanager.h"
#include "cursormodepolicy.h"
#include "backend/hostcapabilities.h"
#include "automaticpacing.h"
#include "hostcapacity.h"
#include "hostlatencymeter.h"
#include "streamplan.h"
#include "streamrecommendation.h"

class ClipboardSync;
class CursorSync;
class RuntimeBitrate;
class AutomaticBitrateDriver;
class ConnectionDiagnostic;
class FileTransfer;

class SupportedVideoFormatList : public QList<int>
{
public:
    operator int() const
    {
        int value = 0;

        for (const int v : *this) {
            value |= v;
        }

        return value;
    }

    void
    removeByMask(int mask)
    {
        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                this->removeAt(i);
            }
            else {
                i++;
            }
        }
    }

    void
    deprioritizeByMask(int mask)
    {
        QList<int> deprioritizedList;

        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                deprioritizedList.append(this->takeAt(i));
            }
            else {
                i++;
            }
        }

        this->append(std::move(deprioritizedList));
    }

    int maskByServerCodecModes(int serverCodecModes)
    {
        int mask = 0;

        const QMap<int, int> mapping = {
            {SCM_H264, VIDEO_FORMAT_H264},
            {SCM_H264_HIGH8_444, VIDEO_FORMAT_H264_HIGH8_444},
            {SCM_HEVC, VIDEO_FORMAT_H265},
            {SCM_HEVC_MAIN10, VIDEO_FORMAT_H265_MAIN10},
            {SCM_HEVC_REXT8_444, VIDEO_FORMAT_H265_REXT8_444},
            {SCM_HEVC_REXT10_444, VIDEO_FORMAT_H265_REXT10_444},
            {SCM_AV1_MAIN8, VIDEO_FORMAT_AV1_MAIN8},
            {SCM_AV1_MAIN10, VIDEO_FORMAT_AV1_MAIN10},
            {SCM_AV1_HIGH8_444, VIDEO_FORMAT_AV1_HIGH8_444},
            {SCM_AV1_HIGH10_444, VIDEO_FORMAT_AV1_HIGH10_444},
        };

        for (QMap<int, int>::const_iterator it = mapping.cbegin(); it != mapping.cend(); ++it) {
            if (serverCodecModes & it.key()) {
                mask |= it.value();
                serverCodecModes &= ~it.key();
            }
        }

        // Hosts may advertise extra codecs such as PyroWave. Only intersect
        // formats implemented by this client; unknown host bits are harmless.

        int val = *this;
        return val & mask;
    }
};

class Session : public QObject
{
    Q_OBJECT

    friend class SdlInputHandler;
    friend class DeferredSessionCleanupTask;
    friend class AsyncConnectionStartThread;

public:
    explicit Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences = nullptr,
                     bool safeSettings = false, bool desktopRequest = false, bool desktopTakeover = false);
    virtual ~Session();

    Q_INVOKABLE bool initialize(QWindow* qtWindow);
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancelPendingStart();
    Q_INVOKABLE void interrupt();
    Q_INVOKABLE void cancelConnectivityTest();
    Q_INVOKABLE Session* createRetrySession(bool safeSettings = false, bool resumeOnly = true,
                                          bool manualRetry = false, bool lastSuccessful = false);
    Q_INVOKABLE bool hasLastSuccessfulSettings() const;
    Q_INVOKABLE bool keepSafeSettings();
    Q_INVOKABLE void retainForRetry() { m_RetryRetained = true; }
    Q_INVOKABLE void releaseRetryRetention();
    Q_INVOKABLE qint64 stableVideoMilliseconds() const {
        return m_StableVideoMs.load();
    }
    Q_PROPERTY(bool connectionWasEstablished READ connectionWasEstablished NOTIFY connectionStarted)
    Q_PROPERTY(bool retryableTermination READ retryableTermination NOTIFY retryStateChanged)
    // True only after the user explicitly applies a stream recommendation.
    Q_PROPERTY(bool adjustingForHost READ adjustingForHost NOTIFY retryStateChanged)
    Q_PROPERTY(bool cleanupComplete READ cleanupComplete NOTIFY cleanupCompleteChanged)
    Q_PROPERTY(bool safeSettingsTrial READ safeSettingsTrial CONSTANT)
    Q_PROPERTY(bool hasVideoFrame READ hasVideoFrame NOTIFY videoFramePresented)
    bool connectionWasEstablished() const { return m_ConnectionWasEstablished.load(); }
    bool adjustingForHost() const { return m_AdjustingForHost.load(); }
    bool retryableTermination() const { return m_RetryableTermination.load() && !m_StartCancellation.isCancelled(); }
    bool cleanupComplete() const { return m_CleanupComplete; }
    bool safeSettingsTrial() const { return m_SafeSettingsTrial; }
    bool hasVideoFrame() const { return m_HasVideoFrame.load() && (!m_CaptureHealthSupported || sourceHealthy()); }
    bool sourceHealthy() const { return m_SourceHealthy.load() && Uint32(SDL_GetTicks()-m_SourceHealthAt.load())<=5000; }
    static void notifyFramePresented();
    // Each decoded frame on its way to the screen, from the decoder's thread.
    static void notifyFrameArrived(quint64 arrivalUs, bool windowVisible);
    // Round trip, presented frames and network state, read while the panel is open.
    Q_INVOKABLE QVariantMap liveStats() const;
    Q_INVOKABLE void openControls();
    Q_INVOKABLE void prepareControlsClipboard();
    Q_INVOKABLE void closeControls();
    Q_INVOKABLE void controlAction(QString action, QVariant value = QVariant());
    Q_PROPERTY(bool controlsOpen READ controlsOpen NOTIFY controlsChanged)
    Q_PROPERTY(QVariantMap controlState READ controlState NOTIFY controlsChanged)
    bool controlsOpen() const { return m_ControlsOpen; }
    QVariantMap controlState() const;
    Q_PROPERTY(QObject* fileTransfer READ fileTransfer NOTIFY transferChanged)
    QObject* fileTransfer() const;
    Q_PROPERTY(QStringList launchWarnings MEMBER m_LaunchWarnings NOTIFY launchWarningsChanged);
    Q_PROPERTY(int connectivityTestResult READ connectivityTestResult NOTIFY connectionDiagnosticReady)
    int connectivityTestResult() const { return m_PortTestResults.load(); }

    // hasHwHevcOrAv1Decode is set true iff a FORCE_HARDWARE HEVC or AV1 probe
    // below succeeded (HEVC Main10, AV1 Main10, or plain HEVC) -- i.e.
    // anything better than the H.264 fallback. It is used by
    // SystemProperties::hasHwHevcOrAv1Decode as a proxy for "modern decode
    // silicon" (design-native-first.md V.1) and is false whenever only H.264
    // worked, or nothing worked at all.
    static
    bool getDecoderInfo(SDL_Window* window,
                        bool& isHardwareAccelerated, bool& isFullScreenOnly,
                        bool& isHdrSupported, QSize& maxResolution,
                        bool& hasHwHevcOrAv1Decode);

    static Session* get()
    {
        return s_ActiveSession;
    }

    Overlay::OverlayManager& getOverlayManager()
    {
        return m_OverlayManager;
    }

    void flushWindowEvents();

    void setShouldExit(bool quitHostApp = false);

signals:
    void stageStarting(QString stage);

    void stageFailed(QString stage, int errorCode, QString failingPorts);

    void connectionStarted();

    void displayLaunchError(QString text);

    void quitStarting();

    void sessionFinished(int portTestResult);

    // Emitted after sessionFinished() when the session is ready to be destroyed
    void readyForDeletion();
    void hostCleanupFailed(QString message);

    void launchWarningsChanged();
    void connectionDiagnosticReady(int result);
    void retryStateChanged();
    void cleanupCompleteChanged();
    void videoFramePresented();
    void controlsChanged();
    void transferChanged();
    void fileDropRequested(QUrl file);
    void filesDropRequested(QVariantList files);

private:
    bool initializeInternal(QWindow* qtWindow);
    void startWhenReady();
    void exec();
    void updateLocalTextInput();

    bool startConnectionAsync();

    bool validateLaunch(SDL_Window* testWindow);

    void emitLaunchWarning(QString text);
    void refreshStreamStatus();
    void requestConnectivityTest(unsigned int portFlags);
    void connectionCleanupComplete();
    QString endHostTarget();

    bool populateDecoderProperties(SDL_Window* window);

    IAudioRenderer* createAudioRenderer(const POPUS_MULTISTREAM_CONFIGURATION opusConfig);

    bool initializeAudioRenderer();

    bool testAudio(int audioConfiguration);

    int getAudioRendererCapabilities(int audioConfiguration);

    void getWindowDimensions(int& x, int& y,
                             int& width, int& height);

    void toggleFullscreen();

    void notifyMouseEmulationMode(bool enabled);
    void notifyMouseModeChanged(bool absolute);
    void warnCursorReconnectRequired();

    void updateOptimalWindowDisplayMode();

    // Picks size, frame rate, codec and bitrate for this display and host.
    void planStream(SDL_Window* testWindow);
    // What the host sustained, kept for planning its next stream.
    // early: a few seconds are enough, for a host that plainly couldn't keep up.
    void rememberStreamMeasurements(bool early = false);
    HostCapacity streamMeasurement(const HostLatencyMeter& latency) const;
    bool capacityNetworkClean() const;
    std::atomic<Uint32> m_BitrateAppliedAt {0};
public:
    // Every frame the decoder receives, pushed or pulled: what the host spent on it.
    void noteDecodedSize(int width, int height) {
        m_DecodedWidth = width; m_DecodedHeight = height;
    }
    void noteVideoFrame(const DECODE_UNIT* du)
    {
        std::lock_guard<std::mutex> lock(m_MeasurementLock);
        m_HostLatency.addFrame(du->receiveTimeUs, du->frameHostProcessingLatency, du->fullLength, quint32(du->frameNumber));
        m_RecentHostLatency.addFrame(du->receiveTimeUs, du->frameHostProcessingLatency, du->fullLength, quint32(du->frameNumber));
    }
private:
    bool framePacingEnabled(bool enableVsync) const;
    void rememberPacing();

    enum class DecoderAvailability {
        None,
        Software,
        Hardware
    };

    void traceStartup(const char* phase) const;
    QElapsedTimer m_StartupTimer;

    DecoderAvailability getDecoderAvailability(SDL_Window* window,
                                               StreamingPreferences::VideoDecoderSelection vds,
                                               int videoFormat, int width, int height, int frameRate);

    static
    bool chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                       SDL_Window* window, int videoFormat, int width, int height,
                       int frameRate, bool enableVsync, bool enableFramePacing,
                       bool testOnly,
                       IVideoDecoder*& chosenDecoder);

    static
    void clStageStarting(int stage);

    static
    void clStageFailed(int stage, int errorCode);

    static
    void clConnectionTerminated(int errorCode);

    static
    void clLogMessage(const char* format, ...);

    static
    void clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

    static
    void clConnectionStatusUpdate(int connectionStatus);

    static
    void clSetHdrMode(bool enabled);

    static
    void clRumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger);

    static
    void clSetMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz);

    static
    void clSetControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b);

    static
    void clSetAdaptiveTriggers(uint16_t controllerNumber, uint8_t eventFlags, uint8_t typeLeft, uint8_t typeRight, uint8_t *left, uint8_t *right);

    static
    int arInit(int audioConfiguration,
               const POPUS_MULTISTREAM_CONFIGURATION opusConfig,
               void* arContext, int arFlags);

    static
    void arCleanup();

    static
    void arDecodeAndPlaySample(char* sampleData, int sampleLength);

    static
    int drSetup(int videoFormat, int width, int height, int frameRate, void*, int);

    static
    void drCleanup();

    static
    int drSubmitDecodeUnit(PDECODE_UNIT du);

    std::unique_ptr<StreamingPreferences> m_OwnedPreferences;
    StreamingPreferences* m_Preferences;
    bool m_SafeSettingsTrial = false;
    bool m_SafeSettingsKept = false;
    bool m_ResumeOnly = false;
    std::atomic<bool> m_ConnectionWasEstablished {false};
    std::atomic<bool> m_RetryableTermination {false};
    std::atomic<bool> m_AdjustingForHost {false};
    bool m_HostTargetMayExist = false;
    bool m_PlannedSizeAutomatically = false;
    StreamAdaptation m_Adaptation;
    StreamRecommendation m_Recommendation;
    bool m_ConnectionRecovery = false;
    int m_RecoveryBitrateKbps = 0;
    StreamPlanning::Rung m_RecoveryMode {0, 0, 0};
    bool m_RejectTrialMeasurement = false;
    QVector<StreamPlanning::Rung> recommendationModes(int fps) const;
    void recommendStreamChange(StreamPlanning::Rung target, const HostCapacity& baseline, bool rollback = false, int bitrateKbps = 0);
    QString m_BitrateCostContext;
    QString m_PerformanceHostContext, m_ViewingContext;
    std::atomic<int> m_DecodedWidth {0}, m_DecodedHeight {0};
    std::atomic<bool> m_HasVideoFrame {false};
    std::atomic<Uint32> m_FirstFrameAt {0};
    std::atomic<Uint32> m_LastFrameAt {0};
    std::atomic<quint64> m_PresentedFrames {0};
    std::atomic<Uint32> m_StableVideoMs {0};
    PresentationHealth m_PresentationHealth;
    bool m_CaptureHealthSupported = false;
    quint64 m_CaptureBaseline = 0;
    bool m_CaptureBaselineActive = false;
    std::atomic<bool> m_SourceHealthy {false};
    std::atomic<double> m_CapturedFps {0};
    std::atomic<Uint32> m_SourceHealthAt {0};
    std::atomic<bool> m_RemoteInputAvailable {true};
    bool m_SuccessfulSettingsRemembered = false;
    bool m_FirstFrameNotified = false;
    int m_NegotiatedEncryption = -1;
    bool m_RetryRetained = false;
    bool m_DeletionReady = false;
    bool m_ControlsOpen = false;
    bool m_ToolbarAction = false;
    QString m_ControlsClipboardNotice;
    QString m_ResolvedDesktopScale = QStringLiteral("auto");
    bool m_CaptureBeforeControls = false;
    bool m_HaveCaptureOverride = false;
    bool m_DesiredCapture = false;
    bool m_ManualAudioMuted = false;
    bool m_ClipboardPaused = false;
    int m_ClipboardDirection = 1;
    bool m_ImageClipboardSupported = false;
    HostCapabilities m_HostCapabilities;
    RuntimeBitrate* m_RuntimeBitrate = nullptr;
    AutomaticBitrateDriver* m_AutomaticBitrate = nullptr;
    std::atomic<int> m_AutomaticBitrateKbps {0};
    qint64 m_RuntimeRequestedAt = 0;
    bool m_RuntimeRequestRaised = false;
    std::atomic<bool> m_RuntimeBitrateSupported {false};
    std::atomic<int> m_RuntimeBitrateKbps {0};
    int m_NormalStartupBitrateKbps = 0;
    bool m_StartupBitrateKnown = false;
    // How a mid-stream bitrate change lands on this host, as last measured.
    std::atomic<HostCapacity::BitrateChange> m_BitrateChange {HostCapacity::BitrateChange::Unknown};
    bool m_BitrateRebuilt = false;
    int m_BitrateChangeStallMs = 0;
    QString m_RuntimeBitrateNotice;
    std::atomic<bool> m_NetworkPoor {false};
    bool m_FileTransferSupported = false;
    FileTransfer* m_FileTransfer = nullptr;
    bool m_SystemKeysRequested = false;
    QString m_CompositionText;
    QString m_RenderedCompositionText;
    QString m_TextInputNotice;
    Uint32 m_TextInputNoticeAt = 0;
    NvAddress m_PreferredAddress;
    quint64 m_RouteGeneration = 0;
    bool m_IsFullScreen;
    SupportedVideoFormatList m_SupportedVideoFormats; // Sorted in order of descending priority
    STREAM_CONFIGURATION m_StreamConfig;
    DECODER_RENDERER_CALLBACKS m_VideoCallbacks;
    AUDIO_RENDERER_CALLBACKS m_AudioCallbacks;
    NvComputer* m_Computer;
    QSharedPointer<NvComputer> m_ComputerOwner;
    bool m_StartRequested = false;
    bool m_ConnectionStartInProgress = false; // GUI thread only
    NvHTTP::CancelToken m_StartCancellation;
    NvApp m_App;
    bool m_IsDesktopRequest = false;
    bool m_DesktopTakeoverApproved = false;
    SDL_Window* m_Window;
    IVideoDecoder* m_VideoDecoder;
    SDL_mutex* m_DecoderLock;
    bool m_AudioDisabled;
    std::atomic<bool> m_AudioMuted;
    Uint32 m_FullScreenFlag;
    bool m_WindowNeedsPlacement = false;
    Uint32 m_RefreshTitleAt = 0;
    qreal m_LocalDisplayScale = 1;
    QWindow* m_QtWindow;
    bool m_UnexpectedTermination;
    SdlInputHandler* m_InputHandler;
    // In-stream native clipboard sync (app/streaming/clipboardsync.h). Constructed just
    // before entering the SDL loop in exec() and destroyed at DispatchDeferredCleanup;
    // nullptr the rest of the time (including for the entire duration of any failed/
    // aborted connection that never reaches the SDL loop).
    ClipboardSync* m_ClipboardSync;
    CursorSync* m_CursorSync;
    CursorModePolicy m_CursorPolicy;
    bool m_CursorReconnectWarningShown = false;
    int m_MouseEmulationRefCount;
    int m_FlushingWindowEventsRef;
    QStringList m_LaunchWarnings;
    bool m_ShouldExit;

    bool m_AsyncConnectionSuccess;
    std::atomic<int> m_PortTestResults;
    ConnectionDiagnostic* m_ConnectionDiagnostic = nullptr; // GUI thread only
    bool m_CleanupComplete = false; // GUI thread only
    quint64 m_DiagnosticGeneration = 0;

    StreamPlan m_StreamPlan;
    QSize m_HostDesktopSize;
    QString m_PerformanceContext, m_BitrateContext, m_StartupBitrateContext;
    // Probing a decoder is slow, and its answer doesn't change during a launch.
    QHash<QString, DecoderAvailability> m_DecoderProbes;
    int m_DisplayHdr = -1; // -1 until a 10-bit decoder says whether this display shows HDR
    mutable std::mutex m_MeasurementLock;
    HostLatencyMeter m_HostLatency;
    HostLatencyMeter m_RecentHostLatency;
    HostLatencyMeter m_LastQualifiedHostLatency;
    Uint32 m_LastQualifiedHostAt = 0;
    enum class PacingMode { Off, On, Automatic };
    PacingMode m_PacingMode = PacingMode::Automatic;
    AutomaticPacing m_AutomaticPacing;
    std::atomic<bool> m_PacingActive {false};

    int m_ActiveVideoFormat;
    int m_ActiveVideoWidth;
    int m_ActiveVideoHeight;
    int m_ActiveVideoFrameRate;

    OpusMSDecoder* m_OpusDecoder;
    IAudioRenderer* m_AudioRenderer;
    OPUS_MULTISTREAM_CONFIGURATION m_ActiveAudioConfig;
    OPUS_MULTISTREAM_CONFIGURATION m_OriginalAudioConfig;
    int m_AudioSampleCount;
    Uint32 m_DropAudioEndTime;

    Overlay::OverlayManager m_OverlayManager;
    StreamStatus m_StreamStatus;
    StreamStatus::Snapshot m_RenderedStreamStatus;

    static CONNECTION_LISTENER_CALLBACKS k_ConnCallbacks;
    static Session* s_ActiveSession;
    static QSemaphore s_ActiveSessionSemaphore;
};
