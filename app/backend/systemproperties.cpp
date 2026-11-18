#include "systemproperties.h"
#include "../buildinfo.h"
#include "utils.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QLibraryInfo>
#include <stdexcept>
#include <thread>

#ifndef DECODER_PROBE_TIMEOUT_MS
#define DECODER_PROBE_TIMEOUT_MS 15000
#endif

#include "streaming/session.h"
#include "streaming/streamutils.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

class SystemPropertyQueryThread : public QThread
{
public:
    SystemPropertyQueryThread(SystemProperties* properties)
        : QThread(properties), m_Properties(properties)
    {
        setObjectName("System Properties Async Query Thread");
    }

    bool usable = false;
    bool hasHardwareAcceleration = false;
    bool rendererAlwaysFullScreen = false;
    bool supportsHdr = false;
    QSize maximumResolution;
    bool hasHwHevcOrAv1Decode = false;

private:
    void run() override
    {
        try {
            // Qt 6.4 reports finished()/wait() before native thread_local
            // destructors finish. Keep all driver work on a joinable native
            // thread, and publish only after its complete teardown is joined.
            std::thread probe([this] {
                try {
                    usable = Session::getDecoderInfo(m_Properties->testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen, supportsHdr, maximumResolution, hasHwHevcOrAv1Decode);
                } catch (...) {
                    usable = false;
                }
            });
            probe.join();
        } catch (...) {
            // This also covers a failure to create the native probe thread.
            usable = false;
        }
        if (!usable) {
            hasHardwareAcceleration = rendererAlwaysFullScreen = supportsHdr = hasHwHevcOrAv1Decode = false;
            maximumResolution = QSize();
        }
    }

private:
    SystemProperties* m_Properties;
};

SystemProperties::SystemProperties()
{
    m_DecoderProbeWatchdog.setSingleShot(true);
    m_DecoderProbeWatchdog.setInterval(DECODER_PROBE_TIMEOUT_MS);
    connect(&m_DecoderProbeWatchdog, &QTimer::timeout, this, [this] {
        if (!m_DecoderProbeRunning) return;
        m_DecoderProbeError = tr("The video capability check has not finished. Return to hosts and try again once it completes.");
        emit decoderProbeStateChanged();
    });
    const auto watchScreen = [this](QScreen* screen) {
        connect(screen, &QScreen::geometryChanged, this, &SystemProperties::displayConfigurationChanged);
        connect(screen, &QScreen::refreshRateChanged, this, &SystemProperties::displayConfigurationChanged);
    };
    for (auto screen : QGuiApplication::screens()) watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watchScreen](QScreen* screen) {
        watchScreen(screen);
        emit displayConfigurationChanged();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &SystemProperties::displayConfigurationChanged);

    versionString = QString(VERSION_STR);
    starlightVersionString = QString(STARLIGHT_VERSION_STR);
    hasDesktopEnvironment = WMUtils::isRunningDesktopEnvironment();
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";
    usesMaterial3Theme = QLibraryInfo::version() >= QVersionNumber(6, 5, 0);
    QString nativeArch = QSysInfo::currentCpuArchitecture();

#ifdef Q_OS_WIN32
    {
        USHORT processArch, machineArch;

        // Use IsWow64Process2() because it doesn't lie on ARM64
        if (IsWow64Process2(GetCurrentProcess(), &processArch, &machineArch)) {
            switch (machineArch) {
            case IMAGE_FILE_MACHINE_I386:
                nativeArch = "i386";
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                nativeArch = "x86_64";
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                nativeArch = "arm64";
                break;
            }
        }

        isWow64 = nativeArch != QSysInfo::buildCpuArchitecture();
    }
#else
    isWow64 = false;
#endif

    if (nativeArch == "i386") {
        friendlyNativeArchName = "x86";
    }
    else if (nativeArch == "x86_64") {
        friendlyNativeArchName = "x64";
    }
    else {
        friendlyNativeArchName = nativeArch.toUpper();
    }

    // Assume we can probably launch a browser if we're in a GUI environment
    hasBrowser = hasDesktopEnvironment;

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    // These will be queried asynchronously to avoid blocking the UI
    hasHardwareAcceleration = false;
    rendererAlwaysFullScreen = false;
    supportsHdr = false;
    maximumResolution = QSize(0, 0);
    hasHwHevcOrAv1Decode = false;
}

SystemProperties::~SystemProperties()
{
    waitForAsyncLoad();
}

void SystemProperties::updateDecoderProperties(bool usable, bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr, bool hasHwHevcOrAv1Decode)
{
    SDL_assert(testWindow);

    if (hasHardwareAcceleration != this->hasHardwareAcceleration) {
        this->hasHardwareAcceleration = hasHardwareAcceleration;
        emit hasHardwareAccelerationChanged();
    }

    if (rendererAlwaysFullScreen != this->rendererAlwaysFullScreen) {
        this->rendererAlwaysFullScreen = rendererAlwaysFullScreen;
        emit rendererAlwaysFullScreenChanged();
    }

    if (maximumResolution != this->maximumResolution) {
        this->maximumResolution = maximumResolution;
        emit maximumResolutionChanged();
    }

    if (supportsHdr != this->supportsHdr) {
        this->supportsHdr = supportsHdr;
        emit supportsHdrChanged();
    }

    if (hasHwHevcOrAv1Decode != this->hasHwHevcOrAv1Decode) {
        this->hasHwHevcOrAv1Decode = hasHwHevcOrAv1Decode;
        emit hasHwHevcOrAv1DecodeChanged();
    }

    SDL_DestroyWindow(testWindow);
    testWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    m_DecoderProbeReady = usable;
    m_DecoderProbeRunning = false;
    m_DecoderProbeError = usable ? QString() : tr("No usable video decoder was found. Check the video driver and decoder settings, then try the video check again.");
    emit decoderProbeStateChanged();
    emit decoderProbeReadyChanged();
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds

    return m_Displays.displays.value(displayIndex).nativeResolution;
}

QRect SystemProperties::getSafeAreaResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds

    return m_Displays.displays.value(displayIndex).safeAreaResolution;
}

int SystemProperties::getRefreshRate(int displayIndex)
{
    // Returns 0 if out of bounds

    return m_Displays.displays.value(displayIndex).refreshRate;
}

void SystemProperties::startAsyncLoad()
{
    if (m_DecoderProbeRunning || m_DecoderProbeReady) return;
    if (systemPropertyQueryThread) {
        systemPropertyQueryThread->wait();
        delete systemPropertyQueryThread;
        systemPropertyQueryThread = nullptr;
    }
    m_DecoderProbeError.clear();
    emit decoderProbeStateChanged();

    // This isn't actually asynchronous (due to the need to synchronize with
    // SdlGamepadKeyNavigation), but we don't query it in the constructor
    // because it's expensive.
    unmappedGamepads = SdlInputHandler::getUnmappedGamepads();
    if (!unmappedGamepads.isEmpty()) {
        emit unmappedGamepadsChanged();
    }

    // We initialize the video subsystem and test window on the main thread
    // because some platforms (macOS) do not support window creation on
    // non-main threads.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        m_DecoderProbeError = tr("Video could not be initialized on this device. Check the video driver and try the video check again.");
        emit decoderProbeStateChanged();
        return;
    }

    testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        m_DecoderProbeError = tr("A video test window could not be created. Check the display connection and try the video check again.");
        emit decoderProbeStateChanged();
        return;
    }

    // Update display related attributes (max FPS, native resolution, etc).
    //
    // NB: SDL3 will forcefully refresh displays when a window is created,
    // so we place this after the window creation to ensure we don't pay
    // the penalty for mode enumeration twice.
    refreshDisplays();

    m_DecoderProbeReady = false;
    emit decoderProbeReadyChanged();
    m_DecoderProbeRunning = true;
    emit decoderProbeStateChanged();
    systemPropertyQueryThread = new SystemPropertyQueryThread(this);
    connect(systemPropertyQueryThread, &QThread::finished, this, &SystemProperties::finishDecoderProbe);
    m_DecoderProbeWatchdog.start();
    systemPropertyQueryThread->start();
}

void SystemProperties::waitForAsyncLoad()
{
    if (systemPropertyQueryThread) {
        systemPropertyQueryThread->wait();
        finishDecoderProbe();
    }
}

void SystemProperties::finishDecoderProbe()
{
    if (!m_DecoderProbeRunning || !systemPropertyQueryThread) return;
    // The wrapper has already joined all driver work and its native TLS. This
    // nonblocking wait only reaps the Qt notification thread; it never owns SDL.
    if (!systemPropertyQueryThread->wait(0)) {
        QTimer::singleShot(10, this, &SystemProperties::finishDecoderProbe);
        return;
    }
    m_DecoderProbeWatchdog.stop();
    const auto result = static_cast<SystemPropertyQueryThread*>(systemPropertyQueryThread);
    updateDecoderProperties(result->usable, result->hasHardwareAcceleration, result->rendererAlwaysFullScreen,
                            result->maximumResolution, result->supportsHdr, result->hasHwHevcOrAv1Decode);
}

void SystemProperties::refreshDisplays()
{
    // Failure invalidates the old snapshot, so a removed screen cannot seed a
    // newly added host with an obsolete display mode.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        m_Displays = {};
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
        return;
    }

    m_Displays = DisplaySnapshot::capture(SDL_GetNumVideoDisplays(), [](int displayIndex) {
        DisplayMetadata display;
        display.name = QString::fromUtf8(SDL_GetDisplayName(displayIndex));
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(displayIndex, &bounds) == 0)
            display.bounds = QRect(bounds.x, bounds.y, bounds.w, bounds.h);
        SDL_DisplayMode desktopMode;
        SDL_Rect safeArea;
        if (!StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea)) return display;
        if (desktopMode.w <= 8192 && desktopMode.h <= 8192) {
            display.nativeResolution = QRect(0, 0, desktopMode.w, desktopMode.h);
            display.safeAreaResolution = QRect(0, 0, safeArea.w, safeArea.h);
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Skipping resolution over 8K: %dx%d",
                        desktopMode.w, desktopMode.h);
        }
        // Follow the mode currently in use. A supported 144 Hz mode must not
        // make a display running at 60 Hz request a 144 FPS remote session.
        SDL_DisplayMode currentMode;
        int refreshRate = SDL_GetCurrentDisplayMode(displayIndex, &currentMode) == 0
                ? currentMode.refresh_rate : desktopMode.refresh_rate;
        // Normalize small reporting differences around standard refresh rates.
        if (refreshRate >= 58 && refreshRate <= 62) refreshRate = 60;
        else if (refreshRate >= 28 && refreshRate <= 32) refreshRate = 30;
        display.refreshRate = refreshRate;
        return display;
    });
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int SystemProperties::displayIndexForWindow(QObject* object)
{
    auto window = qobject_cast<QWindow*>(object);
    auto screen = window ? window->screen() : QGuiApplication::primaryScreen();
    // Qt/SDL enumerate independently. Resolve an actual identity/bounds match;
    // an ambiguous or missing screen returns unknown rather than display zero.
    return screen ? m_Displays.indexForScreen(screen->name(), screen->geometry(), screen->devicePixelRatio()) : -1;
}
