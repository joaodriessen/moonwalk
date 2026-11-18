#pragma once

#include <QObject>
#include <QRect>
#include <QTimer>

#include "SDL_compat.h"
#include "displaysnapshot.h"

class SystemProperties : public QObject
{
    Q_OBJECT

    friend class SystemPropertyQueryThread;

public:
    SystemProperties();
    ~SystemProperties();

    // Static properties queried synchronously during the constructor
    Q_PROPERTY(bool isRunningWayland MEMBER isRunningWayland CONSTANT)
    Q_PROPERTY(bool isRunningXWayland MEMBER isRunningXWayland CONSTANT)
    Q_PROPERTY(bool isWow64 MEMBER isWow64 CONSTANT)
    Q_PROPERTY(QString friendlyNativeArchName MEMBER friendlyNativeArchName CONSTANT)
    Q_PROPERTY(bool hasDesktopEnvironment MEMBER hasDesktopEnvironment CONSTANT)
    Q_PROPERTY(bool hasBrowser MEMBER hasBrowser CONSTANT)
    Q_PROPERTY(bool hasDiscordIntegration MEMBER hasDiscordIntegration CONSTANT)
    Q_PROPERTY(bool usesMaterial3Theme MEMBER usesMaterial3Theme CONSTANT)
    Q_PROPERTY(QString versionString MEMBER versionString CONSTANT)
    Q_PROPERTY(QString starlightVersionString MEMBER starlightVersionString CONSTANT)

    // Properties queried asynchronously (startAsyncLoad() must be called!)
    Q_PROPERTY(bool hasHardwareAcceleration MEMBER hasHardwareAcceleration NOTIFY hasHardwareAccelerationChanged)
    Q_PROPERTY(bool rendererAlwaysFullScreen MEMBER rendererAlwaysFullScreen NOTIFY rendererAlwaysFullScreenChanged)
    Q_PROPERTY(QString unmappedGamepads MEMBER unmappedGamepads NOTIFY unmappedGamepadsChanged)
    Q_PROPERTY(QSize maximumResolution MEMBER maximumResolution NOTIFY maximumResolutionChanged)
    Q_PROPERTY(bool supportsHdr MEMBER supportsHdr NOTIFY supportsHdrChanged)
    // True iff a hardware HEVC or AV1 decoder probed successfully (Session::
    // getDecoderInfo() FORCE_HARDWARE branches) -- a reliable proxy for
    // "modern decode silicon". Used by PcView.qml's native-resolution
    // auto-default (design-native-first.md V.1) to cap H.264-only clients at
    // 1080p instead of handing them a native resolution they can't sustain.
    Q_PROPERTY(bool hasHwHevcOrAv1Decode MEMBER hasHwHevcOrAv1Decode NOTIFY hasHwHevcOrAv1DecodeChanged)
    Q_PROPERTY(bool decoderProbeReady READ isDecoderProbeReady NOTIFY decoderProbeReadyChanged)
    Q_PROPERTY(QString decoderProbeError READ decoderProbeError NOTIFY decoderProbeStateChanged)
    Q_PROPERTY(bool decoderProbeRunning READ decoderProbeRunning NOTIFY decoderProbeStateChanged)

    // Either startAsyncLoad()+waitForAsyncLoad() or refreshDisplays() must be invoked first
    Q_INVOKABLE QRect getNativeResolution(int displayIndex);
    Q_INVOKABLE QRect getSafeAreaResolution(int displayIndex);
    Q_INVOKABLE int getRefreshRate(int displayIndex);

    Q_INVOKABLE void startAsyncLoad();
    Q_INVOKABLE void waitForAsyncLoad();
    bool isDecoderProbeReady() const { return m_DecoderProbeReady; }
    QString decoderProbeError() const { return m_DecoderProbeError; }
    bool decoderProbeRunning() const { return m_DecoderProbeRunning; }
    Q_INVOKABLE void refreshDisplays();
    Q_INVOKABLE int displayIndexForWindow(QObject* window);

signals:
    void displayConfigurationChanged();
    void unmappedGamepadsChanged();
    void hasHardwareAccelerationChanged();
    void rendererAlwaysFullScreenChanged();
    void maximumResolutionChanged();
    void supportsHdrChanged();
    void hasHwHevcOrAv1DecodeChanged();
    void decoderProbeReadyChanged();
    void decoderProbeStateChanged();

private slots:
    void updateDecoderProperties(bool usable, bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr, bool hasHwHevcOrAv1Decode);

private:
    QThread* systemPropertyQueryThread = nullptr;
    void finishDecoderProbe();
    bool m_DecoderProbeReady = false;
    bool m_DecoderProbeRunning = false;
    QString m_DecoderProbeError;
    QTimer m_DecoderProbeWatchdog;
    SDL_Window* testWindow = nullptr;

    // Properties set by the constructor
    bool isRunningWayland;
    bool isRunningXWayland;
    bool isWow64;
    QString friendlyNativeArchName;
    bool hasDesktopEnvironment;
    bool hasBrowser;
    bool hasDiscordIntegration;
    QString versionString;
    QString starlightVersionString;
    bool usesMaterial3Theme;

    // Properties only set if startAsyncLoad() is called
    bool hasHardwareAcceleration;
    bool rendererAlwaysFullScreen;
    QSize maximumResolution;
    bool supportsHdr;
    bool hasHwHevcOrAv1Decode;
    QString unmappedGamepads;

    // Properties set by refreshDisplays()
    DisplaySnapshot m_Displays;
};
