#include "session.h"
#include "backend/audiolayoutpolicy.h"
#include "settings/streamingpreferences.h"
#include "streaming/streamutils.h"
#include "streaming/clipboardsync.h"
#include "streaming/runtimebitrate.h"
#include "streaming/automaticbitratedriver.h"
#include <QDateTime>
#include <utility>
#include "streaming/clipboardfocusguard.h"
#include "streaming/cursorsync.h"
#include "streaming/cursorsizepolicy.h"
#include "streaming/connectiondiagnostic.h"
#include "streaming/sessionretrypolicy.h"
#include "streaming/desktopscalepolicy.h"
#include "streaming/clientclipboardimage.h"
#include "streaming/nativeclipboard.h"
#include "streaming/filetransfer.h"
#include "streaming/streamtoolbar.h"
#include "backend/displaysnapshot.h"
#include "streaming/viewscalepolicy.h"
#include "streaming/windowgeometrypolicy.h"
#include "streaming/windowdecoration.h"
#include "backend/richpresencemanager.h"
#include "application/preferencesworkflow.h"

#include <Limelight.h>
#include "../../moonlight-common-c/adaptivefeedback.h"
#include <PlatformSockets.h>
#include "SDL_compat.h"
#include "utils.h"

#ifdef HAVE_FFMPEG
#include "video/ffmpeg.h"
#endif

#ifdef HAVE_SLVIDEO
#include "video/slvid.h"
#endif

#ifdef Q_OS_WIN32
// Scaling the icon down on Win32 looks dreadful, so render at lower res
#define ICON_SIZE 32
#else
#define ICON_SIZE 64
#endif

#define SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER 100
#define SDL_CODE_GAMECONTROLLER_RUMBLE 101
#define SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS 102
#define SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE 103
#define SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED 104
#define SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS 105

#include <openssl/rand.h>

#include <QtEndian>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QThreadPool>
#include <QImage>
#include <QGuiApplication>
#include <QCursor>
#include <QScreen>
#include <QHostAddress>
#include <QtGlobal>
#include <QPointer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQuickOpenGLUtils>
#endif

#define CONN_TEST_SERVER "qt.conntest.moonlight-stream.org"

// Optional diagnostics outlive network teardown, but never a newer attempt.
// These two values are accessed only on the GUI thread.
static QPointer<ConnectionDiagnostic> s_CurrentDiagnostic;
static quint64 s_DiagnosticGeneration = 0;

CONNECTION_LISTENER_CALLBACKS Session::k_ConnCallbacks = {
    Session::clStageStarting,
    nullptr,
    Session::clStageFailed,
    nullptr,
    Session::clConnectionTerminated,
    Session::clLogMessage,
    Session::clRumble,
    Session::clConnectionStatusUpdate,
    Session::clSetHdrMode,
    Session::clRumbleTriggers,
    Session::clSetMotionEventState,
    Session::clSetControllerLED,
    Session::clSetAdaptiveTriggers
};

Session* Session::s_ActiveSession;
QSemaphore Session::s_ActiveSessionSemaphore(1);

void Session::clStageStarting(int stage)
{
    // LiStartConnection resets its interrupt flag before the first callback.
    // Reassert a cancellation that raced that reset.
    if (s_ActiveSession->m_StartCancellation.isCancelled()) {
        LiInterruptConnection();
        return;
    }
    s_ActiveSession->traceStartup(LiGetStageName(stage));
    emit s_ActiveSession->stageStarting(QString::fromLocal8Bit(LiGetStageName(stage)));
}

void Session::clStageFailed(int stage, int errorCode)
{
    if (s_ActiveSession->m_StartCancellation.isCancelled()) return;
    s_ActiveSession->m_RetryableTermination = s_ActiveSession->m_ResumeOnly && SessionRetryPolicy::retryStage(stage);
    emit s_ActiveSession->retryStateChanged();
    unsigned int portFlags = LiGetPortFlagsFromStage(stage);

    char failingPorts[128];
    LiStringifyPortFlags(portFlags, ", ", failingPorts, sizeof(failingPorts));
    emit s_ActiveSession->stageFailed(QString::fromLocal8Bit(LiGetStageName(stage)), errorCode, QString(failingPorts));
    s_ActiveSession->requestConnectivityTest(portFlags);
}

void Session::clConnectionTerminated(int errorCode)
{
    s_ActiveSession->m_RetryableTermination = SessionRetryPolicy::retryTermination(
                errorCode, s_ActiveSession->m_ConnectionWasEstablished.load());
    emit s_ActiveSession->retryStateChanged();
    unsigned int portFlags = LiGetPortFlagsFromTerminationErrorCode(errorCode);

    // Display the termination dialog if this was not intended
    switch (errorCode) {
    case ML_ERROR_GRACEFUL_TERMINATION:
        break;

    case ML_ERROR_NO_VIDEO_TRAFFIC:
        s_ActiveSession->m_UnexpectedTermination = true;

        char ports[128];
        SDL_assert(portFlags != 0);
        LiStringifyPortFlags(portFlags, ", ", ports, sizeof(ports));
        emit s_ActiveSession->displayLaunchError(tr("No video arrived from the host.") + "\n\n" +
                                                 tr("Check the host's streaming logs and whether the network allows traffic on %1.").arg(ports));
        break;

    case ML_ERROR_NO_VIDEO_FRAME:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("Video data arrived, but Moonwalk couldn't assemble a complete frame.") + "\n\n" +
                                                 tr("Try a more stable connection or lower the bitrate before reconnecting."));
        break;

    case ML_ERROR_PROTECTED_CONTENT:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("The host stopped the stream after detecting protected content.") + "\n\n" +
                                                 tr("Close the protected content on the host, then reconnect."));
        break;

    case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("The host ended the stream before Moonwalk received a picture.") + "\n\n" +
                                                 tr("Check the host's streaming logs for the reason."));
        break;

    case ML_ERROR_FRAME_CONVERSION:
        s_ActiveSession->m_UnexpectedTermination = true;
        emit s_ActiveSession->displayLaunchError(tr("The host reported a fatal video encoding error.") + "\n\n" +
                                                 tr("Try disabling HDR mode, changing the streaming resolution, or changing the host's display resolution."));
        break;

    default:
        s_ActiveSession->m_UnexpectedTermination = true;

        // We'll assume large errors are hex values
        bool hexError = qAbs(errorCode) > 1000;
        emit s_ActiveSession->displayLaunchError(tr("Connection terminated") + "\n\n" +
                                                 tr("Error code: %1").arg(errorCode, hexError ? 8 : 0, hexError ? 16 : 10, QChar('0')));
        break;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Connection terminated: %d",
                 errorCode);

    // Push a quit event to the main loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    s_ActiveSession->requestConnectivityTest(portFlags);
    SDL_PushEvent(&event);
}

void Session::requestConnectivityTest(unsigned int portFlags)
{
    if (!portFlags || !m_Preferences->detectNetworkBlocking) return;
    // Callback callers still own their active Session until teardown finishes.
    // Queue onto that context; Qt drops this start if its owner is destroyed.
    QMetaObject::invokeMethod(this, [this, portFlags] {
        if (m_DiagnosticGeneration != s_DiagnosticGeneration || m_StartCancellation.isCancelled()) return;
        if (m_ConnectionDiagnostic) return;
        QVector<ConnectionDiagnostic::Port> ports;
        for (int i = 0; i < 32; ++i) {
            if (portFlags & (1U << i)) {
                ports.append({1U << i, LiGetPortFromPortFlagIndex(i),
                              LiGetProtocolFromPortFlagIndex(i) == IPPROTO_UDP});
            }
        }
        auto diagnostic = new ConnectionDiagnostic(this);
        m_ConnectionDiagnostic = diagnostic;
        s_CurrentDiagnostic = diagnostic;
        connect(diagnostic, &ConnectionDiagnostic::finished, this, [this, diagnostic](int result) {
            m_ConnectionDiagnostic = nullptr;
            diagnostic->deleteLater();
            if (m_DiagnosticGeneration == s_DiagnosticGeneration) {
                m_PortTestResults.store(result);
                emit connectionDiagnosticReady(result);
            }
            if (m_CleanupComplete) emit readyForDeletion();
        });
        diagnostic->start(QStringLiteral(CONN_TEST_SERVER), 443, ports);
    }, Qt::QueuedConnection);
}

void Session::cancelConnectivityTest()
{
    if (m_ConnectionDiagnostic) m_ConnectionDiagnostic->cancel();
}

void Session::connectionCleanupComplete()
{
    m_CleanupComplete = true;
    if (m_StartCancellation.isCancelled() && m_HostTargetMayExist) {
        cancelPendingStart();
        if (!m_CleanupComplete) return;
    }
    emit cleanupCompleteChanged();
    if (!m_ConnectionDiagnostic) emit readyForDeletion();
}

void Session::clLogMessage(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION,
                    SDL_LOG_PRIORITY_INFO,
                    format,
                    ap);
    va_end(ap);
}

void Session::clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event rumbleEvent = {};
    rumbleEvent.type = SDL_USEREVENT;
    rumbleEvent.user.code = SDL_CODE_GAMECONTROLLER_RUMBLE;
    rumbleEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    rumbleEvent.user.data2 = (void*)(uintptr_t)((lowFreqMotor << 16) | highFreqMotor);
    SDL_PushEvent(&rumbleEvent);
}

void Session::clConnectionStatusUpdate(int connectionStatus)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Connection status update: %d",
                connectionStatus);

    s_ActiveSession->m_NetworkPoor = connectionStatus == CONN_STATUS_POOR;
    if (!s_ActiveSession->m_Preferences->connectionWarnings) {
        return;
    }

    switch (connectionStatus)
    {
    case CONN_STATUS_POOR:
        s_ActiveSession->m_StreamStatus.setNetwork(s_ActiveSession->m_StreamConfig.bitrate > 5000
                ? StreamStatus::Network::Slow : StreamStatus::Network::Poor);
        break;
    case CONN_STATUS_OKAY:
        s_ActiveSession->m_StreamStatus.setNetwork(StreamStatus::Network::None);
        break;
    }
}

void Session::clSetHdrMode(bool enabled)
{
    // If we're in the process of recreating our decoder when we get
    // this callback, we'll drop it. The main thread will make the
    // callback when it finishes creating the new decoder.
    if (SDL_TryLockMutex(s_ActiveSession->m_DecoderLock) == 0) {
        IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
        if (decoder != nullptr) {
            decoder->setHdrMode(enabled);
        }
        SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
    }
}

void Session::clRumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event rumbleEvent = {};
    rumbleEvent.type = SDL_USEREVENT;
    rumbleEvent.user.code = SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS;
    rumbleEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    rumbleEvent.user.data2 = (void*)(uintptr_t)((leftTrigger << 16) | rightTrigger);
    SDL_PushEvent(&rumbleEvent);
}

void Session::clSetMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setMotionEventStateEvent = {};
    setMotionEventStateEvent.type = SDL_USEREVENT;
    setMotionEventStateEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE;
    setMotionEventStateEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    setMotionEventStateEvent.user.data2 = (void*)(uintptr_t)((motionType << 16) | reportRateHz);
    SDL_PushEvent(&setMotionEventStateEvent);
}

void Session::clSetControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b)
{
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setControllerLEDEvent = {};
    setControllerLEDEvent.type = SDL_USEREVENT;
    setControllerLEDEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED;
    setControllerLEDEvent.user.data1 = (void*)(uintptr_t)controllerNumber;
    setControllerLEDEvent.user.data2 = (void*)(uintptr_t)(r << 16 | g << 8 | b);
    SDL_PushEvent(&setControllerLEDEvent);
}

void Session::clSetAdaptiveTriggers(uint16_t controllerNumber, uint8_t eventFlags, uint8_t typeLeft, uint8_t typeRight, uint8_t *left, uint8_t *right){
    // We push an event for the main thread to handle in order to properly synchronize
    // with the removal of game controllers that could result in our game controller
    // going away during this callback.
    SDL_Event setControllerLEDEvent = {};
    setControllerLEDEvent.type = SDL_USEREVENT;
    setControllerLEDEvent.user.code = SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS;
    setControllerLEDEvent.user.data1 = (void*)(uintptr_t)controllerNumber;

    // Based on the following SDL code:
    // https://github.com/libsdl-org/SDL/blob/120c76c84bbce4c1bfed4e9eb74e10678bd83120/test/testgamecontroller.c#L286-L307
    DualSenseOutputReport *state = (DualSenseOutputReport *) SDL_malloc(sizeof(DualSenseOutputReport));
    SDL_zero(*state);
    state->validFlag0 = (eventFlags & DS_EFFECT_RIGHT_TRIGGER) | (eventFlags & DS_EFFECT_LEFT_TRIGGER);
    state->rightTriggerEffectType = typeRight;
    SDL_memcpy(state->rightTriggerEffect, right, sizeof(state->rightTriggerEffect));
    state->leftTriggerEffectType = typeLeft;
    SDL_memcpy(state->leftTriggerEffect, left, sizeof(state->leftTriggerEffect));

    setControllerLEDEvent.user.data2 = (void *) state;
    SDL_PushEvent(&setControllerLEDEvent);
}


bool Session::chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window, int videoFormat, int width, int height,
                            int frameRate, bool enableVsync, bool enableFramePacing, bool testOnly, IVideoDecoder*& chosenDecoder)
{
    DECODER_PARAMETERS params;

    // We should never have vsync enabled for test-mode.
    // It introduces unnecessary delay for renderers that may
    // block while waiting for a backbuffer swap.
    SDL_assert(!enableVsync || !testOnly);

    params.width = width;
    params.height = height;
    params.frameRate = frameRate;
    params.videoFormat = videoFormat;
    params.window = window;
    params.enableVsync = enableVsync;
    params.enableFramePacing = enableFramePacing;
    params.testOnly = testOnly;
    params.vds = vds;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "V-sync %s",
                enableVsync ? "enabled" : "disabled");

#ifdef HAVE_SLVIDEO
    chosenDecoder = new SLVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SLVideo video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load SLVideo decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#ifdef HAVE_FFMPEG
    chosenDecoder = new FFmpegVideoDecoder(testOnly);
    if (chosenDecoder->initialize(&params)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FFmpeg-based video decoder chosen");
        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load FFmpeg decoder");
        delete chosenDecoder;
        chosenDecoder = nullptr;
    }
#endif

#if !defined(HAVE_FFMPEG) && !defined(HAVE_SLVIDEO)
#error No video decoding libraries available!
#endif

    // If we reach this, we didn't initialize any decoders successfully
    return false;
}

int Session::drSetup(int videoFormat, int width, int height, int frameRate, void *, int)
{
    s_ActiveSession->m_ActiveVideoFormat = videoFormat;
    s_ActiveSession->m_ActiveVideoWidth = width;
    s_ActiveSession->m_ActiveVideoHeight = height;
    s_ActiveSession->m_ActiveVideoFrameRate = frameRate;

    // Defer decoder setup until we've started streaming so we
    // don't have to hide and show the SDL window (which seems to
    // cause pointer hiding to break on Windows).

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Video stream is %dx%dx%d (format 0x%x)",
                width, height, frameRate, videoFormat);

    return 0;
}

int Session::drSubmitDecodeUnit(PDECODE_UNIT du)
{
    // Use a lock since we'll be yanking this decoder out
    // from underneath the session when we initiate destruction.
    // We need to destroy the decoder on the main thread to satisfy
    // some API constraints (like DXVA2). If we can't acquire it,
    // that means the decoder is about to be destroyed, so we can
    // safely return DR_OK and wait for the IDR frame request by
    // the decoder reinitialization code.

    if (SDL_TryLockMutex(s_ActiveSession->m_DecoderLock) == 0) {
        IVideoDecoder* decoder = s_ActiveSession->m_VideoDecoder;
        if (decoder != nullptr) {
            int ret = decoder->submitDecodeUnit(du);
            SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
            return ret;
        }
        else {
            SDL_UnlockMutex(s_ActiveSession->m_DecoderLock);
            return DR_OK;
        }
    }
    else {
        // Decoder is going away. Ignore anything coming in until
        // the lock is released.
        return DR_OK;
    }
}

bool Session::getDecoderInfo(SDL_Window* window,
                             bool& isHardwareAccelerated, bool& isFullScreenOnly,
                             bool& isHdrSupported, QSize& maxResolution,
                             bool& hasHwHevcOrAv1Decode)
{
    isHardwareAccelerated = isFullScreenOnly = isHdrSupported = hasHwHevcOrAv1Decode = false;
    maxResolution = {};
    IVideoDecoder* decoder;

    // Default to false; only set true below when a FORCE_HARDWARE HEVC or
    // AV1 probe actually succeeds. Reaching the H.264 fallback at the bottom
    // of this function (or finding nothing at all) must never leave this
    // true unless the AV1 Main10 branch already set it (see below).
    hasHwHevcOrAv1Decode = false;

    // Since AV1 support on the host side is in its infancy, let's not consider
    // _only_ a working AV1 decoder to be acceptable and still show the warning
    // dialog indicating lack of hardware decoding support.

    // Try an HEVC Main10 decoder first to see if we have HDR support
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      window, VIDEO_FORMAT_H265_MAIN10, 1920, 1080, 60,
                      false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        isHdrSupported = decoder->isHdrSupported();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        hasHwHevcOrAv1Decode = true;
        return true;
    }

    // Try an AV1 Main10 decoder next to see if we have HDR support
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      window, VIDEO_FORMAT_AV1_MAIN10, 1920, 1080, 60,
                      false, false, true, decoder)) {
        // If we've got a working AV1 Main 10-bit decoder, we'll enable the HDR checkbox
        // but we will still continue probing to get other attributes for HEVC or H.264
        // decoders. See the AV1 comment at the top of the function for more info.
        isHdrSupported = decoder->isHdrSupported();
        delete decoder;

        // A working hardware AV1 decoder already proves modern decode
        // silicon, even though we keep probing below (no early return here)
        // to fill in isHardwareAccelerated/isFullScreenOnly/maxResolution
        // from the best non-Main10 decoder. Neither of the branches below
        // ever resets this back to false.
        hasHwHevcOrAv1Decode = true;
    }
    else {
        // If we found no hardware decoders with HDR, check for a renderer
        // that supports HDR rendering with software decoded frames.
        if (chooseDecoder(StreamingPreferences::VDS_FORCE_SOFTWARE,
                          window, VIDEO_FORMAT_H265_MAIN10, 1920, 1080, 60,
                          false, false, true, decoder) ||
            chooseDecoder(StreamingPreferences::VDS_FORCE_SOFTWARE,
                          window, VIDEO_FORMAT_AV1_MAIN10, 1920, 1080, 60,
                          false, false, true, decoder)) {
            isHdrSupported = decoder->isHdrSupported();
            delete decoder;
        }
        else {
            // We weren't compiled with an HDR-capable renderer or we don't
            // have the required GPU driver support for any HDR renderers.
            isHdrSupported = false;
        }
    }

    // Try a regular hardware accelerated HEVC decoder now
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      window, VIDEO_FORMAT_H265, 1920, 1080, 60,
                      false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        hasHwHevcOrAv1Decode = true;
        return true;
    }


#if 0 // See AV1 comment at the top of this function
    if (chooseDecoder(StreamingPreferences::VDS_FORCE_HARDWARE,
                      window, VIDEO_FORMAT_AV1_MAIN8, 1920, 1080, 60,
                      false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        return true;
    }
#endif

    // If we still didn't find a hardware decoder, try H.264 now.
    // This will fall back to software decoding, so it should always work.
    if (chooseDecoder(StreamingPreferences::VDS_AUTO,
                      window, VIDEO_FORMAT_H264, 1920, 1080, 60,
                      false, false, true, decoder)) {
        isHardwareAccelerated = decoder->isHardwareAccelerated();
        isFullScreenOnly = decoder->isAlwaysFullScreen();
        maxResolution = decoder->getDecoderMaxResolution();
        delete decoder;

        // hasHwHevcOrAv1Decode is intentionally left as-is here: it stays
        // true only if the AV1 Main10 probe above already set it, and false
        // otherwise. Falling through to the H.264 decoder never means we
        // have HEVC/AV1 hardware decode.
        return true;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find ANY working H.264 or HEVC decoder!");
    return false;
}

Session::DecoderAvailability
Session::getDecoderAvailability(SDL_Window* window,
                                StreamingPreferences::VideoDecoderSelection vds,
                                int videoFormat, int width, int height, int frameRate)
{
    const QString key = QStringLiteral("%1/%2/%3x%4x%5").arg(int(vds)).arg(videoFormat).arg(width).arg(height).arg(frameRate);
    const auto known = m_DecoderProbes.constFind(key);
    if (known != m_DecoderProbes.constEnd()) {
        return *known;
    }

    IVideoDecoder* decoder;

    if (!chooseDecoder(vds, window, videoFormat, width, height, frameRate, false, false, true, decoder)) {
        m_DecoderProbes.insert(key, DecoderAvailability::None);
        return DecoderAvailability::None;
    }

    bool hw = decoder->isHardwareAccelerated();
    // A 10-bit decoder that can't present HDR means this display can't show it.
    if (videoFormat & VIDEO_FORMAT_MASK_10BIT) {
        m_DisplayHdr = qMax(m_DisplayHdr, decoder->isHdrSupported() ? 1 : 0);
    }

    delete decoder;

    const auto result = hw ? DecoderAvailability::Hardware : DecoderAvailability::Software;
    m_DecoderProbes.insert(key, result);
    return result;
}

void Session::planStream(SDL_Window* testWindow)
{
    StreamPlanInput input;
    {
        QReadLocker lock(&m_Computer->lock);
        input.hostName = m_Computer->name;
        m_PerformanceHostContext = m_Computer->serverSoftwareLabel() + QLatin1Char('/') + m_Computer->hostSoftwareRevision;
        // Old shared-capture observations do not describe a newly owned main
        // desktop. Keep transport/encoder-change observations independently.
        if (m_IsDesktopRequest) m_PerformanceHostContext += QStringLiteral("/main-desktop-v2");
        input.performanceContext = m_PerformanceHostContext + QLatin1Char('/') +
            (m_PreferredAddress.isNull() ? m_Computer->activeAddress.toString() : m_PreferredAddress.toString());
        input.serverCodecModeSupport = quint32(m_Computer->serverCodecModeSupport);
        if (m_Computer->maxLumaPixelsHEVC == 0) input.serverCodecModeSupport &= ~quint32(SCM_MASK_HEVC);
    }
    input.capacity = HostCapacity::load(m_Computer->uuid);
    { QReadLocker lock(&m_Computer->lock); input.integerDesktopScale = m_Computer->desktopScaleSupported; }

    // Automatic size and rate were measured from the display by the launcher. A Manual
    // choice is compared against the screen the launcher is on.
    QScreen* screen = m_QtWindow ? m_QtWindow->screen() : QGuiApplication::primaryScreen();
    const QSize screenPixels = screen ? screen->size() * screen->devicePixelRatio() : QSize();
    m_PlannedSizeAutomatically = m_Preferences->automaticResolution;
    input.displayScale = screen ? screen->devicePixelRatio() : 0;
    if (m_Preferences->automaticResolution) {
        input.nativeWidth = m_Preferences->width;
        input.nativeHeight = m_Preferences->height;
    }
    else {
        input.nativeWidth = screenPixels.width();
        input.nativeHeight = screenPixels.height();
        input.width = m_Preferences->width;
        input.height = m_Preferences->height;
    }
    if (m_Preferences->automaticFrameRate) {
        input.displayHz = m_Preferences->fps;
    }
    else {
        input.displayHz = screen ? qRound(screen->refreshRate()) : 0;
        input.fps = m_Preferences->fps;
    }
    if (!m_Preferences->autoAdjustBitrate) {
        input.bitrateKbps = m_StreamConfig.bitrate;
    }
    switch (m_Preferences->videoCodecConfig) {
    case StreamingPreferences::VCC_FORCE_H264: input.codec = QStringLiteral("H.264"); break;
    case StreamingPreferences::VCC_FORCE_HEVC:
    case StreamingPreferences::VCC_FORCE_HEVC_HDR_DEPRECATED: input.codec = QStringLiteral("HEVC"); break;
    case StreamingPreferences::VCC_FORCE_AV1: input.codec = QStringLiteral("AV1"); break;
    default: break;
    }

    // What this computer decodes in hardware, at the largest stream the plan can pick.
    // Codecs the host doesn't send aren't worth the probe.
    const int probeWidth = input.width > 0 ? input.width : input.nativeWidth;
    const int probeHeight = input.height > 0 ? input.height : input.nativeHeight;
    const int probeFps = input.fps > 0 ? input.fps : input.displayHz;
    const auto hardware = [&](int format) {
        return getDecoderAvailability(testWindow, m_Preferences->videoDecoderSelection, format,
                                      probeWidth, probeHeight, probeFps) == DecoderAvailability::Hardware;
    };
    QSettings decodes;
    decodes.beginGroup(QStringLiteral("clientdecode"));
    input.h264 = hardware(VIDEO_FORMAT_H264);
    decodes.setValue(QStringLiteral("h264"), input.h264);
    if (input.serverCodecModeSupport & SCM_HEVC) {
        input.hevc = hardware(VIDEO_FORMAT_H265);
        decodes.setValue(QStringLiteral("hevc"), input.hevc);
    }
    if (input.serverCodecModeSupport & SCM_AV1_MAIN8) {
        input.av1 = hardware(VIDEO_FORMAT_AV1_MAIN8);
        decodes.setValue(QStringLiteral("av1"), input.av1);
    }
    decodes.setValue(QStringLiteral("hevc"), input.hevc);
    decodes.setValue(QStringLiteral("av1"), input.av1);
    decodes.setValue(QStringLiteral("context"), StreamPlanning::decoderContext(screen ? screen->name() : QString(),
        probeWidth, probeHeight, probeFps, input.displayScale, int(m_Preferences->videoDecoderSelection)));
    decodes.setValue(QStringLiteral("measuredAt"), QDateTime::currentSecsSinceEpoch());
    if (m_Preferences->enableHdr) {
        input.hevc10 = (input.serverCodecModeSupport & SCM_HEVC_MAIN10) && hardware(VIDEO_FORMAT_H265_MAIN10);
        input.av1_10 = (input.serverCodecModeSupport & SCM_AV1_MAIN10) && hardware(VIDEO_FORMAT_AV1_MAIN10);
    }

    // Observed delivery belongs to this route and viewing display, not a universal
    // claim about the host's encoder. Active measurements remain useful with queues.
    m_ViewingContext = StreamPlanning::viewingContext({},input.nativeWidth,input.nativeHeight,input.displayHz,input.displayScale);
    // An encoded-size trial must not resize a desktop whose DPI cannot be
    // negotiated. RTSP scales its picture while launch keeps the native layout.
    if (m_PlannedSizeAutomatically && !input.integerDesktopScale && !m_Computer->isNvidiaServerSoftware)
        m_HostDesktopSize = QSize(input.nativeWidth,input.nativeHeight);
    input.performanceContext += m_ViewingContext;
    m_PerformanceContext = input.performanceContext;
    const auto initialPlan = ::planStream(input);
    if ((m_PlannedSizeAutomatically || m_Preferences->autoAdjustBitrate) &&
            !m_SafeSettingsTrial && !m_ConnectionRecovery && !m_Adaptation.pending) {
        const auto eligible = m_PlannedSizeAutomatically
            ? StreamPlanning::ladder(input.nativeWidth, input.nativeHeight, initialPlan.fps,
                                     false, input.displayScale, input.integerDesktopScale)
            : QVector<StreamPlanning::Rung> {{initialPlan.width, initialPlan.height, initialPlan.fps}};
        const auto saved = StreamRecommendation::load(m_Computer->uuid, m_PerformanceContext, initialPlan.codec);
        const bool permitted = saved.bitrateKbps > 0 ? m_Preferences->autoAdjustBitrate : m_PlannedSizeAutomatically;
        if (permitted &&
                saved.valid(eligible, m_PerformanceContext, initialPlan.codec, QDateTime::currentSecsSinceEpoch())) {
            m_Adaptation.baseline = saved.baseline;
            m_Adaptation.target = saved.target;
            m_Adaptation.pending = true;
            m_Adaptation.checkTrial = !saved.rollback && saved.bitrateKbps == 0;
            m_Adaptation.encoderCeilingKbps = saved.bitrateKbps;
            m_Adaptation.restoring = m_Adaptation.locked = saved.rollback;
            m_Adaptation.transitionAtMs = LiGetMicroseconds() / 1000;
        }
    }
    if (m_PlannedSizeAutomatically && !m_SafeSettingsTrial && !m_ConnectionRecovery && !m_Adaptation.pending &&
            input.capacity.performanceContext == m_PerformanceContext && input.capacity.codec == initialPlan.codec &&
            input.capacity.measuredAt <= QDateTime::currentSecsSinceEpoch() &&
            QDateTime::currentSecsSinceEpoch() - input.capacity.measuredAt <= StreamPlanning::SampleLifetimeSeconds) {
        m_Adaptation.request({initialPlan.width, initialPlan.height, initialPlan.fps},
            StreamAdaptation::mode(input.capacity), input.capacity, LiGetMicroseconds() / 1000);
    }
    if (m_ConnectionRecovery && m_RecoveryMode.width > 0 && m_RecoveryMode.height > 0 && m_RecoveryMode.fps > 0) {
        // A real network recovery resumes the picture already chosen by the user.
        input.width = m_RecoveryMode.width;
        input.height = m_RecoveryMode.height;
        input.fps = m_RecoveryMode.fps;
        input.bitrateKbps = m_RecoveryBitrateKbps;
        m_StreamConfig.bitrate = m_RecoveryBitrateKbps;
    }
    if (m_PlannedSizeAutomatically && m_Adaptation.pending) {
        input.trialWidth = m_Adaptation.target.width;
        input.trialHeight = m_Adaptation.target.height;
        input.capacity = m_Adaptation.baseline;
    }
    m_StreamPlan = ::planStream(input);
    m_StreamConfig.width = m_StreamPlan.width;
    m_StreamConfig.height = m_StreamPlan.height;
    m_StreamConfig.fps = m_StreamPlan.fps;
    if (m_Preferences->autoAdjustBitrate) {
        m_StreamConfig.bitrate = m_StreamPlan.startBitrateKbps;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Stream plan: %dx%dx%d %s, %d kbps (ceiling %d, floor %d)%s%s",
                m_StreamPlan.width, m_StreamPlan.height, m_StreamPlan.fps, qPrintable(m_StreamPlan.codec),
                m_StreamPlan.startBitrateKbps, m_StreamPlan.ceilingKbps, m_StreamPlan.floorKbps,
                m_StreamPlan.limitedByHost ? "; " : "", qPrintable(m_StreamPlan.limitReason));
    for (const QString& warning : {m_StreamPlan.resolutionWarning, m_StreamPlan.fpsWarning,
                                   m_StreamPlan.codecWarning, m_StreamPlan.bitrateWarning}) {
        if (!warning.isEmpty()) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stream plan: %s", qPrintable(warning));
    }
}

bool Session::capacityNetworkClean() const
{
    if (m_NetworkPoor.load()) return false;
    if (!m_AutomaticBitrate) return true;
    const auto status = m_AutomaticBitrate->status();
    return status.lossPercent < .5 && status.queueMs < 8;
}

void Session::rememberStreamMeasurements(bool early)
{
    HostLatencyMeter latency;
    {
        std::lock_guard<std::mutex> lock(m_MeasurementLock);
        latency = early ? m_RecentHostLatency : m_LastQualifiedHostLatency;
        if (!early && (!m_LastQualifiedHostAt || Uint32(SDL_GetTicks() - m_LastQualifiedHostAt) > 30000)) return;
    }
    if (!latency.enoughToJudge() || !m_ActiveVideoWidth || !m_ActiveVideoHeight || !m_ActiveVideoFrameRate) {
        return;
    }
    const auto capacity = streamMeasurement(latency);
    capacity.save(m_Computer->uuid);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Host processing latency at %dx%dx%d: average %.1f ms, 95th percentile %.1f ms over %.0f s",
                m_ActiveVideoWidth, m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                latency.averageMs(), latency.p95Ms(), latency.spanUs() / 1e6);
}

HostCapacity Session::streamMeasurement(const HostLatencyMeter& latency) const
{
    const QString codec = (m_ActiveVideoFormat & VIDEO_FORMAT_MASK_AV1) ? QStringLiteral("AV1")
        : (m_ActiveVideoFormat & VIDEO_FORMAT_MASK_H265) ? QStringLiteral("HEVC") : QStringLiteral("H.264");
    // Only this record's own fields change; how bitrate changes land is kept as it was.
    HostCapacity capacity = HostCapacity::load(m_Computer->uuid);
    if (capacity.performanceContext != m_PerformanceContext || capacity.codec != codec) {
        capacity.overloadedPixelRate = capacity.overloadedAt = capacity.reductionRejectedUntil = 0;
    }
    StreamPlanning::remember(capacity, m_ActiveVideoWidth, m_ActiveVideoHeight, m_ActiveVideoFrameRate, codec,
                             latency.averageMs(), latency.p95Ms(), QDateTime::currentSecsSinceEpoch(), latency.sourceFps(),
                             StreamPlanning::adaptationEvidence(latency, m_ActiveVideoWidth, m_ActiveVideoHeight,
                                 m_ActiveVideoFrameRate, m_CaptureHealthSupported, m_CapturedFps.load(), sourceHealthy(),
                                 capacityNetworkClean()).active, latency.deliveredFps());
    capacity.performanceContext = m_PerformanceContext;
    return capacity;
}

QVector<StreamPlanning::Rung> Session::recommendationModes(int fps) const
{
    if (!m_PlannedSizeAutomatically)
        return {{m_StreamConfig.width, m_StreamConfig.height, m_StreamConfig.fps}};
    QReadLocker lock(&m_Computer->lock);
    return StreamPlanning::ladder(m_Preferences->width, m_Preferences->height,
        fps, false, m_LocalDisplayScale, m_Computer->desktopScaleSupported);
}

void Session::recommendStreamChange(StreamPlanning::Rung target, const HostCapacity& baseline, bool rollback, int bitrateKbps)
{
    if (m_Recommendation.createdAt && (!rollback || m_Recommendation.rollback)) return;
    StreamRecommendation candidate {target, baseline, QDateTime::currentSecsSinceEpoch(), rollback, bitrateKbps};
    const auto rungs = recommendationModes(m_ActiveVideoFrameRate);
    if (!candidate.valid(rungs, m_PerformanceContext, m_StreamPlan.codec, candidate.createdAt)) return;
    m_Recommendation = candidate;
    candidate.save(m_Computer->uuid, m_PerformanceContext, m_StreamPlan.codec);
    const auto bitrate = bitrateKbps > 0 ? QStringLiteral(" at %1 kbps encoder target").arg(bitrateKbps) : QString();
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stream recommendation: %dx%dx%d for next connection%s; current stream continues",
        target.width, target.height, target.fps, qPrintable(bitrate));
    emit controlsChanged();
}

bool Session::populateDecoderProperties(SDL_Window* window)
{
    IVideoDecoder* decoder;

    if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                       window,
                       m_SupportedVideoFormats.first(),
                       m_StreamConfig.width,
                       m_StreamConfig.height,
                       m_StreamConfig.fps,
                       false, false, true, decoder)) {
        return false;
    }

    m_VideoCallbacks.capabilities = decoder->getDecoderCapabilities();
    if (m_VideoCallbacks.capabilities & CAPABILITY_PULL_RENDERER) {
        // It is an error to pass a push callback when in pull mode
        m_VideoCallbacks.submitDecodeUnit = nullptr;
    }
    else {
        m_VideoCallbacks.submitDecodeUnit = drSubmitDecodeUnit;
    }

    if (Utils::getEnvironmentVariableOverride("COLOR_SPACE_OVERRIDE", &m_StreamConfig.colorSpace)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using colorspace override: %d",
                    m_StreamConfig.colorSpace);
    }
    else {
        m_StreamConfig.colorSpace = decoder->getDecoderColorspace();
    }

    if (Utils::getEnvironmentVariableOverride("COLOR_RANGE_OVERRIDE", &m_StreamConfig.colorRange)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using color range override: %d",
                    m_StreamConfig.colorRange);
    }
    else {
        m_StreamConfig.colorRange = decoder->getDecoderColorRange();
    }

    if (decoder->isAlwaysFullScreen()) {
        m_IsFullScreen = true;
    }

    delete decoder;

    return true;
}

Session::Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences, bool safeSettings, bool desktopRequest, bool desktopTakeover)
    : m_OwnedPreferences((preferences ? preferences : StreamingPreferences::get())->cloneForSession(safeSettings)),
      m_Preferences(m_OwnedPreferences.get()),
      m_SafeSettingsTrial(safeSettings),
      m_IsFullScreen(m_Preferences->windowMode != StreamingPreferences::WM_WINDOWED || !WMUtils::isRunningDesktopEnvironment()),
      m_Computer(computer),
      m_ComputerOwner(computer->retain()),
      m_App(app),
      m_Window(nullptr),
      m_VideoDecoder(nullptr),
      m_DecoderLock(SDL_CreateMutex()),
      m_AudioMuted(false),
      m_QtWindow(nullptr),
      m_UnexpectedTermination(true), // Failure prior to streaming is unexpected
      m_InputHandler(nullptr),
      m_ClipboardSync(nullptr),
      m_CursorSync(nullptr),
      m_MouseEmulationRefCount(0),
      m_FlushingWindowEventsRef(0),
      m_ShouldExit(false),
      m_AsyncConnectionSuccess(false),
      m_PortTestResults(-1),
      m_OpusDecoder(nullptr),
      m_AudioRenderer(nullptr),
      m_AudioSampleCount(0),
      m_DropAudioEndTime(0)
{
    // The configured Desktop means main-desktop intent in GUI and CLI.
    // A UUID-identified running-app shortcut remains explicit shared resume.
    m_IsDesktopRequest = desktopRequest || app.isDesktopApp();
    m_DesktopTakeoverApproved = desktopTakeover;
    m_StartupTimer.start();
    m_Preferences->absoluteMouseMode = CursorModePolicy::absoluteInput(m_Preferences->automaticPointer,
        m_Preferences->absoluteMouseMode, m_App.isDesktopTarget());
    traceStartup("session-created");
    connect(this, &Session::readyForDeletion, this, [this] {
        m_DeletionReady = true;
        if (!m_RetryRetained) deleteLater();
    });
    {
        QReadLocker lock(&m_Computer->lock);
        m_CursorPolicy = CursorModePolicy(m_Preferences->localCursor,
                                         m_Preferences->absoluteMouseMode,
                                         m_Computer->serverSoftware == QStringLiteral("Skylight"),
                                         qEnvironmentVariableIntValue("MOONLIGHT_LOCAL_CURSOR"));
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Host software: %s %s source %s (protocol %s)",
                    qUtf8Printable(m_Computer->serverSoftware),
                    qUtf8Printable(m_Computer->hostSoftwareVersion),
                    qUtf8Printable(m_Computer->hostSoftwareRevision),
                    qUtf8Printable(m_Computer->appVersion));
    }

    // Parse the deliberate route without mutating the polled host while its
    // caller holds a read lock. The active session acquires a route lease only
    // after it owns the streaming slot; cleanup releases that lease. The
    // environment override takes precedence over the saved per-host route.
    QString hostOverride = qEnvironmentVariable("MOONLIGHT_HOST_OVERRIDE");
    if (!hostOverride.isEmpty()) {
        QHostAddress parsedAddr;
        if (parsedAddr.setAddress(hostOverride)) {
            m_PreferredAddress = NvAddress(hostOverride, DEFAULT_HTTP_PORT);
        }
        else {
            qWarning() << "MOONLIGHT_HOST_OVERRIDE value" << hostOverride << "is not a valid IP address; ignoring";
        }
    }
    if (m_PreferredAddress.isNull() && !m_Preferences->connectionAddress.isEmpty()) {
        const QString address = m_Preferences->connectionAddress.trimmed();
        QHostAddress literal;
        if (literal.setAddress(address)) m_PreferredAddress = NvAddress(address, DEFAULT_HTTP_PORT);
        else {
            const QUrl route(QStringLiteral("//") + address);
            if (!route.host().isEmpty()) m_PreferredAddress = NvAddress(route.host(), route.port(DEFAULT_HTTP_PORT));
        }
    }
}

Session::~Session()
{
    // NB: This may not get destroyed for a long time! Don't put any non-trivial cleanup here.
    // Use Session::exec() or DeferredSessionCleanupTask instead.

    if (m_RouteGeneration) m_Computer->endStreamingRoute(m_RouteGeneration);
    delete m_AutomaticBitrate;
    SDL_DestroyMutex(m_DecoderLock);
}

bool Session::initialize(QWindow* qtWindow)
{
    traceStartup("prepare-video");
    const bool result = initializeInternal(qtWindow);
    traceStartup(result ? "video-prepared" : "video-preparation-failed");
    if (!result) connectionCleanupComplete();
    return result;
}

bool Session::hasLastSuccessfulSettings() const
{
    return bool(m_Preferences->cloneWithLastSuccessfulSettings());
}

Session* Session::createRetrySession(bool safeSettings, bool resumeOnly, bool manualRetry, bool lastSuccessful)
{
    if (!m_CleanupComplete || m_Computer->isRemoved() ||
            (m_StartCancellation.isCancelled() && !(manualRetry && m_RetryRetained))) return nullptr;
    cancelConnectivityTest();
    auto app = m_App;
    auto previousSettings = lastSuccessful ? m_Preferences->cloneWithLastSuccessfulSettings() : nullptr;
    if (lastSuccessful && !previousSettings) return nullptr;
    auto retry = new Session(m_Computer, app, previousSettings ? previousSettings.get() : m_Preferences, safeSettings, m_IsDesktopRequest);
    retry->m_SafeSettingsTrial = safeSettings || (!lastSuccessful && m_SafeSettingsTrial);
    retry->m_HostTargetMayExist = m_HostTargetMayExist;
    m_HostTargetMayExist = false; // The successor now owns final cleanup.
    retry->m_ResumeOnly = resumeOnly;
    if (!manualRetry && !safeSettings && !lastSuccessful) {
        retry->m_Adaptation = m_Adaptation;
        retry->m_ConnectionRecovery = !m_AdjustingForHost;
        retry->m_RejectTrialMeasurement = retry->m_ConnectionRecovery && m_RejectTrialMeasurement;
        retry->m_RecoveryMode = {m_StreamConfig.width, m_StreamConfig.height, m_StreamConfig.fps};
        const int automaticKbps = m_AutomaticBitrateKbps.load();
        const int runtimeKbps = m_RuntimeBitrateKbps.load();
        retry->m_RecoveryBitrateKbps = automaticKbps > 0
            ? RuntimeBitratePolicy::wireKbps(automaticKbps,
                CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration))
            : runtimeKbps > 0 ? runtimeKbps : m_StreamConfig.bitrate;
    }
    retry->m_ManualAudioMuted = m_ManualAudioMuted;
    retry->m_AudioMuted = m_ManualAudioMuted;
    retry->m_ClipboardDirection = m_ClipboardDirection;
    retry->m_ClipboardPaused = m_ClipboardPaused;
    retry->m_HaveCaptureOverride = m_HaveCaptureOverride;
    retry->m_DesiredCapture = m_DesiredCapture;
    return retry;
}

void Session::releaseRetryRetention()
{
    m_RetryRetained = false;
    if (m_DeletionReady) deleteLater();
}

bool Session::keepSafeSettings()
{
    if (!m_SafeSettingsTrial || !m_HasVideoFrame.load()) return false;
    m_SafeSettingsKept = StreamingPreferences::get()->keepSessionSettings(*m_Preferences);
    if (m_SafeSettingsKept) m_Preferences->rememberSuccessfulSettings();
    emit controlsChanged();
    return m_SafeSettingsKept;
}

void Session::notifyFramePresented()
{
    auto* active = s_ActiveSession;
    if (!active) return;
    const Uint32 now = SDL_GetTicks();
    active->m_LastFrameAt.store(now);
    active->m_PresentedFrames.fetch_add(1);
    if (!active->m_HasVideoFrame.exchange(true)) {
        active->m_FirstFrameAt.store(now);
        active->traceStartup("first-frame-presented");
    }
}

void Session::notifyFrameArrived(quint64 arrivalUs, bool windowVisible)
{
    auto* active = s_ActiveSession;
    if (!active || active->m_PacingMode != PacingMode::Automatic) return;
    bool pacing;
    {
        std::lock_guard<std::mutex> lock(active->m_MeasurementLock);
        if (!active->m_AutomaticPacing.observe(arrivalUs, windowVisible)) return;
        pacing = active->m_AutomaticPacing.active();
    }
    active->m_PacingActive.store(pacing);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                pacing ? "Frames are arriving in bunches; pacing them to the display"
                       : "Frames are arriving steadily; no longer pacing them");
    // The renderer is chosen with pacing on or off, so it is made again.
    SDL_Event event = {};
    event.type = SDL_RENDER_DEVICE_RESET;
    SDL_PushEvent(&event);
}

bool Session::framePacingEnabled(bool enableVsync) const
{
    if (!enableVsync) return false;
    switch (m_PacingMode) {
    case PacingMode::On: return true;
    case PacingMode::Off: return false;
    case PacingMode::Automatic: return m_PacingActive.load();
    }
    return false;
}

void Session::rememberPacing()
{
    AutomaticPacing pacing;
    {
        std::lock_guard<std::mutex> lock(m_MeasurementLock);
        pacing = m_AutomaticPacing;
    }
    // The next stream from this host starts where this one ended, if it watched long enough.
    if (m_PacingMode == PacingMode::Automatic && pacing.visibleUs() >= HostLatencyMeter::MeaningfulUs) {
        QSettings().setValue(QStringLiteral("automaticpacing/") + m_Computer->uuid, pacing.active());
    }
}

void Session::traceStartup(const char* phase) const
{
    qInfo("Connection startup: phase=%s elapsed_ms=%lld", phase,
          static_cast<long long>(m_StartupTimer.elapsed()));
}

QVariantMap Session::controlState() const
{
    QString hostName;
    { QReadLocker lock(&m_Computer->lock); hostName = m_Computer->name; }
    const int runtimeKbps = m_RuntimeBitrateKbps.load();
    return {{"host", hostName}, {"app", m_App.name},
            {"format", tr("%1 × %2 · %3 fps target · %4 Mbps launch budget").arg(m_StreamConfig.width).arg(m_StreamConfig.height)
                       .arg(m_StreamConfig.fps).arg(m_StreamConfig.bitrate / 1000.0, 0, 'f', 1)},
            {"mediaProtection", m_NegotiatedEncryption < 0 ? QString()
                : (m_NegotiatedEncryption & ENCFLG_VIDEO) ? tr("Video encrypted") : tr("Video is not encrypted")},
            {"viewScale", m_Preferences->viewScale}, {"viewingOnly", !m_RemoteInputAvailable.load()},
            {"muted", m_ManualAudioMuted}, {"capture", m_CaptureBeforeControls}, {"absolute", m_InputHandler && m_InputHandler->isAbsoluteMouseMode()},
            {"systemKeys", m_SystemKeysRequested}, {"fullscreen", m_Window && bool(SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN)},
            {"localText", m_Preferences->localTextInput}, {"clipboardPaused", m_ClipboardPaused},
            {"clipboardDirection", m_ClipboardDirection}, {"imageClipboard", m_ImageClipboardSupported},
            {"clipboardNotice", m_ControlsClipboardNotice},
            {"clipboardReadAllowed", m_HostCapabilities.allows(HostCapabilities::ClipboardRead)},
            {"clipboardWriteAllowed", m_HostCapabilities.allows(HostCapabilities::ClipboardWrite)},
            {"liveBitrateSupported", m_RuntimeBitrateSupported && !m_AutomaticBitrate},
            {"liveBitrateKbps", runtimeKbps > 0 ? runtimeKbps : m_StreamConfig.bitrate},
            // Unless it's known to change in place, each new value may freeze the picture.
            {"liveBitrateRebuilds", m_BitrateChange != HostCapacity::BitrateChange::Live},
            {"automaticBitrate", m_AutomaticBitrate != nullptr},
            {"automaticBitrateKbps", m_AutomaticBitrateKbps.load()},
            {"automaticBitrateDeferred", m_BitrateChange != HostCapacity::BitrateChange::Live},
            {"liveBitrateNotice", m_RuntimeBitrateNotice},
            {"optimizationAvailable", m_Recommendation.createdAt > 0},
            {"optimizationNotice", m_Recommendation.createdAt <= 0 ? QString() : m_Recommendation.bitrateKbps > 0
                ? tr("Try %1 Mbps at the current resolution. Moonwalk will use it next time you connect. Apply Now reconnects this stream.")
                    .arg(m_Recommendation.bitrateKbps / 1000.0, 0, 'f', 1)
                : m_Recommendation.rollback
                ? tr("The new picture didn't improve delivery. Moonwalk will use the previous mode next time. Apply Now reconnects this stream.")
                : tr("Try %1 × %2 at %3 FPS. Moonwalk will use it next time you connect. Apply Now reconnects this stream.")
                    .arg(m_Recommendation.target.width).arg(m_Recommendation.target.height).arg(m_Recommendation.target.fps)},
            {"safeTrial", m_SafeSettingsTrial}, {"safeKept", m_SafeSettingsKept}, {"hasVideo", hasVideoFrame()}};
}

QVariantMap Session::liveStats() const
{
    uint32_t rtt = 0;
    const bool hasRtt = m_ConnectionWasEstablished.load() && LiGetEstimatedRttInfo(&rtt, nullptr);
    double hostAverageMs, hostP95Ms;
    {
        std::lock_guard<std::mutex> lock(m_MeasurementLock);
        hostAverageMs = m_HostLatency.averageMs();
        hostP95Ms = m_HostLatency.p95Ms();
    }
    return {{"rttMs", hasRtt ? int(rtt) : -1},
            {"hostLatencyAverageMs", hostAverageMs},
            {"hostLatencyP95Ms", hostP95Ms},
            {"presentedFrames", qulonglong(m_PresentedFrames.load())},
            {"networkPoor", m_NetworkPoor.load()}};
}

QObject* Session::fileTransfer() const { return m_FileTransfer; }

void Session::updateLocalTextInput()
{
    m_CompositionText.clear();
    if (m_InputHandler) m_InputHandler->setTextComposition(false);
    if (m_Window && !m_ControlsOpen && m_Preferences->localTextInput &&
            (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS)) {
        int width, height;
        SDL_GetWindowSize(m_Window, &width, &height);
        SDL_Rect caret {width / 2, qMax(0, height - 80), 1, 24};
        SDL_SetTextInputRect(&caret);
        SDL_StartTextInput();
    }
    else SDL_StopTextInput();
}

void Session::openControls()
{
    if (!WMUtils::isRunningDesktopEnvironment()) return;
    if (s_ActiveSession != this || !m_ConnectionWasEstablished || !m_Window || !m_InputHandler || m_ControlsOpen) return;
    m_CaptureBeforeControls = m_InputHandler->isCaptureActive();
    m_SystemKeysRequested = m_Preferences->captureSysKeysMode != StreamingPreferences::CSK_OFF;
    m_ControlsOpen = true;
    m_InputHandler->pauseForControls();
    updateLocalTextInput();
    if (m_ClipboardSync) m_ClipboardSync->setFocused(false);
    if (m_CursorSync) { m_CursorSync->setFocused(false); m_CursorSync->resetCursor(); }
    emit controlsChanged();
}

void Session::prepareControlsClipboard()
{
    if (s_ActiveSession == this && m_ControlsOpen && m_Window &&
            !m_StartCancellation.isCancelled()) {
        const QString notice = NativeClipboard::prepareForQtControls() ? QString() :
            tr("Clipboard paste is unavailable in this panel. Return to the stream and paste into the remote app.");
        if (notice != m_ControlsClipboardNotice) {
            m_ControlsClipboardNotice = notice;
            emit controlsChanged();
        }
    }
}

void Session::closeControls()
{
    if (!m_ControlsOpen) return;
    m_ControlsOpen = false;
    emit controlsChanged();
    if (s_ActiveSession != this || !m_Window || !m_InputHandler || m_StartCancellation.isCancelled()) return;
    // Discard any controls-window key state before returning to the same stream.
    m_InputHandler->pauseForControls();
    SDL_RaiseWindow(m_Window);
    if (m_CaptureBeforeControls) m_InputHandler->setCaptureActive(true);
    updateLocalTextInput();
    notifyMouseModeChanged(m_InputHandler->isAbsoluteMouseMode());
}

void Session::controlAction(QString action, QVariant value)
{
    if (action == "applyOptimization")
        qInfo("Stream recommendation apply: controls=%d active=%d input=%d offered=%d cancelled=%d",
            m_ControlsOpen, s_ActiveSession == this, m_InputHandler != nullptr,
            m_Recommendation.createdAt > 0, m_StartCancellation.isCancelled());
    if ((!m_ControlsOpen && !m_ToolbarAction) || s_ActiveSession != this || !m_InputHandler) return;
    if (action == "mute") {
        m_ManualAudioMuted = value.toBool();
        m_AudioMuted = m_ManualAudioMuted || (m_Preferences->muteOnFocusLoss &&
                       !(SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS));
    }
    else if (action == "capture") {
        m_HaveCaptureOverride = true;
        m_DesiredCapture = m_CaptureBeforeControls = value.toBool();
        if (!m_ControlsOpen) {
            if (!value.toBool()) m_InputHandler->pauseForControls();
            else m_InputHandler->setCaptureActive(true);
        }
    }
    else if (action == "absolute") m_InputHandler->setAbsoluteMouseMode(value.toBool());
    else if (action == "bitrate" && m_RuntimeBitrate && m_RuntimeBitrateSupported) {
        bool ok = false; const int requested = value.toInt(&ok);
        if (ok && requested >= 500 && requested <= (m_Preferences->unlockBitrate ? 500000 : 150000))
            m_RuntimeBitrate->requestBitrate(requested);
    }
    else if (action == "systemKeys") {
        m_SystemKeysRequested = value.toBool();
        m_Preferences->captureSysKeysMode = m_SystemKeysRequested ? StreamingPreferences::CSK_ALWAYS : StreamingPreferences::CSK_OFF;
        m_InputHandler->setSystemKeyCapture(m_SystemKeysRequested);
    }
    else if (action == "fullscreen") toggleFullscreen();
    else if (action == "viewScale") {
        m_Preferences->viewScale = value.toString() == "actual" ? "actual" : "fit";
        StreamUtils::setActualSize(m_Preferences->viewScale == "actual");
        if (StreamUtils::actualSize()) {
            m_HaveCaptureOverride = true;
            m_DesiredCapture = m_CaptureBeforeControls = false;
            if (!m_ControlsOpen) m_InputHandler->pauseForControls();
        }
        SDL_Event redraw {}; redraw.type = SDL_WINDOWEVENT;
        redraw.window.windowID = SDL_GetWindowID(m_Window);
        redraw.window.event = SDL_WINDOWEVENT_EXPOSED; SDL_PushEvent(&redraw);
    }
    else if (action == "localText") {
        m_Preferences->localTextInput = value.toBool();
        m_InputHandler->setLocalTextInput(value.toBool());
        updateLocalTextInput();
    }
    else if (action == "clipboardDirection") {
        const int direction = value.toInt();
        if (direction >= 0 && direction <= 3) {
            m_ClipboardDirection = direction;
            m_ClipboardPaused = direction == 0;
            if (m_ClipboardSync) m_ClipboardSync->setDirection(direction);
        }
    }
    else if (action == "keepSafeSettings") keepSafeSettings();
    else if (action == "applyOptimization" && m_Recommendation.createdAt > 0 && !m_StartCancellation.isCancelled()) {
        const auto current = StreamPlanning::Rung {m_ActiveVideoWidth, m_ActiveVideoHeight, m_ActiveVideoFrameRate};
        const auto rungs = recommendationModes(current.fps);
        if (!m_Recommendation.valid(rungs, m_PerformanceContext, m_StreamPlan.codec, QDateTime::currentSecsSinceEpoch())) {
            qWarning("Stream recommendation expired or no longer matches this session; no reconnect requested");
            return;
        }
        m_Adaptation = {};
        m_Adaptation.target = m_Recommendation.target;
        m_Adaptation.baseline = m_Recommendation.baseline;
        m_Adaptation.pending = true;
        m_Adaptation.checkTrial = !m_Recommendation.rollback && m_Recommendation.bitrateKbps == 0;
        m_Adaptation.encoderCeilingKbps = m_Recommendation.bitrateKbps;
        m_Adaptation.restoring = m_Adaptation.locked = m_Recommendation.rollback;
        m_Adaptation.transitionAtMs = LiGetMicroseconds() / 1000;
        // Consume only this explicit action. A retry never inherits takeover consent.
        m_Recommendation = {};
        m_AdjustingForHost = m_RetryableTermination = true;
        closeControls();
        // Let the normal cleanup path stop transport. Interrupting it first can
        // report a termination callback that overwrites the approved handoff.
        SDL_Event quit {}; quit.type = SDL_QUIT; SDL_PushEvent(&quit);
    }
    else if (action == "disconnect") {
        m_RetryableTermination = false;
        m_StartCancellation.cancel();
        closeControls();
        interrupt();
    }
    emit controlsChanged();
}

bool Session::initializeInternal(QWindow* qtWindow)
{
    m_QtWindow = qtWindow;

#ifdef Q_OS_DARWIN
    if (qEnvironmentVariableIntValue("I_WANT_BUGGY_FULLSCREEN") == 0) {
        // If we have a notch and the user specified one of the two native display modes
        // (notched or notchless), override the fullscreen mode to ensure it works as expected.
        // - SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=0 will place the video underneath the notch
        // - SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=1 will place the video below the notch
        bool shouldUseFullScreenSpaces = m_Preferences->windowMode != StreamingPreferences::WM_FULLSCREEN;
        SDL_DisplayMode desktopMode;
        SDL_Rect safeArea;
        for (int displayIndex = 0; StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea); displayIndex++) {
            // Check if this display has a notch (safeArea != desktopMode)
            if (desktopMode.h != safeArea.h || desktopMode.w != safeArea.w) {
                // Check if we're trying to stream at the full native resolution (including notch)
                if (m_Preferences->width == desktopMode.w && m_Preferences->height == desktopMode.h) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Overriding default fullscreen mode for native fullscreen resolution");
                    shouldUseFullScreenSpaces = false;
                    break;
                }
                else if (m_Preferences->width == safeArea.w && m_Preferences->height == safeArea.h) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Overriding default fullscreen mode for native safe area resolution");
                    shouldUseFullScreenSpaces = true;
                    break;
                }
            }
        }

        // Using modesetting on modern versions of macOS is extremely unreliable
        // and leads to hangs, deadlocks, and other nasty stuff. The only time
        // people seem to use it is to get the full screen on notched Macs,
        // which setting SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=1 also accomplishes
        // with much less headache.
        //
        // https://github.com/moonlight-stream/moonlight-qt/issues/973
        // https://github.com/moonlight-stream/moonlight-qt/issues/999
        // https://github.com/moonlight-stream/moonlight-qt/issues/1211
        // https://github.com/moonlight-stream/moonlight-qt/issues/1218
        SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, shouldUseFullScreenSpaces ? "1" : "0");
    }
#endif

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        emit displayLaunchError(tr("Moonwalk could not initialize video on this device. Check the video decoder settings and try again.")
                                + "\n\n" + QString::fromUtf8(SDL_GetError()));
        return false;
    }

    // Stop text input. SDL enables it by default
    // when we initialize the video subsystem, but this
    // causes an IME popup when certain keys are held down
    // on macOS.
    SDL_StopTextInput();

    LiInitializeStreamConfiguration(&m_StreamConfig);
    m_StreamConfig.width = m_Preferences->width;
    m_StreamConfig.height = m_Preferences->height;

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

    // Create a hidden window to use for decoder initialization tests
    SDL_Window* testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        emit displayLaunchError(tr("Moonwalk could not create a video test window. Check the video decoder settings and try again.")
                                + "\n\n" + QString::fromUtf8(SDL_GetError()));
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    qInfo() << "Server GPU:" << m_Computer->gpuModel;
    qInfo() << "Server GFE version:" << m_Computer->gfeVersion;

    LiInitializeVideoCallbacks(&m_VideoCallbacks);
    m_VideoCallbacks.setup = drSetup;

    m_StreamConfig.fps = m_Preferences->fps;
    m_StreamConfig.bitrate = PreferencesWorkflow::effectiveBitrate(m_Preferences->bitrateKbps,
                                                                  m_Preferences->unlockBitrate);
    planStream(testWindow);

    // Frame pacing follows how frames arrive unless it was switched on by hand, or
    // STARLIGHT_FRAME_PACING=0/1 says otherwise for a diagnosis.
    bool pacingOverride;
    if (Utils::getEnvironmentVariableOverride("STARLIGHT_FRAME_PACING", &pacingOverride)) {
        m_PacingMode = pacingOverride ? PacingMode::On : PacingMode::Off;
    }
    else if (const auto pacing = m_Preferences->cliFramePacingOverride()) {
        m_PacingMode = *pacing ? PacingMode::On : PacingMode::Off;
    }
    else if (m_Preferences->framePacing) {
        m_PacingMode = PacingMode::On;
    }
    else {
        // Start where the last stream from this host ended, so a host that sends in
        // bunches is paced from its first frame.
        const bool remembered = QSettings().value(QStringLiteral("automaticpacing/") + m_Computer->uuid).toBool();
        m_PacingMode = PacingMode::Automatic;
        m_AutomaticPacing = AutomaticPacing(m_StreamConfig.fps, remembered);
        m_PacingActive.store(remembered);
    }

#ifndef STEAM_LINK
    // Opt-in to all encryption features if we detect that the platform
    // has AES cryptography acceleration instructions and more than 2 cores.
    if (StreamUtils::hasFastAes() && SDL_GetCPUCount() > 2) {
        m_StreamConfig.encryptionFlags = ENCFLG_ALL;
    }
    else {
        // Enable audio encryption as long as we're not on Steam Link.
        // That hardware can hardly handle Opus decoding at all.
        m_StreamConfig.encryptionFlags = ENCFLG_AUDIO;
    }
#endif

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Video bitrate: %d kbps",
                m_StreamConfig.bitrate);

    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesKey),
               sizeof(m_StreamConfig.remoteInputAesKey));

    // Only the first 4 bytes are populated in the RI key IV
    RAND_bytes(reinterpret_cast<unsigned char*>(m_StreamConfig.remoteInputAesIv), 4);

    switch (m_Preferences->audioConfig)
    {
    case StreamingPreferences::AC_STEREO:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        break;
    case StreamingPreferences::AC_51_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        break;
    case StreamingPreferences::AC_71_SURROUND:
        m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_71_SURROUND;
        break;
    }

    int hostAudioLimit;
    {
        QReadLocker lock(&m_Computer->lock);
        hostAudioLimit = m_Computer->maxAudioChannels;
    }
    const int requestedChannels = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration);
    const int effectiveChannels = AudioLayoutPolicy::effectiveChannels(requestedChannels, hostAudioLimit);
    if (effectiveChannels != requestedChannels) {
        m_StreamConfig.audioConfiguration = effectiveChannels == 6
            ? AUDIO_CONFIGURATION_51_SURROUND : AUDIO_CONFIGURATION_STEREO;
        emitLaunchWarning(tr("Requested surround sound; using %1 because the host's current audio source supports up to %2 channels.")
                          .arg(effectiveChannels == 2 ? tr("stereo") : tr("5.1 surround"))
                          .arg(hostAudioLimit));
    }

    LiInitializeAudioCallbacks(&m_AudioCallbacks);
    m_AudioCallbacks.init = arInit;
    m_AudioCallbacks.cleanup = arCleanup;
    m_AudioCallbacks.decodeAndPlaySample = arDecodeAndPlaySample;
    m_AudioCallbacks.capabilities = getAudioRendererCapabilities(m_StreamConfig.audioConfiguration);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel count: %d",
                CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio channel mask: %X",
                CHANNEL_MASK_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));

    // Start with all codecs and profiles in priority order
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_HIGH10_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_MAIN10);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_REXT10_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_MAIN10);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_HIGH8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_AV1_MAIN8);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265_REXT8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H265);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H264_HIGH8_444);
    m_SupportedVideoFormats.append(VIDEO_FORMAT_H264);

    switch (m_Preferences->videoCodecConfig)
    {
    case StreamingPreferences::VCC_AUTO:
    {
        // Codecs are checked in order of ascending decode complexity to ensure
        // the the deprioritized list prefers lighter codecs for software decoding

        // H.264 is already the lowest priority codec, so we don't need to do
        // any probing for deprioritization for it here.

        auto hevcDA = getDecoderAvailability(testWindow,
                                             m_Preferences->videoDecoderSelection,
                                             m_Preferences->enableYUV444 ?
                                                 (m_Preferences->enableHdr ? VIDEO_FORMAT_H265_REXT10_444 : VIDEO_FORMAT_H265_REXT8_444) :
                                                 (m_Preferences->enableHdr ? VIDEO_FORMAT_H265_MAIN10 : VIDEO_FORMAT_H265),
                                             m_StreamConfig.width,
                                             m_StreamConfig.height,
                                             m_StreamConfig.fps);
        if (hevcDA == DecoderAvailability::None && m_Preferences->enableHdr) {
            // Remove all 10-bit HEVC profiles
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265 & VIDEO_FORMAT_MASK_10BIT);

            // Check if we have 10-bit AV1 support
            auto av1DA = getDecoderAvailability(testWindow,
                                                m_Preferences->videoDecoderSelection,
                                                m_Preferences->enableYUV444 ? VIDEO_FORMAT_AV1_HIGH10_444 : VIDEO_FORMAT_AV1_MAIN10,
                                                m_StreamConfig.width,
                                                m_StreamConfig.height,
                                                m_StreamConfig.fps);
            if (av1DA == DecoderAvailability::None) {
                // Remove all 10-bit AV1 profiles
                m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_AV1 & VIDEO_FORMAT_MASK_10BIT);

                // There are no available 10-bit profiles, so reprobe for 8-bit HEVC
                // and we'll proceed as normal for an SDR streaming scenario.
                SDL_assert(!(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT));
                hevcDA = getDecoderAvailability(testWindow,
                                                m_Preferences->videoDecoderSelection,
                                                m_Preferences->enableYUV444 ? VIDEO_FORMAT_H265_REXT8_444 : VIDEO_FORMAT_H265,
                                                m_StreamConfig.width,
                                                m_StreamConfig.height,
                                                m_StreamConfig.fps);
            }
        }

        if (hevcDA != DecoderAvailability::Hardware) {
            // Deprioritize HEVC unless the user forced software decoding and enabled HDR.
            // We need HEVC in that case because we cannot support 10-bit content with H.264,
            // which would ordinarily be prioritized for software decoding performance.
            if (m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE || !m_Preferences->enableHdr) {
                m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_H265);
            }
        }

        // Deprioritize AV1 unless we can't hardware decode HEVC, and have HDR enabled
        // or we're on Windows or a non-x86 Linux/BSD.
        //
        // Normally, we'd assume hardware that can't decode HEVC definitely can't decode
        // AV1 either, and we wouldn't even bother probing for AV1 support. However, some
        // Windows business systems have HEVC support disabled in firmware from the factory,
        // yet they can still decode AV1 in hardware. To avoid falling back to H.264 on
        // these systems, we don't deprioritize AV1. This firmware-based HEVC licensing
        // behavior seems to be unique to Windows, and Linux on the same system is able
        // to decode HEVC in hardware normally using VAAPI.
        // https://www.reddit.com/r/GeForceNOW/comments/1omsckt/psa_be_wary_of_purchasing_dell_computers_with/
        //
        // Some embedded Linux platforms have incomplete V4L2 decoding support which can
        // lead to unusual cases where a system might support H.264 and AV1 but not HEVC,
        // even if the underlying hardware supports all three. RK3588 is an example of
        // such a SoC. To handle this situation, we will also probe for AV1 if we're on
        // a non-x86 non-macOS UNIX system.
        //
        // We want to keep AV1 at the top of the list for HDR with software decoding
        // because dav1d is higher performance than FFmpeg's HEVC software decoder.
        if (hevcDA == DecoderAvailability::Hardware
#if !defined(Q_OS_WIN32) && (!(defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)) || defined(Q_PROCESSOR_X86))
            || !m_Preferences->enableHdr
#endif
            ) {
            m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_AV1);
        }
        else if (!m_Preferences->enableHdr &&
                   getDecoderAvailability(testWindow,
                                          m_Preferences->videoDecoderSelection,
                                          m_Preferences->enableYUV444 ? VIDEO_FORMAT_AV1_HIGH8_444 : VIDEO_FORMAT_AV1_MAIN8,
                                          m_StreamConfig.width,
                                          m_StreamConfig.height,
                                          m_StreamConfig.fps) != DecoderAvailability::Hardware) {
            m_SupportedVideoFormats.deprioritizeByMask(VIDEO_FORMAT_MASK_AV1);
        }

#ifdef Q_OS_DARWIN
        {
            // Prior to GFE 3.11, GFE did not allow us to constrain
            // the number of reference frames, so we have to fixup the SPS
            // to allow decoding via VideoToolbox on macOS. Since we don't
            // have fixup code for HEVC, just avoid it if GFE is too old.
            QVector<int> gfeVersion = NvHTTP::parseQuad(m_Computer->gfeVersion);
            if (gfeVersion.isEmpty() || // Very old versions don't have GfeVersion at all
                    gfeVersion[0] < 3 ||
                    (gfeVersion[0] == 3 && gfeVersion[1] < 11)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Disabling HEVC on macOS due to old GFE version");
                m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265);
            }
        }
#endif
        // The plan's codec first: the best this computer decodes in hardware that the
        // host sends. The rest stay behind it in case the host turns it down.
        if (m_StreamPlan.codec == QLatin1String("AV1")) {
            m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_AV1);
        }
        else if (m_StreamPlan.codec == QLatin1String("HEVC")) {
            m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_H265);
        }
        break;
    }
    case StreamingPreferences::VCC_FORCE_H264:
        m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H264);
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
    case StreamingPreferences::VCC_FORCE_HEVC_HDR_DEPRECATED:
        m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H265);
        break;
    case StreamingPreferences::VCC_FORCE_AV1:
        // We'll try to fall back to HEVC first if AV1 fails. We'd rather not fall back
        // straight to H.264 if the user asked for AV1 and the host doesn't support it.
        m_SupportedVideoFormats.removeByMask(~(VIDEO_FORMAT_MASK_AV1 | VIDEO_FORMAT_MASK_H265));
        break;
    }

    // NB: Since deprioritization puts codecs in reverse order (at the bottom of the list),
    // we want to deprioritize for the most critical attributes last to ensure they are the
    // lowest priority codecs during server negotiation. Here we do that with YUV 4:4:4 and
    // HDR to ensure we never pick a codec profile that doesn't meet the user's requirement
    // if we can avoid it.

    // Mask off YUV 4:4:4 codecs if the option is not enabled
    if (!m_Preferences->enableYUV444) {
        m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_YUV444);
    }
    else {
        // Deprioritize YUV 4:2:0 codecs if the user wants YUV 4:4:4
        //
        // NB: Since this happens first before deprioritizing HDR, we will
        // pick a YUV 4:4:4 profile instead of a 10-bit profile if they
        // aren't both available together for any codec.
        m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_YUV444);
    }

    // Mask off 10-bit codecs if HDR is not enabled
    if (!m_Preferences->enableHdr) {
        m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
    }
    else {
        // Deprioritize 8-bit codecs if HDR is enabled
        m_SupportedVideoFormats.deprioritizeByMask(~VIDEO_FORMAT_MASK_10BIT);
    }

    switch (m_Preferences->windowMode)
    {
    default:
        // Normally we'd default to fullscreen desktop when starting in windowed
        // mode, but in the case of a slow GPU, we want to use real fullscreen
        // to allow the display to assist with the video scaling work.
        if (WMUtils::isGpuSlow()) {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
            break;
        }
        // Fall-through
    case StreamingPreferences::WM_FULLSCREEN_DESKTOP:
        // Only use full-screen desktop mode if we're running a desktop environment
        if (WMUtils::isRunningDesktopEnvironment()) {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        }
        // Fall-through
    case StreamingPreferences::WM_FULLSCREEN:
#ifdef Q_OS_DARWIN
        if (qEnvironmentVariableIntValue("I_WANT_BUGGY_FULLSCREEN") == 0) {
            // Don't use "real" fullscreen on macOS by default. See comments above.
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else {
            m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
        }
#else
        m_FullScreenFlag = SDL_WINDOW_FULLSCREEN;
#endif
        break;
    }

#if !SDL_VERSION_ATLEAST(2, 0, 11)
    // HACK: Using a full-screen window breaks mouse capture on the Pi's LXDE
    // GUI environment. Force the session to use windowed mode (which won't
    // really matter anyway because the MMAL renderer always draws full-screen).
    if (qgetenv("DESKTOP_SESSION") == "LXDE-pi") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Forcing windowed mode on LXDE-Pi");
        m_FullScreenFlag = 0;
    }
#endif

    // HDR needs a display that shows it as well as a decoder for it.
    if (m_Preferences->enableHdr && (m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) && m_DisplayHdr == 0) {
        emitLaunchWarning(tr("Requested HDR; using SDR because this display can't show HDR."));
        m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
    }

    // Check for validation errors/warnings and emit
    // signals for them, if appropriate
    bool ret = validateLaunch(testWindow);

    if (ret) {
        // Video format is now locked in
        m_StreamConfig.supportedVideoFormats = m_SupportedVideoFormats.front();

        // The bitrate follows the codec actually chosen.
        const int format = m_StreamConfig.supportedVideoFormats;
        const QString codec = (format & VIDEO_FORMAT_MASK_AV1) ? QStringLiteral("AV1")
            : (format & VIDEO_FORMAT_MASK_H265) ? QStringLiteral("HEVC") : QStringLiteral("H.264");
        if (codec != m_StreamPlan.codec) {
            const auto needs = StreamPlanning::budget(codec, m_StreamConfig.width, m_StreamConfig.height, m_StreamConfig.fps);
            m_StreamPlan.codec = codec;
            if (m_Preferences->autoAdjustBitrate) {
                m_StreamPlan.startBitrateKbps = m_StreamConfig.bitrate = needs.startKbps;
                m_StreamPlan.ceilingKbps = needs.ceilingKbps;
                m_StreamPlan.floorKbps = needs.floorKbps;
            }
            else {
                m_StreamPlan.floorKbps = qMin(needs.floorKbps, m_StreamConfig.bitrate);
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stream plan: %s chosen, %d kbps",
                        qPrintable(codec), m_StreamConfig.bitrate);
        }

        // Populate decoder-dependent properties.
        // Must be done after validateLaunch() since m_StreamConfig is finalized.
        ret = populateDecoderProperties(testWindow);
    }

    SDL_DestroyWindow(testWindow);

    if (!ret) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    return true;
}

void Session::emitLaunchWarning(QString text)
{
    if (m_Preferences->configurationWarnings) {
        // Queue this launch warning to be displayed after validation
        m_LaunchWarnings.append(text);
        emit launchWarningsChanged();
    }
}

bool Session::validateLaunch(SDL_Window* testWindow)
{
    if (!m_Computer->isSupportedServerVersion) {
        emit displayLaunchError(tr("This build of Moonwalk cannot connect to the version of GeForce Experience on %1. Check for an update to Moonwalk.").arg(m_Computer->name));
        return false;
    }

    const bool desktopTarget = m_IsDesktopRequest || m_App.isDesktopTarget();
    if (m_Preferences->absoluteMouseMode && !desktopTarget && !m_App.isAppCollectorGame) {
        emitLaunchWarning(tr("Desktop pointer mode can interfere with games that use the mouse to turn a camera."));
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        emitLaunchWarning(tr("Your settings selection to force software decoding may cause poor streaming performance."));
    }

    if (m_SupportedVideoFormats & VIDEO_FORMAT_MASK_AV1) {
        if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_MASK_AV1) == 0) {
            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_AV1) {
                emitLaunchWarning(tr("Your host software or GPU doesn't support encoding AV1."));
            }

            // Moonlight-common-c will handle this case already, but we want
            // to set this explicitly here so we can do our hardware acceleration
            // check below.
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_AV1);
        }
        else {
            if (!m_Preferences->enableHdr && // HDR is checked below
                 m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO && // Force hardware decoding checked below
                 m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO && // Auto VCC is already checked in initialize()
                 getDecoderAvailability(testWindow,
                                        m_Preferences->videoDecoderSelection,
                                        VIDEO_FORMAT_AV1_MAIN8,
                                        m_StreamConfig.width,
                                        m_StreamConfig.height,
                                        m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Using software decoding due to your selection to force AV1 without GPU support. This may cause poor streaming performance."));
            }

            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_AV1) {
                m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_AV1);
            }
        }
    }

    if (m_SupportedVideoFormats & VIDEO_FORMAT_MASK_H265) {
        if (m_Computer->maxLumaPixelsHEVC == 0) {
            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC) {
                emitLaunchWarning(tr("Your host doesn't support encoding HEVC."));
            }

            // Moonlight-common-c will handle this case already, but we want
            // to set this explicitly here so we can do our hardware acceleration
            // check below.
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_H265);
        }
        else {
            if (!m_Preferences->enableHdr && // HDR is checked below
                 m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO && // Force hardware decoding checked below
                 m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO && // Auto VCC is already checked in initialize()
                 getDecoderAvailability(testWindow,
                                        m_Preferences->videoDecoderSelection,
                                        VIDEO_FORMAT_H265,
                                        m_StreamConfig.width,
                                        m_StreamConfig.height,
                                        m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Using software decoding due to your selection to force HEVC without GPU support. This may cause poor streaming performance."));
            }

            if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_HEVC) {
                m_SupportedVideoFormats.removeByMask(~VIDEO_FORMAT_MASK_H265);
            }
        }
    }

    if (!(m_SupportedVideoFormats & ~VIDEO_FORMAT_MASK_H264) &&
            m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_AUTO &&
            getDecoderAvailability(testWindow,
                                   m_Preferences->videoDecoderSelection,
                                   VIDEO_FORMAT_H264,
                                   m_StreamConfig.width,
                                   m_StreamConfig.height,
                                   m_StreamConfig.fps) != DecoderAvailability::Hardware) {

        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_H264) {
            emitLaunchWarning(tr("Using software decoding due to your selection to force H.264 without GPU support. This may cause poor streaming performance."));
        }
        else {
            if (m_Computer->maxLumaPixelsHEVC == 0 &&
                    getDecoderAvailability(testWindow,
                                           m_Preferences->videoDecoderSelection,
                                           VIDEO_FORMAT_H265,
                                           m_StreamConfig.width,
                                           m_StreamConfig.height,
                                           m_StreamConfig.fps) == DecoderAvailability::Hardware) {
                emitLaunchWarning(tr("Your host and this computer don't support the same video codecs. This may cause poor streaming performance."));
            }
            else {
                emitLaunchWarning(tr("Your client GPU doesn't support H.264 decoding. This may cause poor streaming performance."));
            }
        }
    }

    if (m_Preferences->enableHdr) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_FORCE_H264) {
            emitLaunchWarning(tr("Requested HDR; using SDR because H.264 does not support HDR streaming."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
        else if (!(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT)) {
            emitLaunchWarning(tr("Requested HDR; using SDR because this computer cannot decode a supported 10-bit HEVC or AV1 format."));
        }
        // Check that the server GPU supports HDR
        else if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_MASK_10BIT) == 0) {
            emitLaunchWarning(tr("Requested HDR; using SDR because this host does not support HDR streaming."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
        else if (m_Preferences->videoCodecConfig != StreamingPreferences::VCC_AUTO) { // Auto was already checked during init
            bool displayedHdrSoftwareDecodeWarning = false;

            // Check that the available HDR-capable codecs on the client and server are compatible
            if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_AV1_MAIN10)) {
                auto da = getDecoderAvailability(testWindow,
                                                 m_Preferences->videoDecoderSelection,
                                                 VIDEO_FORMAT_AV1_MAIN10,
                                                 m_StreamConfig.width,
                                                 m_StreamConfig.height,
                                                 m_StreamConfig.fps);
                if (da == DecoderAvailability::None) {
                    emitLaunchWarning(tr("This computer's GPU doesn't support AV1 Main10 decoding for HDR streaming."));
                    m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_AV1_MAIN10);
                }
                else if (da == DecoderAvailability::Software &&
                           m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE &&
                           !displayedHdrSoftwareDecodeWarning) {
                    emitLaunchWarning(tr("Using software decoding due to your selection to force HDR without GPU support. This may cause poor streaming performance."));
                    displayedHdrSoftwareDecodeWarning = true;
                }
            }
            if (m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport & SCM_HEVC_MAIN10)) {
                auto da = getDecoderAvailability(testWindow,
                                                 m_Preferences->videoDecoderSelection,
                                                 VIDEO_FORMAT_H265_MAIN10,
                                                 m_StreamConfig.width,
                                                 m_StreamConfig.height,
                                                 m_StreamConfig.fps);
                if (da == DecoderAvailability::None) {
                    emitLaunchWarning(tr("This computer's GPU doesn't support HEVC Main10 decoding for HDR streaming."));
                    m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_H265_MAIN10);
                }
                else if (da == DecoderAvailability::Software &&
                         m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE &&
                         !displayedHdrSoftwareDecodeWarning) {
                    emitLaunchWarning(tr("Using software decoding due to your selection to force HDR without GPU support. This may cause poor streaming performance."));
                    displayedHdrSoftwareDecodeWarning = true;
                }
            }
        }

        // Check for compatibility between server and client codecs
        if ((m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) && // Ignore this check if we already failed one above
            !(m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport) & VIDEO_FORMAT_MASK_10BIT)) {
            emitLaunchWarning(tr("Requested HDR; using SDR because the host and this computer have no compatible HDR video format."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_10BIT);
        }
    }

    if (m_Preferences->enableYUV444) {
        if (!(m_Computer->serverCodecModeSupport & SCM_MASK_YUV444)) {
            emitLaunchWarning(tr("Your host doesn't support YUV 4:4:4 streaming."));
            m_SupportedVideoFormats.removeByMask(VIDEO_FORMAT_MASK_YUV444);
        }
        else {
            m_SupportedVideoFormats.removeByMask(~m_SupportedVideoFormats.maskByServerCodecModes(m_Computer->serverCodecModeSupport));

            if (!m_SupportedVideoFormats.isEmpty() &&
                !(m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444)) {
                emitLaunchWarning(tr("Your host doesn't support YUV 4:4:4 streaming for selected video codec."));
            }
            else if (m_Preferences->videoDecoderSelection != StreamingPreferences::VDS_FORCE_SOFTWARE) {
                while (!m_SupportedVideoFormats.isEmpty() &&
                       (m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444) &&
                       getDecoderAvailability(testWindow,
                                              m_Preferences->videoDecoderSelection,
                                              m_SupportedVideoFormats.front(),
                                              m_StreamConfig.width,
                                              m_StreamConfig.height,
                                              m_StreamConfig.fps) != DecoderAvailability::Hardware) {
                    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE) {
                        m_SupportedVideoFormats.removeFirst();
                    }
                    else {
                        emitLaunchWarning(tr("Using software decoding due to your selection to force YUV 4:4:4 without GPU support. This may cause poor streaming performance."));
                        break;
                    }
                }
                if (!m_SupportedVideoFormats.isEmpty() &&
                    !(m_SupportedVideoFormats.front() & VIDEO_FORMAT_MASK_YUV444)) {
                    emitLaunchWarning(tr("This computer's GPU doesn't support YUV 4:4:4 decoding for selected video codec."));
                }
            }
        }
    }

    if (m_StreamConfig.width >= 3840) {
        // Only allow 4K on GFE 3.x+
        if (m_Computer->gfeVersion.isEmpty() || m_Computer->gfeVersion.startsWith("2.")) {
            emitLaunchWarning(tr("Requested %1x%2; using 1920x1080 for compatibility with this host.")
                              .arg(m_StreamConfig.width).arg(m_StreamConfig.height));

            m_StreamConfig.width = 1920;
            m_StreamConfig.height = 1080;
        }
    }

    // Test if audio works at the specified audio configuration
    bool audioTestPassed = testAudio(m_StreamConfig.audioConfiguration);

    // Gracefully degrade to stereo if surround sound doesn't work
    if (!audioTestPassed && CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration) > 2) {
        audioTestPassed = testAudio(AUDIO_CONFIGURATION_STEREO);
        if (audioTestPassed) {
            m_StreamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            emitLaunchWarning(tr("Requested surround sound; using stereo because the current audio device does not support the selected surround layout."));
        }
    }

    // If nothing worked, warn the user that audio will not work
    if (!audioTestPassed) {
        emitLaunchWarning(tr("Failed to open audio device. Audio will be unavailable during this session."));
    }

    // Check for unmapped gamepads
    if (!SdlInputHandler::getUnmappedGamepads().isEmpty()) {
        emitLaunchWarning(tr("A connected gamepad has no button mapping. Moonwalk can't use that controller."));
    }

    // If we removed all codecs with the checks above, use H.264 as the codec of last resort.
    if (m_SupportedVideoFormats.empty()) {
        m_SupportedVideoFormats.append(VIDEO_FORMAT_H264);
    }

    // NVENC will fail to initialize when any dimension exceeds 4096 using:
    // - H.264 on all versions of NVENC
    // - HEVC prior to Pascal
    //
    // However, if we aren't using Nvidia hosting software, don't assume anything about
    // encoding capabilities by using HEVC Main 10 support. It will likely be wrong.
    if ((m_StreamConfig.width > 4096 || m_StreamConfig.height > 4096) && m_Computer->isNvidiaServerSoftware) {
        // Pascal added support for 8K HEVC encoding support. Maxwell 2 could encode HEVC but only up to 4K.
        // We can't directly identify Pascal, but we can look for HEVC Main10 which was added in the same generation.
        if (m_Computer->maxLumaPixelsHEVC == 0 || !(m_Computer->serverCodecModeSupport & SCM_HEVC_MAIN10)) {
            emit displayLaunchError(tr("Your host's GPU doesn't support streaming video resolutions over 4K."));
            return false;
        }
        else if ((m_SupportedVideoFormats & ~VIDEO_FORMAT_MASK_H264) == 0) {
            emit displayLaunchError(tr("Video resolutions over 4K are not supported by the H.264 codec."));
            return false;
        }
    }

    if (m_Preferences->videoDecoderSelection == StreamingPreferences::VDS_FORCE_HARDWARE &&
            !(m_SupportedVideoFormats & VIDEO_FORMAT_MASK_10BIT) && // HDR was already checked for hardware decode support above
            getDecoderAvailability(testWindow,
                                   m_Preferences->videoDecoderSelection,
                                   m_SupportedVideoFormats.front(),
                                   m_StreamConfig.width,
                                   m_StreamConfig.height,
                                   m_StreamConfig.fps) != DecoderAvailability::Hardware) {
        if (m_Preferences->videoCodecConfig == StreamingPreferences::VCC_AUTO) {
            emit displayLaunchError(tr("Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this computer's GPU."));
        }
        else {
            emit displayLaunchError(tr("Your codec selection and force hardware decoding setting are not compatible. This computer's GPU lacks support for decoding your chosen codec."));
        }

        // Fail the launch, because we won't manage to get a decoder for the actual stream
        return false;
    }

    return true;
}


class DeferredSessionCleanupTask : public QRunnable
{
public:
    DeferredSessionCleanupTask(Session* session) :
        m_Session(session) {}

private:
    virtual ~DeferredSessionCleanupTask() override
    {
        if (m_Session->m_RouteGeneration) {
            m_Session->m_Computer->endStreamingRoute(m_Session->m_RouteGeneration);
            m_Session->m_RouteGeneration = 0;
        }
        // Allow another session to start now that we're cleaned up
        Session::s_ActiveSession = nullptr;
        Session::s_ActiveSessionSemaphore.release();

        // Notify that the session is ready to be cleaned up
        QMetaObject::invokeMethod(m_Session, [session = m_Session] {
            session->connectionCleanupComplete();
        }, Qt::QueuedConnection);
    }

    void run() override
    {
        // End an allocated target on final exit, including cancellation after launch.
        // Recoverable interruptions retain their bounded retry opportunity. Desktop
        // target cancellation releases streaming resources, not the user login.
        const bool desktop = m_Session->m_App.isDesktopTarget();
        const bool shouldQuit =
                m_Session->m_HostTargetMayExist &&
                !m_Session->retryableTermination() &&
                !m_Session->m_AdjustingForHost &&
                (desktop || m_Session->m_Preferences->quitAppAfter);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Host cleanup: app='%s' target=%d allocated=%d retry=%d adjusting=%d desktop=%d end=%d",
                    qPrintable(m_Session->m_App.name),m_Session->m_App.id,int(m_Session->m_HostTargetMayExist),
                    int(m_Session->retryableTermination()),int(m_Session->m_AdjustingForHost.load()),int(desktop),int(shouldQuit));

        // Notify the UI
        if (shouldQuit) {
            emit m_Session->quitStarting();
        }
        else {
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }

        // The video decoder must already be destroyed, since it could
        // try to interact with APIs that can only be called between
        // LiStartConnection() and LiStopConnection().
        SDL_assert(m_Session->m_VideoDecoder == nullptr);

        // Finish cleanup of the connection state
        LiStopConnection();
        StarlightSetAdaptiveFeedbackEnabled(false);
        StarlightSetFrameObserver(nullptr, nullptr);
        delete m_Session->m_AutomaticBitrate;
        m_Session->m_AutomaticBitrate = nullptr;

        // Perform a best-effort app quit
        if (shouldQuit) {
            const QString error = m_Session->endHostTarget();
            if (!error.isEmpty()) {
                qWarning() << error;
                emit m_Session->hostCleanupFailed(error);
            }

            // Session is finished now
            emit m_Session->sessionFinished(m_Session->m_PortTestResults);
        }

        // Exit the entire program if requested
        if (m_Session->m_ShouldExit) {
            QCoreApplication::instance()->quit();
        }
    }

    Session* m_Session;
};

void Session::getWindowDimensions(int& x, int& y,
                                  int& width, int& height)
{
    // Unconditional windowed-mode diagnostic trace (item: windowed-streaming
    // geometry adoption). Previously the only logging here was INSIDE the
    // "windowed + have a Qt window" branch below, so when that branch was
    // skipped -- windowMode wasn't WM_WINDOWED, or m_QtWindow was null (e.g.
    // a CLI-launched stream with no GUI window) -- NOTHING printed at all,
    // making it impossible to tell which of those two happened from the
    // client journal. Log it every time, unconditionally, so a live run
    // always shows: the resolved windowMode preference, whether this session
    // is being treated as full-screen, and whether we have a live Qt window
    // to measure.
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "getWindowDimensions(): windowMode=%d m_IsFullScreen=%s m_QtWindow=%s",
                (int)m_Preferences->windowMode,
                m_IsFullScreen ? "true" : "false",
                m_QtWindow != nullptr ? "valid" : "null");

    int displayIndex = 0;

    if (m_Window != nullptr) {
        displayIndex = SDL_GetWindowDisplayIndex(m_Window);
        SDL_assert(displayIndex >= 0);
    }
    // Create our window on the same display that Qt's UI
    // was being displayed on.
    else {
        Q_ASSERT(m_QtWindow != nullptr);
        if (m_QtWindow != nullptr) {
            QScreen* screen = m_QtWindow->screen();
            if (screen != nullptr) {
                QRect displayRect = screen->geometry();

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Qt UI screen is at (%d,%d)",
                            displayRect.x(), displayRect.y());
                for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
                    SDL_Rect displayBounds;

                    if (SDL_GetDisplayBounds(i, &displayBounds) == 0) {
                        if (displayBounds.x == displayRect.x() &&
                            displayBounds.y == displayRect.y()) {
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                        "SDL found matching display %d",
                                        i);
                            displayIndex = i;
                            break;
                        }
                    }
                    else {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "SDL_GetDisplayBounds(%d) failed: %s",
                                    i, SDL_GetError());
                    }
                }
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Qt window is not associated with a QScreen!");
            }
        }
    }

    // Windowed-mode geometry continuity (item 5): when this session started in
    // windowed mode (not full-screen/borderless) and we have a live Qt launcher
    // window to measure, size and position the SDL stream window to EXACTLY
    // match the launcher window's current client-area geometry. This makes
    // connecting feel like the launcher window itself is transforming into the
    // stream, instead of popping open a differently-sized window centered on
    // the display.
    //
    // QWindow::x()/y()/width()/height() are in Qt's device-independent
    // ("logical") pixels. This is our best FIRST GUESS at the SDL geometry to
    // request -- it is exactly right on X11 (SDL's X11 backend has no scale
    // concept of its own; SDL_WINDOW_ALLOW_HIGHDPI only changes what
    // SDL_GL_GetDrawableSize()/SDL_GetWindowSizeInPixels() report, not the
    // window's screen-space size) and on macOS (both Qt and SDL treat window
    // geometry as points, scaled by the same system backing-store factor).
    //
    // On Wayland it is NOT reliably right, which is what the previous version
    // of this comment got wrong. Whether SDL's Wayland backend expresses
    // window geometry in the same logical-point space Qt uses depends on
    // whether the linked SDL negotiates wp-fractional-scale-v1/wp-viewporter
    // with the compositor (version- and build-dependent) versus falling back
    // to the legacy integer wl_output scale -- and KDE/KWin's own advertised
    // integer wl_output.scale doesn't always agree with its precise fractional
    // scale either. We can't determine which convention is in effect just by
    // asking Qt or SDL in isolation, so we don't try to guess a static
    // conversion factor here. Instead, after SDL_CreateWindow() actually
    // creates the window (see exec()), we MEASURE the pixel size SDL realized
    // for this same request and correct it in a second pass if it disagrees
    // with Qt's devicePixelRatio() -- see the "HiDPI correction pass" block in
    // exec() for the derivation. That keeps this function itself simple: it
    // only ever hands out Qt's logical geometry as the starting point.
    //
    // Also benefits the macOS exclusive-fullscreen recovery path in
    // toggleFullscreen() (which calls back into this function), since
    // m_IsFullScreen reflects how the session STARTED, not its current runtime
    // fullscreen state -- a windowed-start session that toggled into fullscreen
    // and back will land on the launcher's original geometry there too, which
    // is the same "continuity" intent.
    if (!m_IsFullScreen && m_QtWindow != nullptr &&
            m_QtWindow->width() > 0 && m_QtWindow->height() > 0) {
        x = m_QtWindow->x();
        y = m_QtWindow->y();
        width = m_QtWindow->width();
        height = m_QtWindow->height();

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "getWindowDimensions(): adopting Qt launcher geometry (%d,%d %dx%d)",
                    x, y, width, height);
        return;
    }

    // Unconditional: explains WHY the Qt-continuity branch above was skipped
    // whenever it is, so a live run never leaves this a silent mystery.
    if (m_IsFullScreen) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "getWindowDimensions(): not adopting launcher geometry -- session is full-screen "
                    "(windowMode=%d, isRunningDesktopEnvironment=%s)",
                    (int)m_Preferences->windowMode,
                    WMUtils::isRunningDesktopEnvironment() ? "true" : "false");
    }
    else if (m_QtWindow == nullptr) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "getWindowDimensions(): not adopting launcher geometry -- no Qt window was "
                    "passed to Session::initialize() (headless/CLI launch?)");
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "getWindowDimensions(): not adopting launcher geometry -- Qt window reported "
                    "non-positive size (%dx%d)",
                    m_QtWindow->width(), m_QtWindow->height());
    }

    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
        const auto floating = WindowGeometryPolicy::initialWindowedSize(
            QSize(m_StreamConfig.width, m_StreamConfig.height), QSize(usableBounds.w, usableBounds.h));
        width = floating.width();
        height = floating.height();
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetDisplayUsableBounds() failed: %s",
                     SDL_GetError());

        width = m_StreamConfig.width;
        height = m_StreamConfig.height;
    }

    x = y = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "getWindowDimensions(): using display-centered fallback geometry (%d,%d %dx%d) on display %d",
                x, y, width, height, displayIndex);
}

void Session::updateOptimalWindowDisplayMode()
{
    SDL_DisplayMode desktopMode, bestMode, mode;
    int displayIndex = SDL_GetWindowDisplayIndex(m_Window);

    // Try the current display mode first. On macOS, this will be the normal
    // scaled desktop resolution setting.
    if (SDL_GetDesktopDisplayMode(displayIndex, &desktopMode) == 0) {
        // If this doesn't fit the selected resolution, use the native
        // resolution of the panel (unscaled).
        if (desktopMode.w < m_ActiveVideoWidth || desktopMode.h < m_ActiveVideoHeight) {
            SDL_Rect safeArea;
            if (!StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea)) {
                return;
            }
        }
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_GetDesktopDisplayMode() failed: %s",
                    SDL_GetError());
        return;
    }

    // On devices with slow GPUs, we will try to match the display mode
    // to the video stream to offload the scaling work to the display.
    //
    // We also try to match the video resolution if we're using KMSDRM,
    // because scaling on the display is generally higher quality than
    // scaling performed by drmModeSetPlane().
    bool matchVideo;
    if (!Utils::getEnvironmentVariableOverride("MATCH_DISPLAY_MODE_TO_VIDEO", &matchVideo)) {
        matchVideo = WMUtils::isGpuSlow() || QString(SDL_GetCurrentVideoDriver()) == "KMSDRM";
    }

    bestMode = desktopMode;
    bestMode.refresh_rate = 0;
    if (!matchVideo) {
        // Start with the native desktop resolution and try to find
        // the highest refresh rate that our stream FPS evenly divides.
        int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
        for (int i = 0; i < numDisplayModes; i++) {
            if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                if (mode.w == desktopMode.w && mode.h == desktopMode.h &&
                    mode.refresh_rate % m_StreamConfig.fps == 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Found display mode with desktop resolution: %dx%dx%d",
                                mode.w, mode.h, mode.refresh_rate);
                    if (mode.refresh_rate > bestMode.refresh_rate) {
                        bestMode = mode;
                    }
                }
            }
        }
    }

    // If we didn't find a mode that matched the current resolution and
    // had a high enough refresh rate, start looking for lower resolution
    // modes that can meet the required refresh rate and minimum video
    // resolution. We will also try to pick a display mode that matches
    // aspect ratio closest to the video stream.
    if (bestMode.refresh_rate == 0) {
        float bestModeAspectRatio = 0;
        float videoAspectRatio = (float)m_ActiveVideoWidth / (float)m_ActiveVideoHeight;
        int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
        for (int i = 0; i < numDisplayModes; i++) {
            if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                float modeAspectRatio = (float)mode.w / (float)mode.h;
                if (mode.w >= m_ActiveVideoWidth && mode.h >= m_ActiveVideoHeight &&
                        mode.refresh_rate % m_StreamConfig.fps == 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Found display mode with video resolution: %dx%dx%d",
                                mode.w, mode.h, mode.refresh_rate);
                    if (mode.refresh_rate >= bestMode.refresh_rate &&
                            (bestModeAspectRatio == 0 || fabs(videoAspectRatio - modeAspectRatio) <= fabs(videoAspectRatio - bestModeAspectRatio))) {
                        bestMode = mode;
                        bestModeAspectRatio = modeAspectRatio;
                    }
                }
            }
        }
    }

    if (bestMode.refresh_rate == 0) {
        // We may find no match if the user has moved a 120 FPS
        // stream onto a 60 Hz monitor (since no refresh rate can
        // divide our FPS setting). We'll stick to the default in
        // this case.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No matching display mode found; using desktop mode");
        bestMode = desktopMode;
    }

    if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        // Only print when the window is actually in full-screen exclusive mode,
        // otherwise we're not actually using the mode we've set here
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Chosen best display mode: %dx%dx%d",
                    bestMode.w, bestMode.h, bestMode.refresh_rate);
    }

    SDL_SetWindowDisplayMode(m_Window, &bestMode);
}

void Session::toggleFullscreen()
{
    m_InputHandler->releaseAllMouseButtons();
    bool fullScreen = !(SDL_GetWindowFlags(m_Window) & m_FullScreenFlag);

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN)
    // Destroy the video decoder before toggling full-screen because D3D9 can try
    // to put the window back into full-screen before we've managed to destroy
    // the renderer. This leads to excessive flickering and can cause the window
    // decorations to get messed up as SDL and D3D9 fight over the window style.
    //
    // On Apple Silicon Macs, the AVSampleBufferDisplayLayer may cause WindowServer
    // to deadlock when transitioning out of fullscreen. Destroy the decoder before
    // exiting fullscreen as a workaround. See issue #973.
    SDL_LockMutex(m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_UnlockMutex(m_DecoderLock);
#else
    // Compositors only choose a position when a window appears. One that appeared
    // fullscreen has nowhere to return to, and lands in the screen's corner: under
    // GNOME's top bar, or any panel there. It has to appear afresh to be placed, and
    // nothing may draw meanwhile: a frame committed to a surface between its roles is
    // a Wayland protocol error, and the compositor disconnects us. The shown event
    // below brings the decoder back.
    const bool placeAfresh = !fullScreen && m_WindowNeedsPlacement;
    if (placeAfresh) {
        SDL_LockMutex(m_DecoderLock);
        delete m_VideoDecoder;
        m_VideoDecoder = nullptr;
        SDL_UnlockMutex(m_DecoderLock);
    }
#endif

    // Actually enter/leave fullscreen
    SDL_SetWindowFullscreen(m_Window, fullScreen ? m_FullScreenFlag : 0);
#if !defined(Q_OS_WIN32) && !defined(Q_OS_DARWIN)
    if (placeAfresh) {
        m_WindowNeedsPlacement = false;
        SDL_HideWindow(m_Window);
        SDL_ShowWindow(m_Window);
        m_RefreshTitleAt = SDL_GetTicks() + 300;
    }
#endif
    m_Preferences->windowMode = (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) ?
            (m_FullScreenFlag == SDL_WINDOW_FULLSCREEN ? StreamingPreferences::WM_FULLSCREEN : StreamingPreferences::WM_FULLSCREEN_DESKTOP) :
            StreamingPreferences::WM_WINDOWED;

#ifdef Q_OS_DARWIN
    // SDL on macOS has a bug that causes the window size to be reset to crazy
    // large dimensions when exiting out of true fullscreen mode. We can work
    // around the issue by manually resetting the position and size here.
    if (!fullScreen && m_FullScreenFlag == SDL_WINDOW_FULLSCREEN) {
        int x, y, width, height;
        getWindowDimensions(x, y, width, height);
        SDL_SetWindowSize(m_Window, width, height);
        SDL_SetWindowPosition(m_Window, x, y);
    }
#endif

    // Input handler might need to start/stop keyboard grab after changing modes
    m_InputHandler->updateKeyboardGrabState();

    // Input handler might need stop/stop mouse grab after changing modes
    m_InputHandler->updatePointerRegionLock();
}

void Session::notifyMouseEmulationMode(bool enabled)
{
    m_MouseEmulationRefCount += enabled ? 1 : -1;
    SDL_assert(m_MouseEmulationRefCount >= 0);

    m_StreamStatus.setGamepadMouse(m_MouseEmulationRefCount > 0);
}

void Session::refreshStreamStatus()
{
    int width = 0;
    SDL_GetWindowSize(m_Window, &width, nullptr);
    if (width>0) {
        int pixels=width;
#if SDL_VERSION_ATLEAST(2,26,0)
        SDL_GetWindowSizeInPixels(m_Window,&pixels,nullptr);
#endif
        // SDL's units can be physical pixels or points; the notice is measured in points.
        const auto points=StreamToolbar::metrics(width,pixels,m_LocalDisplayScale);
        m_OverlayManager.setDisplayWidth(points.width,points.pixelRatio);
    }
    const auto status = m_StreamStatus.snapshot(SDL_GetTicks(), m_AudioMuted);
    QString textInput = m_CompositionText;
    if (!m_TextInputNotice.isEmpty() && Uint32(SDL_GetTicks() - m_TextInputNoticeAt) < 4000)
        textInput += "\n" + m_TextInputNotice;
    QString hostName;
    { QReadLocker lock(&m_Computer->lock); hostName = m_Computer->name; }
    const QString sessionNotice = !m_HasVideoFrame.load() || (m_CaptureHealthSupported && !sourceHealthy())
        ? tr("Preparing the remote desktop…") : !m_RemoteInputAvailable.load()
        ? (m_CaptureHealthSupported ? tr("Viewing only. Allow control in Moonwalk Host on the Mac.")
                                    : tr("Viewing only. %1 hasn't allowed this device to control it.").arg(hostName)) :
          (!m_InputHandler->isCaptureActive() ? (StreamUtils::actualSize()
          ? tr("Input released. Scroll to move around; click the picture to control %1.").arg(hostName)
          : tr("Input released. Click the picture to control %1.").arg(hostName)) : QString());
    textInput += sessionNotice;
    if (status == m_RenderedStreamStatus && textInput == m_RenderedCompositionText) return;
    m_RenderedStreamStatus = status;
    m_RenderedCompositionText = textInput;
    QStringList lines;
    if (!sessionNotice.isEmpty()) lines << sessionNotice;
    if (!m_CompositionText.isEmpty()) lines << tr("Composing: %1").arg(m_CompositionText);
    if (!m_TextInputNotice.isEmpty() && Uint32(SDL_GetTicks() - m_TextInputNoticeAt) < 4000)
        lines << m_TextInputNotice;
    if (status.audioUnavailable)
        lines << tr("Audio output is unavailable. Moonwalk is trying to reconnect it.");
    if (status.network == StreamStatus::Network::Slow)
        lines << tr("Slow connection to host. Reduce your bitrate.");
    else if (status.network == StreamStatus::Network::Poor)
        lines << tr("Poor connection to host.");
    if (status.gamepadMouse)
        lines << tr("Gamepad mouse mode active. Long press Start to deactivate.");
    switch (status.notice) {
    case StreamStatus::Notice::ClipboardEmpty:
        lines << tr("Type Clipboard: the clipboard has no text.");
        break;
    case StreamStatus::Notice::ClipboardTooLarge:
        lines << tr("Type Clipboard: text exceeds the 1 MiB limit. Nothing was typed.");
        break;
    case StreamStatus::Notice::ClipboardQueueFailed:
        lines << tr("Could not queue clipboard text for typing. Try again.");
        break;
    case StreamStatus::Notice::None:
        break;
    }
    const QByteArray text = lines.join(QLatin1Char('\n')).toUtf8();
    m_OverlayManager.updateOverlayText(Overlay::OverlayStatusUpdate, text.constData());
    m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, !lines.isEmpty());
}

void Session::warnCursorReconnectRequired()
{
    if (m_CursorReconnectWarningShown) return;
    m_CursorReconnectWarningShown = true;
    emitLaunchWarning(tr("The host includes its cursor in the picture. Choose Desktop pointer mode and Local cursor, then reconnect to draw a local cursor."));
}

void Session::notifyMouseModeChanged(bool absolute)
{
    m_Preferences->absoluteMouseMode = absolute;
    if (m_CursorSync) {
        m_CursorSync->setFocused(!m_ControlsOpen && m_CursorPolicy.syncShapes(absolute) &&
                                (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS));
        if (!absolute) m_CursorSync->resetCursor();
    }
    if (m_CursorPolicy.needsReconnect(absolute)) warnCursorReconnectRequired();
}

class AsyncConnectionStartThread : public QThread
{
public:
    AsyncConnectionStartThread(Session* session) :
        QThread(nullptr),
        m_Session(session)
    {
        setObjectName("Async Conn Start");
    }

    void run() override
    {
        m_Session->m_AsyncConnectionSuccess = m_Session->startConnectionAsync();
    }

    Session* m_Session;
};

// Called in a non-main thread
bool Session::startConnectionAsync()
{
    bool adaptiveFeedbackNegotiated = false;
    bool verifyDesktopOwnership = false;
    if (m_StartCancellation.isCancelled()) return false;
    traceStartup("select-route");
    m_RouteGeneration = m_Computer->beginStreamingRoute(m_PreferredAddress);
    // Race known routes with fresh discovery on this connection thread.
    // A reachable route gets a short grace for faster peers, without waiting
    // for dead addresses or slow DNS. Deliberate route choices take priority.
    // Pin the result before constructing NvHTTP for serverinfo/launch.
    bool haveAddressOverride;
    {
        QReadLocker readLocker(&m_Computer->lock);
        haveAddressOverride = !m_Computer->addressOverride.isNull();
    }
    if (!haveAddressOverride) {
        // probeFastestAddress() does its own locking internally (via
        // uniqueAddresses()) and only touches sockets, not shared state, so
        // it's safe to call without holding m_Computer->lock here.
        NvAddress fastestAddress = m_Computer->probeFastestAddress([this] {
            return m_StartCancellation.isCancelled();
        });
        if (!fastestAddress.isNull()) {
            m_Computer->pinStreamingRoute(m_RouteGeneration, fastestAddress);
        }
    }

    if (m_StartCancellation.isCancelled()) return false;

    traceStartup("route-selected");
    { QReadLocker lock(&m_Computer->lock);
      m_PerformanceContext = m_PerformanceHostContext + QLatin1Char('/') + m_Computer->activeAddress.toString() + m_ViewingContext; }

    // Recheck the live target below. A poll/dialog snapshot may already be stale.

    bool enableGameOptimizations;
    if (m_Computer->isNvidiaServerSoftware) {
        // GFE will set all settings to 720p60 if it doesn't recognize
        // the chosen resolution. Avoid that by disabling SOPS when it
        // is not streaming a supported resolution.
        enableGameOptimizations = false;
        for (const NvDisplayMode &mode : std::as_const(m_Computer->displayModes)) {
            if (mode.width == m_StreamConfig.width &&
                    mode.height == m_StreamConfig.height) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Found host supported resolution: %dx%d",
                            mode.width, mode.height);
                enableGameOptimizations = m_Preferences->gameOptimizations;
                break;
            }
        }
    }
    else {
        // Always send SOPS to Sunshine because we may repurpose the
        // option to control whether the display mode is adjusted
        enableGameOptimizations = m_Preferences->gameOptimizations;
    }

    QString rtspSessionUrl;

    try {
        NvHTTP http(m_Computer);
        http.setCancelToken(&m_StartCancellation);
        traceStartup("verify-host");
        QString fresh = http.getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR, true);
        if (m_IsDesktopRequest) {
            const bool takeoverApproved = m_DesktopTakeoverApproved;
            m_DesktopTakeoverApproved = false;
            if (NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid) {
                emit displayLaunchError(tr("The host's identity changed before takeover. Check the pairing before connecting again."));
                return false;
            }
            auto apps = http.getAppList();
            const auto caps = HostCapabilities::parse(fresh);
            if (m_Preferences->displayTarget == "virtual" && (!caps.virtualDisplay || !caps.virtualDisplayReady)) {
                emit displayLaunchError(tr("The virtual display driver is unavailable. Moonwalk stopped before taking over the session."));
                return false;
            }
            // Connect means the existing desktop, not an additional monitor.
            // Release only our retained auxiliary role before resolving it.
            if (NvApp::ownsRemoteMonitor(apps)) {
                http.disconnectRemoteMonitor();
                m_HostTargetMayExist = false;
                fresh = http.getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR);
                if (NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid) {
                    emit displayLaunchError(tr("The host's identity changed before launch. Check the pairing before connecting again."));
                    return false;
                }
                apps = http.getAppList();
            }
            if (NvApp::hasForeignDesktop(apps)) {
                if (!takeoverApproved) {
                    emit displayLaunchError(tr("Another device started this Desktop session. Return to the host page to review the takeover prompt."));
                    return false;
                }
                http.takeOverRetainedDesktop(m_Computer->uuid, NvApp::desktopTarget(apps, false));
                m_HostTargetMayExist = false;
                fresh = http.getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR);
                if (NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid) {
                    emit displayLaunchError(tr("The host's identity changed during takeover. Check the pairing before trying to open Desktop again."));
                    return false;
                }
                apps = http.getAppList();
                if (NvApp::hasForeignDesktop(apps)) {
                    emit displayLaunchError(tr("The previous Desktop session is still running. End it before opening a new one."));
                    return false;
                }
            }
            const auto target = NvApp::desktopTarget(apps, false);
            if (!target.isInitialized()) {
                emit displayLaunchError(tr("The requested Desktop session is no longer available."));
                return false;
            }
            m_App = target;
            verifyDesktopOwnership = NvApp::catalogueRole(apps, 5).isInitialized() ||
                                     NvApp::catalogueRole(apps, 4).isInitialized();
            qInfo("Desktop target: app=%d role=%s display=%s", m_App.id,
                  m_App.isRemoteMonitor() ? "private-monitor" : "application",
                  qPrintable(m_Preferences->displayTarget));
        }
        traceStartup("host-verified");
        m_CaptureHealthSupported = !NvHTTP::getXmlString(fresh, "CaptureGeneration").isEmpty();
        m_CaptureBaseline = NvHTTP::getXmlString(fresh, "CaptureGeneration").toULongLong();
        const auto captureState=NvHTTP::getXmlString(fresh,"CaptureState");
        m_CaptureBaselineActive=NvHTTP::getXmlString(fresh,"ActiveStreams").toInt()>0 &&
            NvHTTP::getXmlString(fresh,"CaptureFrames").toULongLong()>0 &&
            (captureState=="complete" || captureState=="idle");
        m_RemoteInputAvailable = NvHTTP::getXmlString(fresh, "AccessibilityGranted") != "0" &&
            (NvHTTP::getXmlString(fresh, "KeyboardEnabled") != "0" || NvHTTP::getXmlString(fresh, "MouseEnabled") != "0");
        m_ImageClipboardSupported = NvHTTP::getXmlString(fresh, "ClipboardImageSupported") == "1";
        m_FileTransferSupported = NvHTTP::getXmlString(fresh, "FileTransferSupported") == "1";
        m_HostCapabilities = HostCapabilities::parse(fresh);
        m_RemoteInputAvailable = m_HostCapabilities.allows(HostCapabilities::Mouse) ||
                                m_HostCapabilities.allows(HostCapabilities::Keyboard);
        if (m_Preferences->displayTarget == "virtual" &&
                (!m_HostCapabilities.virtualDisplay || !m_HostCapabilities.virtualDisplayReady)) {
            emit displayLaunchError(tr("The host's virtual display driver is unavailable. Choose Use host setting or repair the virtual display driver on the host."));
            return false;
        }
        const int runningApp = NvHTTP::getCurrentGame(fresh);
        const bool remoteMonitor = m_App.isRemoteMonitor();
        const bool sameRunningTarget = m_App.matchesRunningTarget(runningApp, NvHTTP::getXmlString(fresh, "currentgameuuid"));
        if (remoteMonitor) {
            const auto supported = NvApp::desktopTarget(http.getAppList(), true);
            if (!m_HostCapabilities.virtualDisplay || !m_HostCapabilities.virtualDisplayReady || !supported.isRemoteMonitor()) {
                emit displayLaunchError(tr("The host no longer offers this separate display. Reconnect from the host page."));
                return false;
            }
        }
        const bool desktopTarget = m_App.isDesktopTarget();
        if ((!remoteMonitor && m_ResumeOnly && !sameRunningTarget && !(desktopTarget && runningApp == 0)) ||
                (!remoteMonitor && runningApp != 0 && !sameRunningTarget) ||
                NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid) {
            m_RetryableTermination = false;
            emit displayLaunchError(tr("The session changed before Moonwalk could connect. Go back to the host page to choose what to open."));
            return false;
        }
        // Same immutable effective contract as input visibility and shape sync,
        // including the legacy local-cursor environment override.
        bool clientSideCursor = m_CursorPolicy.clientSideCursor();
        // A host that takes direct reports hears from Automatic bitrate on the control
        // stream. One that only knows the older protocol gets loss reports instead.
        const bool directReports = NvHTTP::getXmlString(fresh, "AutomaticBitrateTransport") == "1";
        adaptiveFeedbackNegotiated = m_Preferences->adaptiveBitrate && !directReports &&
                                     NvHTTP::getXmlString(fresh, "AdaptiveBitrateSupported") == "1";
        HostCapacity capacity = HostCapacity::load(m_Computer->uuid);
        // A pause observed on another build, codec or route is not evidence for this one.
        { QReadLocker lock(&m_Computer->lock);
          m_BitrateContext = QString::fromLatin1(QCryptographicHash::hash(
              (m_Computer->serverSoftwareLabel() + m_Computer->hostSoftwareRevision + m_Computer->activeAddress.toString() + m_StreamPlan.codec +
               QString::number(m_StreamConfig.width) + QLatin1Char('x') + QString::number(m_StreamConfig.height) +
               QLatin1Char('@') + QString::number(m_StreamConfig.fps) + QString::number(directReports)).toUtf8(),
              QCryptographicHash::Sha256).toHex());
          m_BitrateCostContext = QString::fromLatin1(QCryptographicHash::hash(
              (m_Computer->serverSoftwareLabel() + QLatin1Char('/') + m_Computer->hostSoftwareRevision + QLatin1Char('/') +
               m_Computer->activeAddress.toString() + QLatin1Char('/') + m_StreamPlan.codec + QLatin1Char('/') +
               QString::number(directReports)).toUtf8(), QCryptographicHash::Sha256).toHex()); }
        const auto age = QDateTime::currentSecsSinceEpoch() - capacity.bitrateMeasuredAt;
        const bool costKnown = (capacity.bitrateContext == m_BitrateCostContext || capacity.bitrateContext == m_BitrateContext)
            && age >= 0 && age <= StreamPlanning::SampleLifetimeSeconds;
        m_BitrateChange = costKnown ? capacity.bitrateChange
            : directReports ? HostCapacity::BitrateChange::Live : HostCapacity::BitrateChange::Unknown;
        m_BitrateRebuilt = m_BitrateChange == HostCapacity::BitrateChange::Rebuild;
        m_BitrateChangeStallMs = costKnown ? capacity.bitrateChangeStallMs : 0;
        if (m_Preferences->adaptiveBitrate && m_Preferences->autoAdjustBitrate && (directReports || m_HostCapabilities.apollo)) {
            // The host's first rate is its own share of what we ask for, after FEC and audio.
            // Over HTTP the controller counts in what /bitrate sets: the encoder's rate.
            const int configured = m_StreamPlan.ceilingKbps > 0 ? m_StreamPlan.ceilingKbps : m_StreamConfig.bitrate;
            const int channels = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration);
            const int configuredCeiling = qMax(500, RuntimeBitratePolicy::encoderKbps(configured, channels));
            const int ceiling = m_Adaptation.encoderCeilingKbps > 0
                ? qMin(configuredCeiling,m_Adaptation.encoderCeilingKbps) : configuredCeiling;
            const int remembered = StreamingPreferences::rememberedAutomaticBitrate(m_Computer->uuid, m_BitrateContext);
            const int typical = RuntimeBitratePolicy::encoderKbps(m_StreamPlan.startBitrateKbps, channels);
            const int floor = qBound(500, RuntimeBitratePolicy::encoderKbps(m_StreamPlan.floorKbps, channels), ceiling);
            const int startFloor = remembered > 0 ? qMin(floor, qMax(500, remembered)) : floor;
            const int start = m_ConnectionRecovery && m_RecoveryBitrateKbps > 0
                ? qBound(500, RuntimeBitratePolicy::encoderKbps(m_RecoveryBitrateKbps, channels), ceiling)
                : qBound(startFloor, remembered ? remembered * 11 / 10 : typical, ceiling);
            m_StreamConfig.bitrate = qMin(configured, RuntimeBitratePolicy::wireKbps(start, channels));
            m_AutomaticBitrateKbps.store(RuntimeBitratePolicy::encoderKbps(m_StreamConfig.bitrate, channels));
            delete m_AutomaticBitrate;
            m_AutomaticBitrate = new AutomaticBitrateDriver(start, ceiling, m_StreamConfig.fps, floor, directReports,
                [this, directReports, channels](const AutomaticBitrate::Decision& decision) {
                    const auto change = m_BitrateChange.load();
                    const bool mayApply = AutomaticBitrate::mayApplyLiveTarget(directReports,
                        change == HostCapacity::BitrateChange::Live, change == HostCapacity::BitrateChange::Rebuild);
                    if (directReports) {
                        auto report = decision;
                        if (mayApply) m_AutomaticBitrateKbps.store(decision.targetKbps);
                        else report.targetKbps = m_AutomaticBitrateKbps.load();
                        const auto bytes = AutomaticBitrateDriver::report(report);
                        StarlightSendControlMessage(AutomaticBitrateDriver::ReportType, bytes.constData(), quint16(bytes.size()));
                        return;
                    }
                    // Over HTTP, and the host may rebuild its encoder for each change.
                    if (!m_RuntimeBitrate || !m_RuntimeBitrateSupported ||
                        !mayApply) return;
                    const int runtimeKbps = m_RuntimeBitrateKbps.load();
                    const int current = RuntimeBitratePolicy::encoderKbps(
                        runtimeKbps > 0 ? runtimeKbps : m_StreamConfig.bitrate, channels);
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (AutomaticBitrate::worthAsking(decision.targetKbps, current,
                                                      uint64_t(now - m_RuntimeRequestedAt) * 1000, m_HasVideoFrame.load())) {
                        m_RuntimeRequestedAt = now;
                        m_RuntimeBitrate->requestBitrate(RuntimeBitratePolicy::wireKbps(decision.targetKbps, channels));
                    }
                }, m_BitrateRebuilt);
            m_AutomaticBitrate->controller().setSetOnce(!AutomaticBitrate::continuousChanges(
                directReports, m_BitrateChange == HostCapacity::BitrateChange::Live));
            connect(m_AutomaticBitrate, &AutomaticBitrateDriver::statusChanged, this, [this] { emit controlsChanged(); });
            // Kept for this host's next stream, and for the in-stream bitrate control.
            connect(m_AutomaticBitrate, &AutomaticBitrateDriver::changeMeasured, this, [this](int stallMs, bool keyframe) {
                const bool rebuilt = stallMs >= AutomaticBitrate::RebuildStallMs;
                m_BitrateRebuilt = m_BitrateRebuilt || rebuilt;
                m_BitrateChangeStallMs = qMax(m_BitrateChangeStallMs, stallMs);
                m_BitrateChange = m_BitrateRebuilt ? HostCapacity::BitrateChange::Rebuild : HostCapacity::BitrateChange::Live;
                m_Adaptation.interruptedMs += stallMs;
                HostCapacity capacity = HostCapacity::load(m_Computer->uuid);
                capacity.bitrateChange = m_BitrateChange;
                capacity.bitrateContext = m_BitrateCostContext;
                capacity.bitrateMeasuredAt = QDateTime::currentSecsSinceEpoch();
                capacity.bitrateChangeStallMs = m_BitrateChangeStallMs;
                capacity.save(m_Computer->uuid);
                qInfo("Automatic bitrate: video stopped for %d ms after a change%s; %s", stallMs,
                      keyframe ? " and came back with a keyframe" : "",
                      m_BitrateRebuilt ? "video paused after a quality change; limiting live changes"
                                       : "no long pause was observed after this quality change");
                emit controlsChanged();
            });
        }
        else if (m_Preferences->autoAdjustBitrate && !m_SafeSettingsTrial && !m_ConnectionRecovery) {
            // Hosts without live bitrate control still accept an ordinary
            // launch bitrate. Keep these measurements separate from live ones.
            m_StartupBitrateContext = m_BitrateContext + QStringLiteral("/startup");
            const int channels = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration);
            m_NormalStartupBitrateKbps = RuntimeBitratePolicy::encoderKbps(m_StreamConfig.bitrate, channels);
            const int remembered = StreamingPreferences::rememberedAutomaticBitrate(m_Computer->uuid, m_StartupBitrateContext);
            m_StartupBitrateKnown = remembered > 0;
            const int trial = m_Adaptation.pending ? m_Adaptation.encoderCeilingKbps : 0;
            const int target = trial > 0 ? trial : remembered;
            if (target >= 500 && target < m_NormalStartupBitrateKbps) {
                m_StreamConfig.bitrate = RuntimeBitratePolicy::wireKbps(target, channels);
                if (trial > 0) m_Adaptation.checkTrial = true;
            }
        }
        traceStartup("prepare-host");
        // A cancelled or lost HTTP reply does not prove the host skipped launch.
        // Retain cleanup responsibility before sending; cleanup rechecks host and
        // target identity and refuses to cancel an active or changed target.
        m_HostTargetMayExist = true;
        QString hostWarning;
        http.startApp(!remoteMonitor && runningApp != 0 ? "resume" : "launch",
                      m_Computer->isNvidiaServerSoftware,
                      m_App.id, &m_StreamConfig,
                      enableGameOptimizations,
                      m_Preferences->playAudioOnHost,
                      m_InputHandler->getAttachedGamepadMask(),
                      !m_Preferences->multiController,
                      clientSideCursor,
                      rtspSessionUrl,
                      NvHTTP::getXmlString(fresh, "DesktopScaleSupported") == "1" ? m_ResolvedDesktopScale : QString(),
                      NvHTTP::getXmlString(fresh, "AdaptiveBitrateSupported") == "1" ? int(m_Preferences->adaptiveBitrate) : -1,
                      !m_ResumeOnly || desktopTarget,
                      m_HostCapabilities.displayRequest(m_Preferences->displayTarget,
                          m_App.isDesktopTarget()),
                      m_HostDesktopSize, m_App.uuid, &hostWarning);
        if (!hostWarning.isEmpty()) emitLaunchWarning(hostWarning);
        if (verifyDesktopOwnership) http.verifyMainDesktopOwnership(m_Computer->uuid, m_App);
        traceStartup("host-prepared");
    } catch (const GfeHttpResponseException& e) {
        if (!m_StartCancellation.isCancelled())
            emit displayLaunchError(tr("Couldn't start the stream. (%1)").arg(e.toQString()));
        return false;
    } catch (const QtNetworkReplyException& e) {
        m_RetryableTermination = m_ResumeOnly && e.getError() != QNetworkReply::SslHandshakeFailedError &&
                e.getError() != QNetworkReply::OperationCanceledError;
        if (!m_StartCancellation.isCancelled())
            emit displayLaunchError(tr("Couldn't start the stream. (%1)").arg(e.toQString()));
        return false;
    }

    if (m_StartCancellation.isCancelled()) return false;

    QByteArray hostnameStr = m_Computer->activeAddress.address().toUtf8();
    QByteArray siAppVersion = m_Computer->appVersion.toUtf8();

    SERVER_INFORMATION hostInfo;
    hostInfo.address = hostnameStr.data();
    hostInfo.serverInfoAppVersion = siAppVersion.data();
    hostInfo.serverCodecModeSupport = m_Computer->serverCodecModeSupport;

    // Older GFE versions didn't have this field
    QByteArray siGfeVersion;
    if (!m_Computer->gfeVersion.isEmpty()) {
        siGfeVersion = m_Computer->gfeVersion.toUtf8();
    }
    if (!siGfeVersion.isEmpty()) {
        hostInfo.serverInfoGfeVersion = siGfeVersion.data();
    }

    // Older GFE and Sunshine versions didn't have this field
    QByteArray rtspSessionUrlStr;
    if (!rtspSessionUrl.isEmpty()) {
        rtspSessionUrlStr = rtspSessionUrl.toUtf8();
        hostInfo.rtspSessionUrl = rtspSessionUrlStr.data();
    }

    if (m_Preferences->packetSize != 0) {
        // Override default packet size and remote streaming detection
        // NB: Using STREAM_CFG_AUTO will cap our packet size at 1024 for remote hosts.
        m_StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
        m_StreamConfig.packetSize = m_Preferences->packetSize;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using custom packet size: %d bytes",
                    m_Preferences->packetSize);
    }
    else {
        // Use 1392 byte video packets by default
        m_StreamConfig.packetSize = 1392;

        // getActiveAddressReachability() does network I/O, so we only attempt to check
        // reachability if we've already contacted the PC successfully.
        traceStartup("check-network-route");
        switch (m_Computer->getActiveAddressReachability([this] {
            return m_StartCancellation.isCancelled();
        })) {
        case NvComputer::RI_LAN:
            // This address is on-link, so treat it as a local address
            // even if it's not in RFC 1918 space or it's an IPv6 address.
            m_StreamConfig.streamingRemotely = STREAM_CFG_LOCAL;
            break;
        case NvComputer::RI_VPN:
            // It looks like our route to this PC is over a VPN, so cap at 1024 bytes.
            // Treat it as remote even if the target address is in RFC 1918 address space.
            m_StreamConfig.streamingRemotely = STREAM_CFG_REMOTE;
            m_StreamConfig.packetSize = 1024;
            break;
        default:
            // If we don't have reachability info, let moonlight-common-c decide.
            m_StreamConfig.streamingRemotely = STREAM_CFG_AUTO;
            break;
        }
        traceStartup("network-route-checked");
    }

    // If the user has chosen YUV444 without adjusting the bitrate but the host doesn't
    // support YUV444 streaming, use the default non-444 bitrate for the stream instead.
    // This should provide equivalent image quality for YUV420 as the stream would have
    // had if the host supported YUV444 (though obviously with 4:2:0 subsampling).
    // Recommended mode follows the effective (possibly capped) recommendation.
    // Keep Manual mode exactly as requested, even if it happens to equal that value.
    if (m_Preferences->autoAdjustBitrate && m_Preferences->enableYUV444 &&
        !(m_StreamConfig.supportedVideoFormats & VIDEO_FORMAT_MASK_YUV444) &&
        m_StreamConfig.bitrate == PreferencesWorkflow::effectiveBitrate(
                                     StreamingPreferences::getDefaultBitrate(m_StreamConfig.width,
                                                                          m_StreamConfig.height,
                                                                          m_StreamConfig.fps,
                                                                          true), m_Preferences->unlockBitrate)) {
        m_StreamConfig.bitrate = PreferencesWorkflow::effectiveBitrate(
                                    StreamingPreferences::getDefaultBitrate(m_StreamConfig.width,
                                                                         m_StreamConfig.height,
                                                                         m_StreamConfig.fps,
                                                                         false), m_Preferences->unlockBitrate);
    }

    if (m_StartCancellation.isCancelled()) return false;
    StarlightSetAdaptiveFeedbackEnabled(adaptiveFeedbackNegotiated);
    if (m_AutomaticBitrate) m_AutomaticBitrate->observeConnection();
    int err = LiStartConnection(&hostInfo, &m_StreamConfig, &k_ConnCallbacks,
                                &m_VideoCallbacks, &m_AudioCallbacks,
                                NULL, 0, NULL, 0);
    if (err != 0) {
        StarlightSetAdaptiveFeedbackEnabled(false);
        StarlightSetFrameObserver(nullptr, nullptr);
        // We already displayed an error dialog in the stage failure
        // listener.
        return false;
    }

    if (m_StartCancellation.isCancelled()) return false;
    return true;
}

void Session::flushWindowEvents()
{
    // Pump events to ensure all pending OS events are posted
    SDL_PumpEvents();

    // Insert a barrier to discard any additional window events.
    // We don't use SDL_FlushEvent() here because it could cause
    // important events to be lost.
    m_FlushingWindowEventsRef++;

    // This event will cause us to set m_FlushingWindowEvents back to false.
    SDL_Event flushEvent = {};
    flushEvent.type = SDL_USEREVENT;
    flushEvent.user.code = SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER;
    SDL_PushEvent(&flushEvent);
}

void Session::setShouldExit(bool quitHostApp)
{
    // If the caller has explicitly asked us to quit the host app,
    // override whatever the preferences say and do it. If the
    // caller doesn't override to force quit, let the preferences
    // dictate what we do.
    if (quitHostApp) {
        m_Preferences->quitAppAfter = true;
    }

    m_ShouldExit = true;
}

void Session::start()
{
    if (m_StartRequested) return;
    m_StartRequested = true;
    m_DiagnosticGeneration = ++s_DiagnosticGeneration;
    if (s_CurrentDiagnostic) s_CurrentDiagnostic->cancel();
    // QML may drop its last reference after Back while the worker still runs.
    // Keep the QObject alive until all deferred connection cleanup has completed.
    setParent(QCoreApplication::instance());
    startWhenReady();
}

void Session::startWhenReady()
{
    if (m_StartCancellation.isCancelled()) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        m_CleanupComplete = true;
        emit cleanupCompleteChanged();
        emit sessionFinished(m_PortTestResults);
        emit readyForDeletion();
        return;
    }
    // Cleanup can include a host quit request. Keep the launcher responsive
    // while the previous session releases its slot.
    if (!s_ActiveSessionSemaphore.tryAcquire()) {
        QTimer::singleShot(50, this, &Session::startWhenReady);
        return;
    }

    // We're now active
    s_ActiveSession = this;

    // Snapshot the intended logical video area on the GUI thread. The host
    // handshake runs on a worker, and its old pixel-width heuristic cannot
    // distinguish unscaled 4K from a Retina 4K screen. Resolve per connection;
    // resizing the stream must not recreate or resize the host's display.
    QSizeF logicalViewport;
    if (m_QtWindow) {
        if (m_IsFullScreen) {
            if (auto screen = m_QtWindow->screen()) logicalViewport = screen->geometry().size();
        }
        else {
            logicalViewport = QSizeF(m_QtWindow->width(), m_QtWindow->height());
        }
    }
    m_ResolvedDesktopScale = DesktopScalePolicy::forViewport(m_Preferences->desktopScale,
        QSize(m_StreamConfig.width, m_StreamConfig.height), logicalViewport);
    qInfo() << "Desktop scale:" << m_Preferences->desktopScale << "resolved to" << m_ResolvedDesktopScale
            << "for logical viewport" << logicalViewport;

    StreamUtils::setActualSize(m_Preferences->viewScale == QStringLiteral("actual"));

    // Initialize the gamepad code with our preferences
    // NB: m_InputHandler must be initialize before starting the connection.
    m_InputHandler = new SdlInputHandler(*m_Preferences, m_StreamConfig.width, m_StreamConfig.height,
                                        m_CursorPolicy.initiallyShowLocalCursor());

    // Kick off the async connection thread then return to the caller to pump the event loop
    m_ConnectionStartInProgress = true;
    auto thread = new AsyncConnectionStartThread(this);
    QObject::connect(thread, &QThread::finished, this, &Session::exec);
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

QString Session::endHostTarget()
{
    m_HostTargetMayExist = false;
    try {
        NvHTTP http(m_Computer);
        QString fresh = http.getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR);
        if (m_App.isRemoteMonitor()) {
            if (NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid)
                return tr("Disconnected. The host's identity changed before Moonwalk could end the remote session.");
            // This control is TLS-client scoped. Never cancel the global game
            // to release this client's independent desktop display.
            http.disconnectRemoteMonitor();
            return {};
        }
        // The host may still be releasing our stream after the local window closes.
        // Wait briefly in the cleanup worker and validate each new snapshot.
        for (unsigned delay : {0u, 250u, 500u}) {
            if (delay) {
                QThread::msleep(delay);
                fresh = http.getAuthenticatedServerInfo(NvHTTP::NVLL_ERROR, true);
            }
            const int current = NvHTTP::getCurrentGame(fresh);
            const QString activeStreams = NvHTTP::getXmlString(fresh, "ActiveStreams");
            qInfo("Host cleanup: authenticated state=%s target=%s resolved=%d active_streams=%s",
                  qPrintable(NvHTTP::getXmlString(fresh, "state")),
                  qPrintable(NvHTTP::getXmlString(fresh, "currentgame")), current, qPrintable(activeStreams));
            if (NvHTTP::getXmlString(fresh, "uniqueid") != m_Computer->uuid ||
                (current && !m_App.matchesRunningTarget(current, NvHTTP::getXmlString(fresh, "currentgameuuid"))))
                return tr("Disconnected. The remote session changed before Moonwalk could finish cleanup.");
            if (!current) return {};
            bool countReported;
            const int count = activeStreams.toInt(&countReported);
            if (count < 0 || (!countReported && (delay || !activeStreams.isEmpty())))
                return tr("Disconnected. The host couldn't confirm cleanup. Check its status before you reconnect.");
            if (count == 0) {
                http.quitApp();
                return {};
            }
        }
        return tr("Disconnected from this device. The host still reports an active stream.");
    } catch (const GfeHttpResponseException&) {
        return tr("Disconnected. The host couldn't confirm cleanup. Check its status before you reconnect.");
    } catch (const QtNetworkReplyException&) {
        return tr("Disconnected. Moonwalk couldn't reach the host to end the session. It may still be in use.");
    }
}

void Session::cancelPendingStart()
{
    m_RetryableTermination = false;
    m_AdjustingForHost = false;
    m_StartCancellation.cancel();
    cancelConnectivityTest();
    // A transport may already have stopped while its route waits for a retry.
    // Ending that route must also release the target it retained. Serialize with
    // new streams; never queue a late cancel behind a newer connection.
    const bool desktop = m_App.isDesktopTarget();
    if (m_CleanupComplete && m_HostTargetMayExist && (desktop || m_Preferences->quitAppAfter)) {
        if (!s_ActiveSessionSemaphore.tryAcquire()) {
            m_HostTargetMayExist = false;
            emit displayLaunchError(tr("A newer stream has started. The previous remote target was left untouched."));
            return;
        }
        m_CleanupComplete = false;
        m_DeletionReady = false;
        emit cleanupCompleteChanged();
        QThreadPool::globalInstance()->start([this] {
            const QString error = endHostTarget();
            s_ActiveSessionSemaphore.release();
            QMetaObject::invokeMethod(this, [this, error] {
                if (!error.isEmpty()) { qWarning() << error; emit hostCleanupFailed(error); }
                connectionCleanupComplete();
            }, Qt::QueuedConnection);
        });
        return;
    }
    if (m_ConnectionStartInProgress) {
        LiInterruptConnection();
    }
    else if (s_ActiveSession == this && m_ConnectionWasEstablished.load() && m_Window) {
        interrupt();
    }
}

void Session::interrupt()
{
    if (s_ActiveSession != this) return;
    // Stop any connection in progress
    LiInterruptConnection();

    // Inject a quit event to our SDL event loop
    SDL_Event event;
    event.type = SDL_QUIT;
    event.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&event);
}

void Session::exec()
{
    // Deliver queued launcher navigation before deciding to create a window.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::sendPostedEvents();
    m_ConnectionStartInProgress = false;

    if (!m_AsyncConnectionSuccess || m_StartCancellation.isCancelled()) {
        delete m_InputHandler;
        m_InputHandler = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
        return;
    }

    int x, y, width, height;
    getWindowDimensions(x, y, width, height);

#ifdef STEAM_LINK
    // We need a little delay before creating the window or we will trigger some kind
    // of graphics driver bug on Steam Link that causes a jagged overlay to appear in
    // the top right corner randomly.
    SDL_Delay(500);
#endif

    // Request at least 8 bits per color for GL
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

    // Disable depth and stencil buffers
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    // We always want a resizable window with High DPI enabled
    // libdecor's GTK plugin dispatches the default GLib context while mapping
    // a Wayland window. A visible Qt Quick launcher can re-enter rendering
    // there and wait for frame callbacks before SDL finishes configuration.
    // Allocate hidden first, then let connectionStarted stop/hide the Qt UI
    // before mapping the SDL surface. Other backends retain their usual path.
    const bool deferWindowShow = qstrcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0;
    Uint32 defaultWindowFlags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    if (deferWindowShow) defaultWindowFlags |= SDL_WINDOW_HIDDEN;

    // If we're starting in windowed mode and the Moonlight GUI is maximized or
    // minimized, match that with the streaming window.
    if (!m_IsFullScreen && m_QtWindow != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // Qt 5.10+ can propagate multiple states together
        if (m_QtWindow->windowStates() & Qt::WindowMaximized) {
            defaultWindowFlags |= SDL_WINDOW_MAXIMIZED;
        }
        if (m_QtWindow->windowStates() & Qt::WindowMinimized) {
            defaultWindowFlags |= SDL_WINDOW_MINIMIZED;
        }
#else
        // Qt 5.9 only supports a single state at a time
        if (m_QtWindow->windowState() == Qt::WindowMaximized) {
            defaultWindowFlags |= SDL_WINDOW_MAXIMIZED;
        }
        else if (m_QtWindow->windowState() == Qt::WindowMinimized) {
            defaultWindowFlags |= SDL_WINDOW_MINIMIZED;
        }
#endif
    }

    // We use only the computer name on macOS to match Apple conventions where the
    // app name is featured in the menu bar and the document name is in the title bar.
#ifdef Q_OS_DARWIN
    std::string windowName = QString(m_Computer->name).toStdString();
#else
    std::string windowName = QString(m_Computer->name + " - Moonwalk").toStdString();
#endif

    // Native-looking on the desktops where libdecor has to draw the title bar.
    bool headerDark = StreamToolbar::palette().color(QPalette::Window).lightnessF() < .5;
    WindowDecoration::styleClientSideHeader(headerDark);

    traceStartup("create-stream-window");
    emit stageStarting(tr("streaming window"));
    m_Window = SDL_CreateWindow(windowName.c_str(),
                                x,
                                y,
                                width,
                                height,
                                defaultWindowFlags | StreamUtils::getPlatformWindowFlags());
    if (!m_Window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL_CreateWindow() failed with platform flags: %s",
                    SDL_GetError());

        m_Window = SDL_CreateWindow(windowName.c_str(),
                                    x,
                                    y,
                                    width,
                                    height,
                                    defaultWindowFlags);
        if (!m_Window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_CreateWindow() failed: %s",
                         SDL_GetError());

            emit displayLaunchError(tr("Moonwalk could not open the streaming window. Try a different display mode or video decoder.")
                                    + "\n\n" + QString::fromUtf8(SDL_GetError()));

            delete m_InputHandler;
            m_InputHandler = nullptr;
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
            return;
        }
    }

    traceStartup("stream-window-created");
    // A valid SDL window now exists. Synchronous QML delivery stops the busy
    // indicator and hides the launcher before native Wayland mapping can pump
    // its GLib events. Creation failures above leave the launcher visible.
    m_NegotiatedEncryption = StarlightGetEncryptionFlags();
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Negotiated media encryption: video=%s, audio=%s",
        (m_NegotiatedEncryption & ENCFLG_VIDEO) ? "on" : "off",
        (m_NegotiatedEncryption & ENCFLG_AUDIO) ? "on" : "off");
    m_ConnectionWasEstablished = true;
    emit connectionStarted();
    if (deferWindowShow) {
        traceStartup("show-stream-window");
        // Wayland's first configure should already request the selected state.
        if (m_IsFullScreen) SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
        m_WindowNeedsPlacement = m_IsFullScreen;
        SDL_ShowWindow(m_Window);
        traceStartup("stream-window-shown");
    }

    // Windowed-mode geometry continuity: HiDPI correction pass.
    //
    // getWindowDimensions() handed us Qt's logical-pixel geometry as a first
    // guess for the SDL window we just created. That guess is only exactly
    // right if SDL's notion of window-geometry units for this platform/
    // compositor combination agrees with Qt's -- true on X11 and macOS, not
    // guaranteed on Wayland (see the comment in getWindowDimensions()).
    // Rather than hardcode a guess about which convention SDL used, we
    // measure it: SDL_GetWindowSizeInPixels() tells us the actual physical
    // pixel size SDL realized for our logical-size request, which lets us
    // compute the scale factor SDL actually applied and compare it to Qt's
    // devicePixelRatio(). If they disagree, we resize the SDL window so its
    // PHYSICAL on-screen footprint matches the launcher window's physical
    // footprint exactly -- that's what "the same size" means to a human
    // looking at the screen, regardless of which logical-pixel convention
    // either toolkit used internally.
    //
    //   qtDpr         = m_QtWindow->devicePixelRatio()            // Qt's logical->physical factor
    //   realizedScale = SDL_GetWindowSizeInPixels() / requested(w,h) // SDL's actual factor
    //   correction    = qtDpr / realizedScale
    //   corrected(w,h) = requested(w,h) * correction
    //
    // realizedScale == qtDpr (correction == 1, no-op) on X11, on macOS, and
    // on any Wayland session where SDL's fractional/integer scale handling
    // happens to already match Qt's for the current output scale (e.g. both
    // agree at 1x). Skipped entirely when the window was created maximized
    // or minimized, since there's no meaningful "windowed size" to correct
    // in that case.
    //
    // The trace line INSIDE this block is gated on the same condition as the
    // correction itself (nothing to measure/correct otherwise), but we log
    // unconditionally right here first so a live run's journal always shows
    // whether this pass ran at all and, if not, exactly why -- this used to
    // be entirely silent when the condition was false, which is exactly the
    // "the logging we added didn't surface" symptom from the field report.
    bool windowMaximizedOrMinimized = (SDL_GetWindowFlags(m_Window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) != 0;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Windowed-mode geometry continuity: SDL_CreateWindow requested (%d,%d %dx%d); "
                "m_IsFullScreen=%s m_QtWindow=%s maximizedOrMinimized=%s",
                x, y, width, height,
                m_IsFullScreen ? "true" : "false",
                m_QtWindow != nullptr ? "valid" : "null",
                windowMaximizedOrMinimized ? "true" : "false");

    if (!m_IsFullScreen && m_QtWindow != nullptr && !windowMaximizedOrMinimized) {
        qreal qtDpr = m_QtWindow->devicePixelRatio();

#if SDL_VERSION_ATLEAST(2, 26, 0)
        int realizedPixelW = 0, realizedPixelH = 0;
        SDL_GetWindowSizeInPixels(m_Window, &realizedPixelW, &realizedPixelH);

        double realizedScaleW = (width > 0) ? (double)realizedPixelW / width : qtDpr;
        double realizedScaleH = (height > 0) ? (double)realizedPixelH / height : qtDpr;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Windowed-mode geometry continuity: Qt launcher geometry=(%d,%d %dx%d) dpr=%.3f; "
                    "SDL requested %dx%d, realized %dx%d px (scale %.3f x %.3f)",
                    x, y, width, height, qtDpr,
                    width, height, realizedPixelW, realizedPixelH,
                    realizedScaleW, realizedScaleH);

        if (realizedScaleW > 0.0 && realizedScaleH > 0.0 &&
                (qAbs(realizedScaleW - qtDpr) > 0.01 || qAbs(realizedScaleH - qtDpr) > 0.01)) {
            int correctedWidth = qMax(1, (int)qRound(width * (qtDpr / realizedScaleW)));
            int correctedHeight = qMax(1, (int)qRound(height * (qtDpr / realizedScaleH)));

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Windowed-mode geometry continuity: correcting SDL window size "
                        "%dx%d -> %dx%d to match the launcher's on-screen size (factor %.3f x %.3f)",
                        width, height, correctedWidth, correctedHeight,
                        qtDpr / realizedScaleW, qtDpr / realizedScaleH);

            SDL_SetWindowSize(m_Window, correctedWidth, correctedHeight);
            width = correctedWidth;
            height = correctedHeight;
        }
#else
        // SDL_GetWindowSizeInPixels() isn't available before SDL 2.26; we
        // have no way to measure the realized scale on this SDL version, so
        // we can only log what we asked for and hope SDL's convention
        // matches Qt's (true on X11/macOS, unverified on Wayland).
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Windowed-mode geometry continuity: Qt launcher geometry=(%d,%d %dx%d) dpr=%.3f; "
                    "SDL requested %dx%d (built against SDL < 2.26: no pixel-size introspection "
                    "available to verify/correct against a HiDPI Wayland compositor)",
                    x, y, width, height, qtDpr, width, height);
#endif
    }

    // Reassert the windowed launcher-continuity position explicitly. It's
    // already passed to SDL_CreateWindow() above, but some platforms only
    // loosely honor a creation-time position hint, so re-applying it once the
    // window exists is cheap insurance.
    //
    // On Wayland, xdg-shell forbids clients from positioning their own
    // top-level windows, so this call is a documented no-op there (and was
    // already a no-op as a creation-time hint too) -- SDL doesn't report an
    // error for it, and we don't treat it as one. It still works on X11.
    if (!m_IsFullScreen && m_QtWindow != nullptr) {
        SDL_SetWindowPosition(m_Window, x, y);
    }

    m_InputHandler->setWindow(m_Window);
    m_SystemKeysRequested = m_Preferences->captureSysKeysMode != StreamingPreferences::CSK_OFF;
    updateLocalTextInput();

    QImage iconImage = QImage(QStringLiteral(":/res/starlight.png"))
                           .scaled(ICON_SIZE, ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_RGBA8888);
    SDL_Surface* iconSurface = SDL_CreateRGBSurfaceWithFormatFrom((void*)iconImage.constBits(),
                                                                  iconImage.width(),
                                                                  iconImage.height(),
                                                                  32,
                                                                  iconImage.bytesPerLine(),
                                                                  SDL_PIXELFORMAT_RGBA32);
#ifndef Q_OS_DARWIN
    // Other platforms seem to preserve our Qt icon when creating a new window.
    if (iconSurface != nullptr) {
        // This must be called before entering full-screen mode on Windows
        // or our icon will not persist when toggling to windowed mode
        SDL_SetWindowIcon(m_Window, iconSurface);
    }
#endif

    // Update the window display mode based on our current monitor
    // for if/when we enter full-screen mode.
    updateOptimalWindowDisplayMode();

    // Enter full screen if requested
    if (m_IsFullScreen && !deferWindowShow) {
        SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
    }

    bool needsFirstEnterCapture = false;
    bool needsPostDecoderCreationCapture = false;
    bool needsInitialWaylandFullscreen = deferWindowShow && m_IsFullScreen;

    // Avoid capturing the mouse initially for windowed relative mode.
    // We still capture in windowed absolute mode because it doesn't
    // constrain the motion of the cursor. This allows the user to
    // easily reposition or resize the window.
    if (m_HaveCaptureOverride ? m_DesiredCapture : (m_IsFullScreen || m_Preferences->absoluteMouseMode)) {
        // HACK: For Wayland, we wait until we get the first SDL_WINDOWEVENT_ENTER
        // event where it seems to work consistently on GNOME. For other platforms,
        // especially where SDL may call SDL_RecreateWindow(), we must only capture
        // after the decoder is created.
        if (strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
            // Native Wayland: Capture on SDL_WINDOWEVENT_ENTER
            needsFirstEnterCapture = true;
        }
        else {
            // X11/XWayland: Capture after decoder creation
            needsPostDecoderCreationCapture = true;
        }
    }

    // Disable the screen saver if requested
    if (m_Preferences->keepAwake) {
        SDL_DisableScreenSaver();
    }

    // Hide Qt's fake mouse cursor on EGLFS systems
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    }

    // Set timer resolution to 1 ms on Windows for greater
    // sleep precision and more accurate callback timing.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    int currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
    auto currentDisplayScale = [this]() -> qreal {
        const int current = SDL_GetWindowDisplayIndex(m_Window);
        const auto displays = DisplaySnapshot::capture(SDL_GetNumVideoDisplays(), [](int index) {
            DisplayMetadata metadata;
            metadata.name = QString::fromUtf8(SDL_GetDisplayName(index));
            SDL_Rect bounds {};
            if (SDL_GetDisplayBounds(index, &bounds) == 0)
                metadata.bounds = QRect(bounds.x, bounds.y, bounds.w, bounds.h);
            return metadata;
        });
        for (auto* screen : QGuiApplication::screens())
            if (displays.indexForScreen(screen->name(), screen->geometry(), screen->devicePixelRatio()) == current)
                return screen->devicePixelRatio();
        return m_QtWindow ? m_QtWindow->devicePixelRatio() : 1;
    };
    qreal localDisplayScale = currentDisplayScale();
    m_LocalDisplayScale = localDisplayScale;

    // Now that we're about to stream, any SDL_QUIT event is expected
    // unless it comes from the connection termination callback where
    // (m_UnexpectedTermination is set back to true).
    m_UnexpectedTermination = false;

    // Start rich presence to indicate we're in game
    RichPresenceManager presence(*m_Preferences, m_App.name);

    // Toggle the stats overlay if requested by the user
    m_OverlayManager.setOverlayState(Overlay::OverlayDebug, m_Preferences->showPerformanceOverlay);

    // Switch to async logging mode when we enter the SDL loop
    StreamUtils::enterAsyncLoggingMode();

    // Construct and start the dedicated clipboard networking thread right before we
    // enter the SDL loop below (design doc Part 1.3 / Wave C - see
    // streaming/clipboardsync.h for the full threading rationale). It owns all
    // clipboard networking off the SDL thread now; the SDL loop below only ever
    // enqueues local changes / publishes focus / handles the apply handoff, none of
    // which touch the network.
    m_ImageClipboardSupported = m_ImageClipboardSupported && ClientClipboardImage::supportsPng();
    emit controlsChanged();
    m_ClipboardSync = new ClipboardSync(m_Computer, m_ImageClipboardSupported,
                                       m_HostCapabilities.apollo && !m_CaptureHealthSupported);
    m_ClipboardSync->setPermissions(m_HostCapabilities.allows(HostCapabilities::ClipboardRead),
                                    m_HostCapabilities.allows(HostCapabilities::ClipboardWrite));
    m_ClipboardSync->setDirection(m_ClipboardDirection);
    connect(m_ClipboardSync, &ClipboardSync::warning, this, [this](const QString& message) {
        m_TextInputNotice = message;
        m_TextInputNoticeAt = SDL_GetTicks();
    });
    m_ClipboardSync->setFocused((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    m_ClipboardSync->start();
    if (m_HostCapabilities.apollo && !m_CaptureHealthSupported) {
        m_RuntimeBitrate = new RuntimeBitrate(m_Computer, m_StreamConfig.bitrate, m_Preferences->adaptiveBitrate && !m_AutomaticBitrate && m_BitrateChange == HostCapacity::BitrateChange::Live, [this] {
            return RuntimeBitrate::Observation {m_NetworkPoor.load(), m_HasVideoFrame.load() &&
                Uint32(SDL_GetTicks() - m_LastFrameAt.load()) < 1500};
        }, CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration));
        // Straight from the request thread, so the stall that may follow is timed from the answer.
        connect(m_RuntimeBitrate, &RuntimeBitrate::applied, this, [this] {
            m_BitrateAppliedAt = SDL_GetTicks();
            if (m_AutomaticBitrate) m_AutomaticBitrate->changeApplied();
        }, Qt::DirectConnection);
        connect(m_RuntimeBitrate, &RuntimeBitrate::changed, this, [this](bool supported, int bitrate, const QString& notice) {
            if (!m_RuntimeBitrate) return;
            m_RuntimeBitrateSupported = supported;
            m_RuntimeBitrateKbps = bitrate;
            if (m_AutomaticBitrate && bitrate > 0)
                m_AutomaticBitrateKbps.store(RuntimeBitratePolicy::encoderKbps(bitrate,
                    CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration)));
            m_RuntimeBitrateNotice = notice;
            emit controlsChanged();
        });
        m_RuntimeBitrate->start();
    }
    // The controller can call the HTTP worker directly. Publish it before starting
    // the controller and join the controller before deleting the worker.
    if (m_AutomaticBitrate) m_AutomaticBitrate->start();

    // Keep a snapshot across the focus edge: an in-flight host response must not
    // replace clipboard text copied in another local application after alt-tab.
    auto currentClipboardHash = [this]() {
        if (m_ImageClipboardSupported) {
            const auto value = ClientClipboardImage::read();
            if (value.image) return value.hash;
        }
        return QCryptographicHash::hash(NativeClipboard::readText(), QCryptographicHash::Sha256);
    };
    ClipboardFocusGuard clipboardFocusGuard(currentClipboardHash());
    const bool waylandClipboard = QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
    struct RemoteClipboard {
        QByteArray bytes;
        quintptr generation = 0;
        bool image = false;
    };
    RemoteClipboard pendingClipboard;
    bool clipboardNeedsInput = false;
    auto noteClipboardChange = [&](const QByteArray &hash) {
        clipboardFocusGuard.focusedClipboardChanged(hash);
        if (waylandClipboard && !NativeClipboard::ownsSelection()) {
            // Even a copy of the same bytes gives the external selection a newer
            // Wayland serial. SDL can report success when the compositor ignores it.
            clipboardNeedsInput = true;
            pendingClipboard = {};
        }
    };
    auto applyClipboard = [&](RemoteClipboard change) {
        if (change.bytes.isEmpty() || !m_ClipboardSync->canApplyRemote(change.generation) ||
                !clipboardFocusGuard.mayApply(currentClipboardHash(), change.generation)) {
            return;
        }
        if (clipboardNeedsInput && !m_ControlsOpen) {
            pendingClipboard = std::move(change);
            return;
        }
        pendingClipboard = {};
        const bool applied = change.image
            ? ClientClipboardImage::publish(change.bytes, m_ControlsOpen)
            : NativeClipboard::publishText(change.bytes, m_ControlsOpen);
        if (applied) {
            clipboardFocusGuard.applied(change.image
                ? ClipboardSync::imageHash(change.bytes)
                : QCryptographicHash::hash(change.bytes, QCryptographicHash::Sha256));
            return;
        }
        m_ClipboardSync->reportRemoteApplyFailure(change.generation, change.bytes, change.image);
        if (change.image) {
            m_TextInputNotice = tr("Could not apply the clipboard image. Try copying it again.");
            m_TextInputNoticeAt = SDL_GetTicks();
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to apply host clipboard: %s", SDL_GetError());
        }
    };
    auto refreshClipboardInput = [&] {
        if (clipboardNeedsInput) {
            clipboardNeedsInput = false;
            applyClipboard(std::exchange(pendingClipboard, {}));
        }
    };
    // Qt also owns text copied by the controls and X11 publications. Keep its
    // ownership cache current even when the host supports text only.
    const bool qtClipboardEvents = NativeClipboard::needsQtEvents();
    const auto clipboardConnection = qtClipboardEvents ?
            connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [] {
                SDL_Event clipboardEvent {};
                clipboardEvent.type = SDL_CLIPBOARDUPDATE;
                SDL_PushEvent(&clipboardEvent);
            }) : QMetaObject::Connection();
    m_FileTransfer = new FileTransfer(m_Computer, m_FileTransferSupported, this);
    emit transferChanged();
    Uint32 lastAuxiliaryPump = 0;
    NvHTTP::CancelToken healthCancellation;
    QThread* healthThread = nullptr;
    if (m_CaptureHealthSupported) {
        healthThread = QThread::create([this, &healthCancellation] {
            NvHTTP http(m_Computer);
            http.setCancelToken(&healthCancellation);
            quint64 previousFrames = 0, previousGeneration = 0;
            Uint32 previousAt = 0;
            while (!healthCancellation.isCancelled()) {
                try {
                    const QString info = http.getServerInfo(NvHTTP::NVLL_NONE, true);
                    m_SourceHealthAt=SDL_GetTicks();
                    const QString state = NvHTTP::getXmlString(info, "CaptureState");
                    const quint64 generation = NvHTTP::getXmlString(info, "CaptureGeneration").toULongLong();
                    const quint64 frames = NvHTTP::getXmlString(info, "CaptureFrames").toULongLong();
                    const Uint32 now = SDL_GetTicks();
                    m_CapturedFps = previousAt && generation == previousGeneration && frames >= previousFrames && now != previousAt
                        ? double(frames - previousFrames) * 1000 / Uint32(now - previousAt) : 0;
                    previousFrames = frames; previousGeneration = generation; previousAt = now;
                    m_RemoteInputAvailable = NvHTTP::getXmlString(info, "AccessibilityGranted") != "0" &&
                        (NvHTTP::getXmlString(info, "KeyboardEnabled") != "0" || NvHTTP::getXmlString(info, "MouseEnabled") != "0");
                    m_SourceHealthy = (generation>m_CaptureBaseline ||
                        (m_CaptureBaselineActive && generation==m_CaptureBaseline)) && frames>0 &&
                        (state == "complete" || state == "idle");
                } catch (...) { m_SourceHealthy = false; m_CapturedFps = 0; }
                const int waits=m_SourceHealthy.load() ? 40 : 5;
                for (int i=0;i<waits && !healthCancellation.isCancelled();++i) QThread::msleep(50);
            }
        });
        healthThread->setObjectName("Capture health");
        healthThread->start();
    }
    ViewScalePolicy::WheelPan viewWheelPan;
    QVariantList droppedFiles;
    bool inFileDrop = false;
    bool toolbarExpanded = false;
    Uint32 toolbarRevealAt = 0;
    bool toolbarHeldOpen = false;
    // Pressed on the pill; it opens on release, like a button.
    bool toolbarHandlePress=false;
    Uint32 remotePointerButtons=0;
    QString toolbarKey;
    StreamToolbar::Motion toolbarMotion;
    bool toolbarMoving = false;
    Uint64 toolbarSteppedAt = SDL_GetPerformanceCounter();
    Uint32 toolbarPaintedAt = 0;
    QImage toolbarControls;
    QString toolbarControlsKey;
    // New frames reach the screen with the stream's own, so paint no faster than those.
    const Uint32 toolbarFrameMs = Uint32(qBound(6, 1000 / qMax(1, m_StreamConfig.fps), 16));
    StreamToolbar::Status toolbarStatus;
    { QReadLocker lock(&m_Computer->lock); toolbarStatus.host = m_Computer->name; }
    quint64 toolbarLastFrames = 0;
    Uint32 toolbarStatusAt = 0;
    StreamToolbar::Action toolbarPressed = StreamToolbar::None;
    auto dispatchToolbar = [this](StreamToolbar::Action action) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stream toolbar action: %d", static_cast<int>(action));
        m_ToolbarAction = true;
        switch (action) {
        case StreamToolbar::Controls: openControls(); break;
        case StreamToolbar::Mute: controlAction("mute", !m_ManualAudioMuted); break;
        case StreamToolbar::Fullscreen: controlAction("fullscreen", true); break;
        case StreamToolbar::Capture: controlAction("capture", !m_InputHandler->isCaptureActive()); break;
        case StreamToolbar::ViewScale: controlAction("viewScale", StreamUtils::actualSize() ? "fit" : "actual"); break;
        case StreamToolbar::Disconnect: controlAction("disconnect", true); break;
        case StreamToolbar::None: break;
        }
        m_ToolbarAction = false;
    };

    if (m_CursorPolicy.syncShapes(m_InputHandler->isAbsoluteMouseMode())) {
        int windowWidth = 0, pixelWidth = 0;
        SDL_GetWindowSize(m_Window, &windowWidth, nullptr);
        SDL_GetWindowSizeInPixels(m_Window, &pixelWidth, nullptr);
        m_CursorSync = new CursorSync(m_Computer, localDisplayScale,
            CursorSizePolicy::coordinateScale(windowWidth, pixelWidth, localDisplayScale));
        m_CursorSync->setFocused((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0);
        m_CursorSync->start();
    }

    // Keep Qt user input suspended during streaming. Only the explicit controls
    // window receives Qt input while the forwarding owner is paused.
    SDL_Event event;
    const Uint32 presentationStartedAt=SDL_GetTicks();
    int slowWindows = 0, activeWindows = 0, healthyWindows = 0;
    bool qualityRecovered = false;
    for (;;) {
        if (hasVideoFrame() && !m_FirstFrameNotified) {
            m_FirstFrameNotified = true;
            const int decodedWidth = m_DecodedWidth.load(), decodedHeight = m_DecodedHeight.load();
            if (decodedWidth > 0 && decodedHeight > 0) {
                m_ActiveVideoWidth = decodedWidth;
                m_ActiveVideoHeight = decodedHeight;
            }
            const StreamPlanning::Rung actual {m_ActiveVideoWidth,m_ActiveVideoHeight,m_ActiveVideoFrameRate};
            const bool applyingRecommendation = m_Adaptation.pending;
            if (!m_Adaptation.presented(actual,LiGetMicroseconds()/1000)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Automatic stream: host sent %dx%dx%d instead of %dx%dx%d; further size trials stopped",
                    actual.width,actual.height,actual.fps,m_Adaptation.target.width,m_Adaptation.target.height,m_Adaptation.target.fps);
                auto rejected = HostCapacity::load(m_Computer->uuid);
                rejected.reductionRejectedUntil = QDateTime::currentSecsSinceEpoch() + StreamPlanning::OverloadMemorySeconds;
                rejected.save(m_Computer->uuid);
            }
            if (applyingRecommendation)
                StreamRecommendation::clear(m_Computer->uuid, m_PerformanceContext, m_StreamPlan.codec);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Effective stream picture: %dx%dx%d",actual.width,actual.height,actual.fps);
            emit videoFramePresented(); emit controlsChanged();
        }
        const auto stable = m_PresentationHealth.observe(SDL_GetTicks(), m_PresentedFrames.load(),
            m_LastFrameAt.load(), m_CaptureHealthSupported, sourceHealthy());
        m_StableVideoMs = stable;
        // Judge recent delivery, not the inverse of frame pipeline latency. Two
        // active windows are needed; sparse idle updates must never shrink a desktop.
        const bool bitrateSettling = m_BitrateAppliedAt.load() && Uint32(SDL_GetTicks() - m_BitrateAppliedAt.load()) < 3000;
        if (bitrateSettling || m_ControlsOpen
                || (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED)) {
            slowWindows = activeWindows = healthyWindows = 0;
            std::lock_guard<std::mutex> lock(m_MeasurementLock);
            m_RecentHostLatency = {};
        }
        else if ((m_PlannedSizeAutomatically || !m_StartupBitrateContext.isEmpty()) && !m_SafeSettingsTrial) {
            HostLatencyMeter latency;
            {
                std::lock_guard<std::mutex> lock(m_MeasurementLock);
                latency = m_RecentHostLatency;
            }
            if (latency.enoughToJudge()) {
                const auto evidence = StreamPlanning::adaptationEvidence(latency, m_ActiveVideoWidth, m_ActiveVideoHeight,
                    m_ActiveVideoFrameRate, m_CaptureHealthSupported, m_CapturedFps.load(), sourceHealthy(), capacityNetworkClean());
                const bool active = evidence.active;
                const bool slow = evidence.slow;
                healthyWindows = active && evidence.deliveredFps >= m_ActiveVideoFrameRate * .97 && capacityNetworkClean()
                    ? healthyWindows + 1 : 0;
                activeWindows = active ? activeWindows + 1 : 0;
                if (active && activeWindows >= 2 && activeWindows % 2 == 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Automatic stream observation: %dx%dx%d receiving %.2f FPS, host %.2f ms, submitted_frames=%llu",
                        m_ActiveVideoWidth,m_ActiveVideoHeight,m_ActiveVideoFrameRate,evidence.deliveredFps,
                        latency.averageMs(),(unsigned long long)m_PresentedFrames.load());
                }
                if (activeWindows >= 4) {
                    std::lock_guard<std::mutex> lock(m_MeasurementLock);
                    m_LastQualifiedHostLatency = latency;
                    m_LastQualifiedHostAt = SDL_GetTicks();
                }
                slowWindows = slow ? slowWindows + 1 : 0;
                const StreamPlanning::Rung current {m_ActiveVideoWidth,m_ActiveVideoHeight,m_ActiveVideoFrameRate};
                if (activeWindows >= 2 && m_Adaptation.checkTrial && m_Adaptation.encoderCeilingKbps > 0) {
                    m_Adaptation.checkTrial = false;
                    // Retain a lower launch rate only when the same picture
                    // improved. The original rate also records a tried mode.
                    const auto sample = streamMeasurement(latency);
                    const bool helped = StreamPlanning::reductionHelped(m_Adaptation.baseline,
                        evidence.deliveredFps, latency.averageMs());
                    if (!m_StartupBitrateContext.isEmpty() && sample.codec == m_Adaptation.baseline.codec) {
                        const int settled = helped ? m_Adaptation.encoderCeilingKbps : m_NormalStartupBitrateKbps;
                        StreamingPreferences::rememberAutomaticBitrate(m_Computer->uuid, settled, m_StartupBitrateContext);
                        sample.save(m_Computer->uuid);
                        m_StartupBitrateKnown = true;
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Automatic stream: %s startup bitrate at %d kbps",
                            helped ? "retained" : "restored", settled);
                    }
                }
                else if (activeWindows >= 2 && m_Adaptation.checkTrial) {
                    m_Adaptation.checkTrial = false;
                    auto capacity = m_Adaptation.baseline;
                    const auto previousPixels = qint64(capacity.width) * capacity.height;
                    const auto currentPixels = qint64(current.width) * current.height;
                    const bool ineffectiveCut = currentPixels < previousPixels
                        && !StreamPlanning::reductionHelped(capacity,evidence.deliveredFps,latency.averageMs());
                    const bool failedRecovery = currentPixels > previousPixels && slowWindows >= 2;
                    if (ineffectiveCut || failedRecovery) {
                        // Restore the picture measurement, retaining newer encoder-cost evidence.
                        const auto latest = HostCapacity::load(m_Computer->uuid);
                        capacity.bitrateContext = latest.bitrateContext;
                        capacity.bitrateMeasuredAt = latest.bitrateMeasuredAt;
                        capacity.bitrateChange = latest.bitrateChange;
                        capacity.bitrateChangeStallMs = latest.bitrateChangeStallMs;
                        if (ineffectiveCut)
                            capacity.reductionRejectedUntil = QDateTime::currentSecsSinceEpoch() + StreamPlanning::OverloadMemorySeconds;
                        else {
                            capacity.overloadedPixelRate = currentPixels * current.fps;
                            capacity.overloadedAt = QDateTime::currentSecsSinceEpoch();
                        }
                        capacity.save(m_Computer->uuid);
                        m_RejectTrialMeasurement = true;
                        m_Adaptation.locked = true;
                        recommendStreamChange(StreamAdaptation::mode(capacity), capacity, true);
                    }
                    else SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Automatic stream: retained %dx%dx%d at %.1f FPS, host %.1f ms",
                        current.width,current.height,current.fps,evidence.deliveredFps,latency.averageMs());
                }
                const auto rungs = recommendationModes(current.fps);
                const auto capacity = streamMeasurement(latency);
                // A long healthy interval may suggest a larger picture for next time.
                if (m_PlannedSizeAutomatically && !m_Adaptation.recoveryAttempted && m_StreamPlan.limitedByHost && stable >= 180000
                        && healthyWindows >= 12 && capacityNetworkClean()) {
                    for (int i = 1; i < rungs.size(); ++i) {
                        if (!StreamAdaptation::same(rungs[i],current)) continue;
                        m_Adaptation.recoveryAttempted = true;
                        if (!StreamPlanning::rememberedOverload(capacity, rungs[i-1].pixelRate(), QDateTime::currentSecsSinceEpoch()))
                            recommendStreamChange(rungs[i-1], streamMeasurement(latency));
                        break;
                    }
                }
                if (slowWindows >= 2) {
                    // Costly encoder changes are not forced by the outer policy.
                    // Unknown or costly changes are learned for the next connection.
                    if (m_AutomaticBitrate && m_BitrateChange == HostCapacity::BitrateChange::Live &&
                            m_Adaptation.mayTryQuality(m_BitrateChange)) {
                        const auto rate = m_AutomaticBitrate->status();
                        if (rate.targetKbps > rate.floorKbps * 1.15) {
                            ++m_Adaptation.qualityTrials;
                            const int target = qMax(rate.floorKbps,rate.targetKbps *
                                (m_BitrateChange == HostCapacity::BitrateChange::Live ? 7 : 5) / 10);
                            m_Adaptation.encoderCeilingKbps = target;
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Automatic stream: trying encoder target %d kbps after %.1f/%d FPS, host %.1f ms (episode trial %d)",
                                target,evidence.deliveredFps,current.fps,latency.averageMs(),m_Adaptation.qualityTrials);
                            m_AutomaticBitrate->controller().setCeiling(target);
                            slowWindows = activeWindows = healthyWindows = 0;
                            { std::lock_guard<std::mutex> lock(m_MeasurementLock); m_RecentHostLatency = {}; }
                            continue;
                        }
                    }
                    if (m_AutomaticBitrate && m_BitrateChange != HostCapacity::BitrateChange::Live &&
                            m_Adaptation.encoderCeilingKbps == 0 && !m_Recommendation.createdAt) {
                        const auto rate = m_AutomaticBitrate->status();
                        const int applied = m_AutomaticBitrateKbps.load();
                        // Source pressure can need lower encoder work even on a
                        // clean network. Give quality one connection to help before
                        // proposing fewer pixels; never probe a costly path live.
                        if (applied > rate.floorKbps * 1.15)
                            recommendStreamChange(current, streamMeasurement(latency), false,
                                qMin(rate.targetKbps, qMax(rate.floorKbps, applied * 7 / 10)));
                    }
                    if (!m_StartupBitrateContext.isEmpty() && !m_StartupBitrateKnown &&
                            !m_Adaptation.checkTrial && !m_Recommendation.createdAt) {
                        const int channels = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration);
                        const int floor = RuntimeBitratePolicy::encoderKbps(m_StreamPlan.floorKbps, channels);
                        if (const int target = StreamPlanning::lowerStartupBitrate(m_NormalStartupBitrateKbps,
                                floor, capacityNetworkClean(), latency, current.fps))
                            recommendStreamChange(current, capacity, false, target);
                    }
                    if (m_PlannedSizeAutomatically && evidence.canTrySize &&
                            QDateTime::currentSecsSinceEpoch() >= capacity.reductionRejectedUntil) {
                        for (const auto& next : rungs) {
                            if (next.width >= current.width * .97) continue;
                            if (m_Adaptation.mayChange() && !m_Recommendation.createdAt)
                                recommendStreamChange(next, capacity);
                            break;
                        }
                    }
                }
                if (healthyWindows >= 12 && m_Recommendation.createdAt && !m_Recommendation.rollback &&
                        qint64(m_Recommendation.target.width) * m_Recommendation.target.height < qint64(current.width) * current.height) {
                    StreamRecommendation::clear(m_Computer->uuid, m_PerformanceContext, m_StreamPlan.codec);
                    m_Recommendation = {};
                    emit controlsChanged();
                }
                std::lock_guard<std::mutex> lock(m_MeasurementLock);
                m_RecentHostLatency = {};
            }
        }
        if (m_Adaptation.qualityTrials && !qualityRecovered && stable >= 180000 && healthyWindows >= 12
                && m_AutomaticBitrate && m_BitrateChange == HostCapacity::BitrateChange::Live) {
            m_Adaptation.encoderCeilingKbps = 0;
            const int channels = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(m_StreamConfig.audioConfiguration);
            m_AutomaticBitrate->controller().setCeiling(RuntimeBitratePolicy::encoderKbps(m_StreamPlan.ceilingKbps, channels));
            qualityRecovered = true;
        }
        if (stable >= 30000 && !m_SuccessfulSettingsRemembered) {
            m_SuccessfulSettingsRemembered = true;
            if (!m_SafeSettingsTrial) m_Preferences->rememberSuccessfulSettings();
        }
        if (m_CaptureHealthSupported && !m_FirstFrameNotified &&
                Uint32(SDL_GetTicks()-presentationStartedAt)>15000) {
            emit displayLaunchError(tr("Moonwalk Host didn't send a usable desktop picture. Check its Screen Recording permission on the Mac and try again."));
            m_RetryableTermination = false;
            m_UnexpectedTermination = true;
            goto DispatchDeferredCleanup;
        }
        refreshStreamStatus();
        int toolbarWindowWidth=0, toolbarPixelWidth=0;
        SDL_GetWindowSize(m_Window, &toolbarWindowWidth, nullptr);
#if SDL_VERSION_ATLEAST(2,26,0)
        SDL_GetWindowSizeInPixels(m_Window, &toolbarPixelWidth, nullptr);
#endif
        int toolbarPixelHeight=0;
#if SDL_VERSION_ATLEAST(2,26,0)
        SDL_GetWindowSizeInPixels(m_Window, nullptr, &toolbarPixelHeight);
#endif
        // Where the picture starts. Actual Size pans it, so it fills the window.
        SDL_Rect toolbarPicture {0, 0, toolbarPixelWidth, toolbarPixelHeight};
        if (!StreamUtils::actualSize() && toolbarPixelWidth>0 && toolbarPixelHeight>0) {
            SDL_Rect toolbarSource {0, 0, m_StreamConfig.width, m_StreamConfig.height};
            StreamUtils::scaleSourceToDestinationSurface(&toolbarSource, &toolbarPicture);
        }
        const auto toolbarMetrics = StreamToolbar::metrics(toolbarWindowWidth, toolbarPixelWidth, localDisplayScale, toolbarPicture.y);
        if (m_CursorSync) m_CursorSync->setDisplayScale(localDisplayScale, toolbarMetrics.inputScale);
        const int toolbarWidth = toolbarMetrics.width;
        const qreal toolbarDpr = toolbarMetrics.pixelRatio;
        const Uint32 toolbarNow=SDL_GetTicks();
        const bool toolbarStarting=Uint32(toolbarNow-presentationStartedAt)<=5000;
        const bool pointerGrabbed=SDL_GetRelativeMouseMode();
        int mx=0,my=0; SDL_GetMouseState(&mx,&my);
        const QPoint toolbarPointer=toolbarMetrics.point(mx,my);
        const bool pointerFree=!pointerGrabbed && remotePointerButtons==0 && SDL_GetMouseFocus()==m_Window;
        // Tapping the pill opens the bar; it closes soon after the pointer moves away.
        const QRect toolbarReach=StreamToolbar::bounds(toolbarWidth).adjusted(-12,-StreamToolbar::BarTop-toolbarMetrics.topInset,12,12);
        if (toolbarExpanded && pointerFree && toolbarReach.contains(toolbarPointer)) {
            toolbarRevealAt = toolbarNow;
        } else if (toolbarExpanded && toolbarPressed==StreamToolbar::None
                   && Uint32(toolbarNow-toolbarRevealAt)>(toolbarHeldOpen?3000u:900u)) {
            toolbarExpanded = false;
            toolbarHeldOpen = false;
        }
        if (m_ControlsOpen) { toolbarExpanded = false; toolbarHeldOpen = false; toolbarHandlePress = false; }
        // Live numbers, read once a second while someone can see them.
        if (toolbarExpanded && Uint32(toolbarNow-toolbarStatusAt)>=1000) {
            const auto stats=liveStats();
            const auto frames=stats.value("presentedFrames").toULongLong();
            if (toolbarStatusAt && Uint32(toolbarNow-toolbarStatusAt)<4000)
                toolbarStatus.fps=qRound(qreal(frames-toolbarLastFrames)*1000/Uint32(toolbarNow-toolbarStatusAt));
            else toolbarStatus.fps=-1;
            toolbarStatus.rttMs=stats.value("rttMs").toInt();
            toolbarStatus.poor=stats.value("networkPoor").toBool();
            toolbarStatus.kbps=m_AutomaticBitrateKbps.load();
            toolbarLastFrames=frames; toolbarStatusAt=toolbarNow;
        }
        const bool fullScreen = bool(SDL_GetWindowFlags(m_Window)&SDL_WINDOW_FULLSCREEN);
        const auto palette = StreamToolbar::palette();
        // SDL ignores setting the title it already has, hence the two steps.
        const auto refreshTitle=[this] {
            const QByteArray title(SDL_GetWindowTitle(m_Window));
            SDL_SetWindowTitle(m_Window,(title+' ').constData());
            SDL_SetWindowTitle(m_Window,title.constData());
        };
        // The desktop changed theme mid-stream. libdecor only repaints its header when
        // something it draws changes, so a title change brings the new colours in.
        if (const bool nowDark=palette.color(QPalette::Window).lightnessF()<.5; nowDark!=headerDark) {
            headerDark=nowDark;
            if (WindowDecoration::styleClientSideHeader(headerDark)) refreshTitle();
        }
        // A window that has just reappeared doesn't always keep its title in the
        // desktop's window list. Changing it before the window settles costs it height.
        if (m_RefreshTitleAt && int(toolbarNow-m_RefreshTitleAt)>=0) {
            m_RefreshTitleAt=0;
            refreshTitle();
        }
        StreamToolbar::State toolbarState;
        toolbarState.muted=m_ManualAudioMuted; toolbarState.fullscreen=fullScreen;
        toolbarState.captured=m_InputHandler->isCaptureActive(); toolbarState.actual=StreamUtils::actualSize();
        toolbarState.hovered=toolbarExpanded && pointerFree ? StreamToolbar::hitTest(toolbarWidth,toolbarPointer) : StreamToolbar::None;
        toolbarState.pressed=toolbarPressed; toolbarState.status=toolbarStatus;
        // When the pointer is held by a game the top edge can't be reached, so the pill
        // says which keys open the controls while the stream starts.
        const auto toolbarPalette=palette;
        const QString toolbarMessage=pointerGrabbed && toolbarStarting ? tr("Ctrl+Alt+Shift+O for stream controls") : QString();
        const auto toolbarFont=StreamToolbar::font(QGuiApplication::font());
        // The pill grows a little under the pointer and brightens as it nears.
        const qreal toolbarEmphasis=!pointerFree || toolbarExpanded ? 0
            : StreamToolbar::onHandle(toolbarWidth,toolbarPointer) || toolbarHandlePress ? 1
            : StreamToolbar::nearHandle(toolbarWidth,toolbarPointer) ? .3 : 0;
        const auto toolbarMode=toolbarExpanded ? StreamToolbar::Bar
            : !toolbarMessage.isEmpty() ? StreamToolbar::Message : StreamToolbar::Pill;
        const auto toolbarPill=StreamToolbar::pillRect(toolbarWidth,toolbarEmphasis,toolbarHandlePress);
        const auto toolbarWords=StreamToolbar::messageRect(toolbarWidth,toolbarFont,toolbarMessage);
        const auto toolbarBarRect=QRectF(StreamToolbar::bounds(toolbarWidth));
        const Uint64 toolbarClock=SDL_GetPerformanceCounter();
        if (m_ControlsOpen) toolbarMotion.snap(StreamToolbar::Pill,toolbarPill,toolbarWords,toolbarBarRect);
        else toolbarMoving=toolbarMotion.advance(toolbarMode,toolbarPill,toolbarWords,toolbarBarRect,toolbarEmphasis,
            qreal(toolbarClock-toolbarSteppedAt)/SDL_GetPerformanceFrequency(),qApp->property("starlightReducedMotion").toBool());
        toolbarSteppedAt=toolbarClock;
        const auto toolbarLook=toolbarMotion.look();
        // The controls are painted once per change of what they show; moving frames reuse them.
        if (toolbarLook.controls>0 || toolbarMode==StreamToolbar::Bar) {
            const QString controlsKey=QString("%1/%2/%3/%4/%5/%6/%7/%8/%9/%10/%11/%12/%13").arg(toolbarWidth).arg(toolbarDpr)
                .arg(toolbarState.muted).arg(fullScreen).arg(toolbarState.captured).arg(toolbarState.actual).arg(toolbarPalette.cacheKey())
                .arg(toolbarState.hovered).arg(toolbarState.pressed).arg(toolbarStatus.rttMs).arg(toolbarStatus.fps).arg(toolbarStatus.poor)
                .arg(toolbarStatus.kbps);
            if (controlsKey!=toolbarControlsKey || toolbarControls.isNull()) {
                toolbarControlsKey=controlsKey;
                toolbarControls=StreamToolbar::controls(toolbarWidth,toolbarDpr,toolbarFont,toolbarPalette,toolbarState);
            }
        }
        else if (!toolbarControls.isNull()) { toolbarControls=QImage(); toolbarControlsKey.clear(); }
        const auto toolbarGeometry=toolbarLook.capsule;
        // Always upload the settled frame: rounded geometry can otherwise leave
        // the last animation clip cached until hover changes the controls.
        const QString nextToolbarKey=QString("%1/%2/%3/%4/%5/%6/%7/%8/%9/%10/%11/%12/%13/%14").arg(toolbarWidth).arg(toolbarDpr)
            .arg(m_ControlsOpen).arg(toolbarPalette.cacheKey()).arg(toolbarMetrics.topInset).arg(toolbarMessage).arg(toolbarControlsKey)
            .arg(toolbarGeometry.x(),0,'f',2).arg(toolbarGeometry.y(),0,'f',2).arg(toolbarGeometry.width(),0,'f',2).arg(toolbarGeometry.height(),0,'f',2)
            .arg(toolbarLook.glass+toolbarLook.controls*2+toolbarLook.words*4,0,'f',3).arg(toolbarLook.bright,0,'f',3).arg(toolbarMoving);
        // Paint no faster than frames reach the screen, but always paint the frame it comes to rest on.
        if (nextToolbarKey != toolbarKey && (!toolbarMoving || m_ControlsOpen || Uint32(toolbarNow-toolbarPaintedAt)>=toolbarFrameMs)) {
            toolbarKey = nextToolbarKey;
            toolbarPaintedAt = toolbarNow;
            if (m_ControlsOpen) m_OverlayManager.updateToolbar(nullptr);
            else m_OverlayManager.updateToolbar(StreamToolbar::copySurface(StreamToolbar::hud(toolbarWidth,toolbarDpr,toolbarFont,toolbarPalette,
                toolbarLook,toolbarControls,toolbarMessage,toolbarMetrics.topInset)));
        }
        if (m_ControlsOpen) QCoreApplication::processEvents(QEventLoop::AllEvents, 4);
        else if (Uint32(SDL_GetTicks() - lastAuxiliaryPump) >=
                 (qtClipboardEvents || m_FileTransfer->busy() ? 20u : 100u)) {
            lastAuxiliaryPump = SDL_GetTicks();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 4);
        }
#if SDL_VERSION_ATLEAST(2, 0, 18) && !defined(STEAM_LINK)
        // SDL 2.0.18 has a proper wait event implementation that uses platform
        // support to block on events rather than polling on Windows, macOS, X11,
        // and Wayland. It will fall back to 1 ms polling if a joystick is
        // connected, so we don't use it for STEAM_LINK to ensure we only poll
        // every 10 ms.
        //
        // NB: This behavior was introduced in SDL 2.0.16, but had a few critical
        // issues that could cause indefinite timeouts, delayed joystick detection,
        // and other problems.
        if (!SDL_WaitEventTimeout(&event, toolbarMoving ? toolbarFrameMs
                                           : (m_ControlsOpen || qtClipboardEvents || m_FileTransfer->busy() || toolbarExpanded) ? 16 : 1000)) {
            presence.runCallbacks();
            continue;
        }
#else
        // We explicitly use SDL_PollEvent() and SDL_Delay() because
        // SDL_WaitEvent() has an internal SDL_Delay(10) inside which
        // blocks this thread too long for high polling rate mice and high
        // refresh rate displays.
        if (!SDL_PollEvent(&event)) {
#ifndef STEAM_LINK
            SDL_Delay(1);
#else
            // Waking every 1 ms to process input is too much for the low performance
            // ARM core in the Steam Link, so we will wait 10 ms instead.
            SDL_Delay(10);
#endif
            presence.runCallbacks();
            continue;
        }
#endif
        if (!StreamUtils::actualSize() || m_InputHandler->isCaptureActive()) viewWheelPan.reset();
        if (m_ControlsOpen && ((event.type >= SDL_KEYDOWN && event.type <= SDL_MOUSEWHEEL) ||
                (event.type >= SDL_JOYAXISMOTION && event.type <= SDL_CONTROLLERDEVICEREMAPPED) ||
#if SDL_VERSION_ATLEAST(2, 0, 14)
                (event.type >= SDL_CONTROLLERTOUCHPADDOWN && event.type <= SDL_CONTROLLERSENSORUPDATE) ||
#endif
                (event.type >= SDL_FINGERDOWN && event.type <= SDL_MULTIGESTURE))) {
#if SDL_VERSION_ATLEAST(2, 0, 22)
            if (event.type == SDL_TEXTEDITING_EXT) SDL_free(event.editExt.text);
#endif
            continue;
        }
        switch (event.type) {
        case SDL_QUIT:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Quit event received");
            goto DispatchDeferredCleanup;

        case SDL_CLIPBOARDUPDATE:
            if (m_ClipboardPaused) break;
            if (m_ImageClipboardSupported && (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS)) {
                const auto value = ClientClipboardImage::read();
                if (value.image) {
                    noteClipboardChange(value.hash);
                    if (!value.png.isEmpty()) m_ClipboardSync->enqueueLocalImage(value.png);
                    else {
                        m_TextInputNotice = tr("Image not copied. Use a PNG up to 4 MiB and 4096 pixels per side.");
                        m_TextInputNoticeAt = SDL_GetTicks();
                    }
                    break;
                }
            }
            // Fires whenever the local (client-side) clipboard changes, including as
            // a side effect of the SDL_CODE_CLIPBOARD_APPLY handler's own
            // native publication below (the clip thread's loop guard
            // suppresses that echo once we enqueue it - see streaming/
            // clipboardsync.h for the full push/pull/echo-suppression design).
            //
            // The focus + empty + non-text guards run HERE, on the SDL thread,
            // because this is the only thread where SDL clipboard state and window
            // focus are safely readable (design §1.3). Everything after that -
            // size cap, hashing, the loop guard, the POST itself - runs on the
            // dedicated clipboard thread via enqueueLocalChange(), never here.
            //
            // Focus-scoped per design §5: SDL_CLIPBOARDUPDATE fires for ANY local
            // clipboard change regardless of which window/app has focus, so without
            // this gate a copy made in a different, unrelated app while the stream
            // window is backgrounded would leak to the host.
            if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0) {
                noteClipboardChange(currentClipboardHash());
            }
            if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0) {
                const auto text = NativeClipboard::readText();
                if (!text.isEmpty()) m_ClipboardSync->enqueueLocalChange(text);
            }
            break;

        case SDL_USEREVENT:
            switch (event.user.code) {
            case SDL_CODE_FRAME_READY:
                if (m_VideoDecoder != nullptr) {
                    m_VideoDecoder->renderFrameOnMainThread();
                }
                break;
            case SDL_CODE_FLUSH_WINDOW_EVENT_BARRIER:
                m_FlushingWindowEventsRef--;
                break;
            case SDL_CODE_GAMECONTROLLER_RUMBLE:
                m_InputHandler->rumble((uint16_t)(uintptr_t)event.user.data1,
                                       (uint16_t)((uintptr_t)event.user.data2 >> 16),
                                       (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_RUMBLE_TRIGGERS:
                m_InputHandler->rumbleTriggers((uint16_t)(uintptr_t)event.user.data1,
                                               (uint16_t)((uintptr_t)event.user.data2 >> 16),
                                               (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_MOTION_EVENT_STATE:
                m_InputHandler->setMotionEventState((uint16_t)(uintptr_t)event.user.data1,
                                                    (uint8_t)((uintptr_t)event.user.data2 >> 16),
                                                    (uint16_t)((uintptr_t)event.user.data2 & 0xFFFF));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_CONTROLLER_LED:
                m_InputHandler->setControllerLED((uint16_t)(uintptr_t)event.user.data1,
                                                 (uint8_t)((uintptr_t)event.user.data2 >> 16),
                                                 (uint8_t)((uintptr_t)event.user.data2 >> 8),
                                                 (uint8_t)((uintptr_t)event.user.data2));
                break;
            case SDL_CODE_GAMECONTROLLER_SET_ADAPTIVE_TRIGGERS:
                m_InputHandler->setAdaptiveTriggers((uint16_t)(uintptr_t)event.user.data1,
                                                    (DualSenseOutputReport *)event.user.data2);
                break;
            case SDL_CODE_CLIPBOARD_IMAGE_APPLY: {
                std::unique_ptr<QByteArray> png(static_cast<QByteArray*>(event.user.data1));
                if (png) applyClipboard({std::move(*png), reinterpret_cast<quintptr>(event.user.data2), true});
                break;
            }
            case SDL_CODE_CLIPBOARD_APPLY:
                // Marshalled from the dedicated clipboard thread (see
                // ClipboardSync::marshalApplyToSdlThread() in clipboardsync.cpp).
                // Clipboard publication belongs to this GUI/SDL thread.
                // data1 is an SDL_malloc'd null-terminated UTF-8 copy that we own
                // and must SDL_free().
                applyClipboard({QByteArray(static_cast<const char*>(event.user.data1)),
                                reinterpret_cast<quintptr>(event.user.data2), false});
                SDL_free(event.user.data1);
                // This will cause SDL to enqueue SDL_CLIPBOARDUPDATE, re-entering
                // the case above on a later loop iteration; the clip thread's loop
                // guard (already marked pulled before this event was pushed - see
                // ClipboardSync::handleLongPollResponse()) suppresses that echo.
                break;
            case SDL_CODE_CURSOR_SHAPE:
                if (m_CursorSync) {
                    m_CursorSync->applyAndDeleteFrame(event.user.data1);
                }
                break;
            default:
                SDL_assert(false);
            }
            break;

        case SDL_WINDOWEVENT:
#if SDL_VERSION_ATLEAST(2, 26, 0)
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                localDisplayScale = currentDisplayScale();
                m_LocalDisplayScale = localDisplayScale;
            }
#endif
            // Early handling of some events
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_LOST:
                toolbarPressed=StreamToolbar::None; toolbarHandlePress=false; remotePointerButtons=0;
                if (m_Preferences->muteOnFocusLoss) {
                    m_AudioMuted = true;
                }
                m_InputHandler->notifyFocusLost();
                updateLocalTextInput();
                clipboardFocusGuard.focusLost(currentClipboardHash());
                pendingClipboard = {};
                // Publish the focus transition first so the clip thread knows to
                // park (no host reads while backgrounded - design §5), then poke
                // it for one short-budget edge pull before it parks: the common
                // case is "copied on the remote host, now alt-tabbing to a local
                // app to paste" - grab the latest before focus actually leaves.
                m_ClipboardSync->setFocused(false);
                m_ClipboardSync->requestFocusEdgePull();
                if (m_CursorSync) {
                    m_CursorSync->setFocused(false);
                    m_CursorSync->resetCursor();
                }
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (m_Preferences->muteOnFocusLoss) {
                    m_AudioMuted = m_ManualAudioMuted;
                }
                m_InputHandler->notifyFocusGained();
                updateLocalTextInput();
                // Resumes the clip thread's steady-state long-poll loop
                // immediately (see ClipboardSync::run()); requestFocusEdgePull()
                // is kept for symmetry with FOCUS_LOST/design §1.4, though once
                // focused it's subsumed by the normal steady-state loop.
                m_ClipboardSync->setFocused(!m_ClipboardPaused);
                clipboardFocusGuard.focusGained(currentClipboardHash(), m_ClipboardSync->focusGeneration());
                refreshClipboardInput();
                m_ClipboardSync->requestFocusEdgePull();
                if (m_CursorSync) {
                    m_CursorSync->setFocused(m_CursorPolicy.syncShapes(m_InputHandler->isAbsoluteMouseMode()));
                }
                break;
            case SDL_WINDOWEVENT_LEAVE:
                toolbarPressed=StreamToolbar::None; toolbarHandlePress=false;
                m_InputHandler->notifyMouseLeave();
                break;
            }

            presence.runCallbacks();

            // Capture the mouse on SDL_WINDOWEVENT_ENTER if needed
                if (!m_ControlsOpen && needsFirstEnterCapture && event.window.event == SDL_WINDOWEVENT_ENTER) {
                m_InputHandler->setCaptureActive(true);
                needsFirstEnterCapture = false;
            }

            // We want to recreate the decoder for resizes (full-screen toggles) and the initial shown event.
            // We use SDL_WINDOWEVENT_SIZE_CHANGED rather than SDL_WINDOWEVENT_RESIZED because the latter doesn't
            // seem to fire when switching from windowed to full-screen on X11.
            if (event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED &&
                (event.window.event != SDL_WINDOWEVENT_SHOWN || m_VideoDecoder != nullptr)) {
                // Check that the window display hasn't changed. If it has, we want
                // to recreate the decoder to allow it to adapt to the new display.
                // This will allow Pacer to pull the new display refresh rate.
#if SDL_VERSION_ATLEAST(2, 0, 18)
                // On SDL 2.0.18+, there's an event for this specific situation
                if (event.window.event != SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                    break;
                }
#else
                // Prior to SDL 2.0.18, we must check the display index for each window event
                if (SDL_GetWindowDisplayIndex(m_Window) == currentDisplayIndex) {
                    break;
                }
#endif
            }
#ifdef Q_OS_WIN32
            // We can get a resize event after being minimized. Recreating the renderer at that time can cause
            // us to start drawing on the screen even while our window is minimized. Minimizing on Windows also
            // moves the window to -32000, -32000 which can cause a false window display index change. Avoid
            // that whole mess by never recreating the decoder if we're minimized.
            else if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
                break;
            }
#endif

            if (m_FlushingWindowEventsRef > 0) {
                // Ignore window events for renderer reset if flushing
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Dropping window event during flush: %d (%d %d)",
                            event.window.event,
                            event.window.data1,
                            event.window.data2);
                break;
            }

            // Allow the renderer to handle the state change without being recreated
            if (m_VideoDecoder) {
                bool forceRecreation = false;

                WINDOW_STATE_CHANGE_INFO windowChangeInfo = {};
                windowChangeInfo.window = m_Window;

                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    windowChangeInfo.stateChangeFlags |= WINDOW_STATE_CHANGE_SIZE;

                    windowChangeInfo.width = event.window.data1;
                    windowChangeInfo.height = event.window.data2;
                }

                int newDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
                if (newDisplayIndex != currentDisplayIndex) {
                    windowChangeInfo.stateChangeFlags |= WINDOW_STATE_CHANGE_DISPLAY;

                    windowChangeInfo.displayIndex = newDisplayIndex;

                    // If the refresh rates have changed, we will need to go through the full
                    // decoder recreation path to ensure Pacer is switched to the new display
                    // and that we apply any V-Sync disablement rules that may be needed for
                    // this display.
                    SDL_DisplayMode oldMode, newMode;
                    if (SDL_GetCurrentDisplayMode(currentDisplayIndex, &oldMode) < 0 ||
                            SDL_GetCurrentDisplayMode(newDisplayIndex, &newMode) < 0 ||
                            oldMode.refresh_rate != newMode.refresh_rate) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                    "Forcing renderer recreation due to refresh rate change between displays");
                        forceRecreation = true;
                    }
                }

                if (!forceRecreation && m_VideoDecoder->notifyWindowChanged(&windowChangeInfo)) {
                    // Update the window display mode based on our current monitor
                    // NB: Avoid a useless modeset by only doing this if it changed.
                    if (newDisplayIndex != currentDisplayIndex) {
                        currentDisplayIndex = newDisplayIndex;
                        updateOptimalWindowDisplayMode();
                    }

                    break;
                }
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Recreating renderer for window event: %d (%d %d)",
                        event.window.event,
                        event.window.data1,
                        event.window.data2);

            // Fall through
        case SDL_RENDER_DEVICE_RESET:

            if (event.type != SDL_WINDOWEVENT) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Recreating renderer by internal request: %d",
                            event.type);
            }

            SDL_LockMutex(m_DecoderLock);

            // Destroy the old decoder
            delete m_VideoDecoder;

            // Insert a barrier to discard any additional window events
            // that could cause the renderer to be and recreated again.
            // We don't use SDL_FlushEvent() here because it could cause
            // important events to be lost.
            flushWindowEvents();

            // Update the window display mode based on our current monitor
            // NB: Avoid a useless modeset by only doing this if it changed.
            if (currentDisplayIndex != SDL_GetWindowDisplayIndex(m_Window)) {
                currentDisplayIndex = SDL_GetWindowDisplayIndex(m_Window);
                updateOptimalWindowDisplayMode();
            }

            // Now that the old decoder is dead, flush any events it may
            // have queued to reset itself (if this reset was the result
            // of device loss or an internal error).
            SDL_PumpEvents();
            SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);

            {
                // If the stream exceeds the display refresh rate (plus some slack),
                // forcefully disable V-sync to allow the stream to render faster
                // than the display.
                int displayHz = StreamUtils::getDisplayRefreshRate(m_Window);
                bool enableVsync = m_Preferences->enableVsync;
                if (displayHz + 5 < m_StreamConfig.fps) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Disabling V-sync because refresh rate limit exceeded");
                    enableVsync = false;
                }

                // Choose a new decoder (hopefully the same one, but possibly
                // not if a GPU was removed or something).
                if (!chooseDecoder(m_Preferences->videoDecoderSelection,
                                   m_Window, m_ActiveVideoFormat, m_ActiveVideoWidth,
                                   m_ActiveVideoHeight, m_ActiveVideoFrameRate,
                                   enableVsync,
                                   framePacingEnabled(enableVsync),
                                   false,
                                   s_ActiveSession->m_VideoDecoder)) {
                    SDL_UnlockMutex(m_DecoderLock);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to recreate decoder after reset");
                    emit displayLaunchError(tr("Unable to initialize video decoder. Please check your streaming settings and try again."));
                    goto DispatchDeferredCleanup;
                }

                // The previous renderer owned the toolbar surface. Upload it
                // again even when the HUD is settled and its appearance is unchanged.
                toolbarKey.clear();

                // As of SDL 2.0.12, SDL_RecreateWindow() doesn't carry over mouse capture
                // or mouse hiding state to the new window. By capturing after the decoder
                // is set up, this ensures the window re-creation is already done.
                if (needsPostDecoderCreationCapture) {
                    m_InputHandler->setCaptureActive(true);
                    needsPostDecoderCreationCapture = false;
                }
                // SDL can replace the native surface while creating the first
                // renderer, losing an asynchronous Wayland fullscreen request.
                // Reassert once after that replacement, before the first frame.
                if (needsInitialWaylandFullscreen) {
                    SDL_SetWindowFullscreen(m_Window, m_FullScreenFlag);
                    needsInitialWaylandFullscreen = false;
                }
            }

            // Request an IDR frame to complete the reset
            LiRequestIdrFrame();

            // Set HDR mode. We may miss the callback if we're in the middle
            // of recreating our decoder at the time the HDR transition happens.
            m_VideoDecoder->setHdrMode(LiGetCurrentHostDisplayHdrMode());

            // After a window resize, we need to reset the pointer lock region
            m_InputHandler->updatePointerRegionLock();

            SDL_UnlockMutex(m_DecoderLock);
            break;

        case SDL_TEXTINPUT:
            if (m_Preferences->localTextInput && (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS)) {
                m_CompositionText.clear();
                m_InputHandler->setTextComposition(false);
                if (LiSendUtf8TextEvent(event.text.text, unsigned(strlen(event.text.text))) != 0) {
                    m_TextInputNotice = tr("Couldn't send the composed text. Try again, or turn off local keyboard layout in Stream Controls.");
                    m_TextInputNoticeAt = SDL_GetTicks();
                }
            }
            break;
        case SDL_DROPBEGIN:
            droppedFiles.clear(); inFileDrop = true; break;
        case SDL_DROPFILE: {
            const QUrl file = QUrl::fromLocalFile(QString::fromUtf8(event.drop.file));
            SDL_free(event.drop.file);
            // Retain one overflow sentinel so the queue rejects the whole
            // oversized batch with its limit message instead of copying a subset.
            if (droppedFiles.size() <= 32) droppedFiles.append(file);
            if (!inFileDrop) {
                openControls();
                if (m_ControlsOpen) emit filesDropRequested(droppedFiles);
                droppedFiles.clear();
            }
            break;
        }
        case SDL_DROPCOMPLETE:
            inFileDrop = false;
            if (!droppedFiles.isEmpty()) {
                openControls();
                if (m_ControlsOpen) emit filesDropRequested(droppedFiles);
                droppedFiles.clear();
            }
            break;
        case SDL_TEXTEDITING:
            if (m_Preferences->localTextInput) {
                m_CompositionText = QString::fromUtf8(event.edit.text);
                m_InputHandler->setTextComposition(!m_CompositionText.isEmpty());
            }
            break;
#if SDL_VERSION_ATLEAST(2, 0, 22)
        case SDL_TEXTEDITING_EXT:
            if (m_Preferences->localTextInput) {
                m_CompositionText = QString::fromUtf8(event.editExt.text);
                m_InputHandler->setTextComposition(!m_CompositionText.isEmpty());
            }
            SDL_free(event.editExt.text);
            break;
#endif
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            if (!event.key.repeat) refreshClipboardInput();
            presence.runCallbacks();
            m_InputHandler->handleKeyEvent(&event.key);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            refreshClipboardInput();
            presence.runCallbacks();
            if (event.button.button == SDL_BUTTON_LEFT && !SDL_GetRelativeMouseMode()) {
                const QPoint point=toolbarMetrics.point(event.button.x,event.button.y);
                // The pill is a button: pressing squeezes it, releasing on it opens the bar.
                if (!toolbarExpanded && !m_ControlsOpen) {
                    if (event.type == SDL_MOUSEBUTTONDOWN && remotePointerButtons==0 && StreamToolbar::onHandle(toolbarWidth,point)) {
                        toolbarHandlePress=true; break;
                    }
                    if (event.type == SDL_MOUSEBUTTONUP && toolbarHandlePress) {
                        toolbarHandlePress=false;
                        if (StreamToolbar::onHandle(toolbarWidth,point,16)) {
                            toolbarExpanded=true; toolbarHeldOpen=true; toolbarRevealAt=SDL_GetTicks();
                        }
                        break;
                    }
                }
                const auto hit = toolbarExpanded ? StreamToolbar::hitTest(toolbarWidth,point) : StreamToolbar::None;
                // A press on the bar's own background belongs to the bar, not the host behind it.
                if (toolbarExpanded && hit==StreamToolbar::None && remotePointerButtons==0
                        && StreamToolbar::bounds(toolbarWidth).contains(point)) break;
                if (event.type == SDL_MOUSEBUTTONDOWN && hit != StreamToolbar::None && remotePointerButtons==0) {
                    toolbarPressed=hit; toolbarRevealAt=SDL_GetTicks(); break;
                }
                if (event.type == SDL_MOUSEBUTTONUP && toolbarPressed != StreamToolbar::None) {
                    const auto action=toolbarPressed; toolbarPressed=StreamToolbar::None;
                    if (hit==action) dispatchToolbar(action);
                    break;
                }
                // A click elsewhere puts the bar away, and still reaches the host.
                if (toolbarExpanded && event.type == SDL_MOUSEBUTTONDOWN && remotePointerButtons==0) {
                    toolbarExpanded=false; toolbarHeldOpen=false;
                }
            }
            if (event.button.button<32) {
                if (event.type==SDL_MOUSEBUTTONDOWN) remotePointerButtons |= SDL_BUTTON(event.button.button);
                else remotePointerButtons &= ~SDL_BUTTON(event.button.button);
            }
            m_InputHandler->handleMouseButtonEvent(&event.button);
            break;
        case SDL_MOUSEMOTION:
            if (toolbarPressed!=StreamToolbar::None || toolbarHandlePress) break;
            if (!SDL_GetRelativeMouseMode() && remotePointerButtons==0 &&
                (toolbarExpanded ? StreamToolbar::bounds(toolbarWidth).contains(toolbarMetrics.point(event.motion.x,event.motion.y))
                    : StreamToolbar::onHandle(toolbarWidth,toolbarMetrics.point(event.motion.x,event.motion.y)))) {
                SDL_ShowCursor(SDL_ENABLE);
                break;
            }
            m_InputHandler->handleMouseMotionEvent(&event.motion);
            break;
        case SDL_MOUSEWHEEL:
            if (StreamUtils::actualSize() && !m_InputHandler->isCaptureActive()) {
                const auto delta = viewWheelPan.take(event.wheel);
                StreamUtils::panView(delta.x(),delta.y(),
                    m_Window,m_StreamConfig.width,m_StreamConfig.height);
                SDL_Event redraw {}; redraw.type = SDL_WINDOWEVENT;
                redraw.window.windowID = SDL_GetWindowID(m_Window);
                redraw.window.event = SDL_WINDOWEVENT_EXPOSED; SDL_PushEvent(&redraw);
                break;
            }
            m_InputHandler->handleMouseWheelEvent(&event.wheel);
            break;
        case SDL_CONTROLLERAXISMOTION:
            m_InputHandler->handleControllerAxisEvent(&event.caxis);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            presence.runCallbacks();
            m_InputHandler->handleControllerButtonEvent(&event.cbutton);
            break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        case SDL_CONTROLLERSENSORUPDATE:
            m_InputHandler->handleControllerSensorEvent(&event.csensor);
            break;
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADUP:
        case SDL_CONTROLLERTOUCHPADMOTION:
            m_InputHandler->handleControllerTouchpadEvent(&event.ctouchpad);
            break;
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_JOYBATTERYUPDATED:
            m_InputHandler->handleJoystickBatteryEvent(&event.jbattery);
            break;
#endif
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            m_InputHandler->handleControllerDeviceEvent(&event.cdevice);
            break;
        case SDL_JOYDEVICEADDED:
            m_InputHandler->handleJoystickArrivalEvent(&event.jdevice);
            break;
        case SDL_FINGERDOWN:
            refreshClipboardInput();
            [[fallthrough]];
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            m_InputHandler->handleTouchFingerEvent(&event.tfinger);
            break;
        case SDL_DISPLAYEVENT:
            switch (event.display.event) {
            case SDL_DISPLAYEVENT_CONNECTED:
            case SDL_DISPLAYEVENT_DISCONNECTED:
                m_InputHandler->updatePointerRegionLock();
                break;
            }
            break;
        }
    }

DispatchDeferredCleanup:
    if (!m_AdjustingForHost && !m_RejectTrialMeasurement) rememberStreamMeasurements();
    rememberPacing();
    if (m_AutomaticBitrate) {
        m_AutomaticBitrate->stopAndWait();
        if (const int settled = m_AutomaticBitrate->settledKbps())
            StreamingPreferences::rememberAutomaticBitrate(m_Computer->uuid, settled, m_BitrateContext);
        disconnect(m_AutomaticBitrate, nullptr, this, nullptr);
    }
    if (m_RuntimeBitrate) {
        m_RuntimeBitrate->stopAndWait();
        disconnect(m_RuntimeBitrate, nullptr, this, nullptr);
        delete m_RuntimeBitrate; m_RuntimeBitrate = nullptr;
        m_RuntimeBitrateSupported = false;
    }
    healthCancellation.cancel();
    if (healthThread) { healthThread->wait(); delete healthThread; }
    if (m_ControlsOpen) { m_ControlsOpen = false; emit controlsChanged(); }
    SDL_StopTextInput();
    disconnect(clipboardConnection);
    delete m_FileTransfer;
    m_FileTransfer = nullptr;
    emit transferChanged();
    // Switch back to synchronous logging mode
    StreamUtils::exitAsyncLoggingMode();

    if (m_CursorSync) {
        m_CursorSync->stopAndWait();
        delete m_CursorSync;
        m_CursorSync = nullptr;
    }

    // Tear down the clipboard thread now, while we're still on the SDL/main thread,
    // before Qt processing resumes and before m_Computer/the Session itself go
    // away. stopAndWait() cancels any in-flight long-poll (so this does not block
    // for the host's ~25-30s hold budget), wakes anything parked, and joins the
    // thread - see clipboardsync.cpp. The destructor also calls stopAndWait() as a
    // safety net, but call it explicitly here so any failure is attributable to
    // this teardown site rather than an implicit destructor path.
    m_ClipboardSync->stopAndWait();
    ClipboardSync::discardQueuedApplyEvents();
    delete m_ClipboardSync;
    m_ClipboardSync = nullptr;

    // Uncapture the mouse and hide the window immediately,
    // so we can return to the Qt GUI ASAP.
    m_InputHandler->setCaptureActive(false);
    SDL_EnableScreenSaver();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");
    if (QGuiApplication::platformName() == "eglfs") {
        QGuiApplication::restoreOverrideCursor();
    }

    // Raise any keys that are still down
    m_InputHandler->raiseAllKeys();

    // Destroy the input handler now. This must be destroyed
    // before allowwing the UI to continue execution or it could
    // interfere with SDLGamepadKeyNavigation.
    delete m_InputHandler;
    m_InputHandler = nullptr;

    // Destroy the decoder, since this must be done on the main thread
    // NB: This must happen before LiStopConnection() for pull-based
    // decoders.
    SDL_LockMutex(m_DecoderLock);
    delete m_VideoDecoder;
    m_VideoDecoder = nullptr;
    SDL_UnlockMutex(m_DecoderLock);

    // Propagate state changes from the SDL window back to the Qt window
    //
    // NB: We're making a conscious decision not to propagate the maximized
    // or normal state of the window here. The thinking is that users may
    // routinely maximize the streaming window simply to view the stream
    // in a larger window, but they don't necessarily want the UI in such
    // a large window.
    if (!m_IsFullScreen && m_QtWindow != nullptr && m_Window != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
            m_QtWindow->setWindowStates(m_QtWindow->windowStates() | Qt::WindowMinimized);
        }
        else if (m_QtWindow->windowStates() & Qt::WindowMinimized) {
            m_QtWindow->setWindowStates(m_QtWindow->windowStates() & ~Qt::WindowMinimized);
        }
#else
        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
            m_QtWindow->setWindowState(Qt::WindowMinimized);
        }
        else if (m_QtWindow->windowState() & Qt::WindowMinimized) {
            m_QtWindow->setWindowState(Qt::WindowNoState);
        }
#endif

        // NB: Per owner decision (reversing part of the "item 5" geometry
        // continuity change), we do NOT propagate the SDL stream window's
        // final size/position back to the Qt launcher window at teardown
        // anymore. Resizing the stream window while streaming must not
        // resize the launcher. Only minimized-state propagation (above)
        // survives from that change; start-path inheritance in
        // getWindowDimensions() is unaffected.
    }

    // Preserve our known clipboard payload before its SDL video owner disappears.
    NativeClipboard::finishSession();

    // This must be called after the decoder is deleted, because
    // the renderer may want to interact with the window
    SDL_DestroyWindow(m_Window);
    // Route destruction may cancel its retained Session while the remote end
    // is still running. There is no SDL loop to interrupt after this point.
    m_Window = nullptr;

    if (iconSurface != nullptr) {
        SDL_FreeSurface(iconSurface);
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    // Return the launcher before the worker is scheduled or sends remote HTTP.
    // Failed/interrupted sessions keep their existing error/recovery route.
    if (!m_UnexpectedTermination && !retryableTermination() && !m_AdjustingForHost)
        QMetaObject::invokeMethod(this, [this] { emit quitStarting(); }, Qt::QueuedConnection);

    // Cleanup can take a while, so dispatch it to a worker thread.
    // When it is complete, it will release our s_ActiveSessionSemaphore
    // reference.
    QThreadPool::globalInstance()->start(new DeferredSessionCleanupTask(this));
}
