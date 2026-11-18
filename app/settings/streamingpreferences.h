#pragma once

#include <QObject>
#include <QRect>
#include <QQmlEngine>
#include <QVariant>
#include <QHash>
#include <QSettings>

#include <memory>

class LanguageWorkflow;

class StreamingPreferences : public QObject
{
    Q_OBJECT

public:
    ~StreamingPreferences() override;

    static StreamingPreferences* get(QQmlEngine *qmlEngine = nullptr);

    // Takes ownership of an explicit store and replaces the process singleton.
    // A missing store fails fast rather than falling through to the user's
    // platform-default QSettings location.
    static StreamingPreferences* getForTesting(std::unique_ptr<QSettings> settings);
    static void resetForTesting();

    Q_INVOKABLE static int
    getDefaultBitrate(int width, int height, int fps, bool yuv444);

    Q_INVOKABLE bool setResolution(int width, int height);
    Q_INVOKABLE bool setFrameRate(int fps);
    Q_INVOKABLE bool setAutomaticFrameRate(int nativeRefreshRate);
    Q_INVOKABLE bool setAutomaticResolution(int nativeWidth, int nativeHeight);
    Q_INVOKABLE bool initializeHostDisplayMode(const QString& uuid,
                                               int nativeWidth,
                                               int nativeHeight,
                                               int nativeRefreshRate);
    Q_INVOKABLE void save();
    // Session copies have no persistent store. A temporary trial cannot write
    // settings until explicitly kept against its captured host UUID.
    std::unique_ptr<StreamingPreferences> cloneForSession(bool safeSettings = false) const;
    std::unique_ptr<StreamingPreferences> cloneWithLastSuccessfulSettings() const;
    bool keepSessionSettings(const StreamingPreferences& trial);
    void rememberSuccessfulSettings();
    // When this host last streamed healthily, in seconds since the epoch; 0 if never.
    Q_PROPERTY(qint64 lastConnectedAt READ lastConnectedAt NOTIFY hostOptionsChanged)
    qint64 lastConnectedAt() const;
    Q_INVOKABLE bool restoreLastSuccessfulSettings();
    Q_INVOKABLE QVariantList hostPresets() const;
    Q_INVOKABLE bool saveHostPreset(const QString& name);
    Q_INVOKABLE bool applyHostPreset(const QString& id);
    Q_INVOKABLE bool renameHostPreset(const QString& id, const QString& name);
    Q_INVOKABLE bool duplicateHostPreset(const QString& id, const QString& name);
    Q_INVOKABLE bool removeHostPreset(const QString& id);
    Q_PROPERTY(QString activePresetName READ activePresetName NOTIFY hostOptionsChanged)
    QString activePresetName() const;
    Q_INVOKABLE bool resetLearnedPerformance();
    Q_INVOKABLE int effectiveBitrate(int bitrateKbps) const;
    // What the next stream to the selected host will be, why Automatic holds back if it
    // does, and what each Manual choice is expected to cost. Keys follow StreamPlan
    // (app/streaming/streamplan.h): width, height, fps, codec, startBitrateKbps,
    // ceilingKbps, floorKbps, limitedByHost, limitReason, resolutionWarning, fpsWarning,
    // codecWarning, bitrateWarning.
    // The plan for the current host. Zero or empty choices mean Automatic.
    Q_INVOKABLE QVariantMap streamPlan(const QString& hostName, int nativeWidth, int nativeHeight,
                                       int displayHz, int serverCodecModeSupport, int width = 0, int height = 0,
                                       int fps = 0, int bitrateKbps = 0, const QString& codec = QString(), double displayScale = 0, bool integerDesktopScale = false, const QString& performanceContext = QString(), const QString& screenName = QString()) const;

    void reload();

    // Per-host settings (docs/design/2026-07-14-per-host-settings.md).
    //
    // Overlays hostsettings/<uuid>/* for the PER-HOST connection-setting subset
    // onto the in-memory fields (falling back to the flat-key template for any
    // key that host hasn't stored yet), then fires the connection NOTIFY
    // signals so every QML binding re-evaluates. An empty uuid means "global
    // template only" (no host selected). Idempotent: calling again with the
    // uuid already in effect is a no-op.
    Q_INVOKABLE void setCurrentHost(const QString& uuid);
    // The host chosen most recently, reselected on the next launch.
    Q_INVOKABLE QString lastSelectedHost() const;
    QString displayTargetForHost(const QString& uuid) const;
    // The bitrate the last stream to this host settled on, to start the next one near it.
    static int rememberedAutomaticBitrate(const QString& uuid, const QString& context = QString());
    static void rememberAutomaticBitrate(const QString& uuid, int kbps, const QString& context = QString());
    Q_INVOKABLE void setLastSelectedHost(const QString& uuid);

    // Fork-local addition (design-native-first.md V.1/V.2): lets QML (PcView's
    // native-resolution auto-default) check, independent of setCurrentHost()'s
    // in-memory overlay, whether this host has ever had a resolution
    // explicitly stored -- either by the user or by a prior auto-default run.
    // Returns true (nothing to auto-default) for an empty uuid. Checked
    // against SER_WIDTH only: width/height are always written together by
    // save() and the per-host migration, so one key is a sufficient proxy for
    // "this host has a stored resolution."
    Q_INVOKABLE bool hasStoredResolution(const QString& uuid) const;

    // CLI precedence support (docs/design/2026-07-14-per-host-settings.md §4).
    // There is no target host yet when CLI flags are parsed (they're parsed
    // before the target computer is resolved), so the CLI parser records here,
    // by Q_PROPERTY name, exactly the PER-HOST fields an explicit flag set.
    // Once the target host is known and setCurrentHost() has loaded its stored
    // connection base, reapplyCliOverrides() re-stamps just those recorded
    // fields on top, restoring precedence: CLI flag > selected-host stored
    // value > template default. Keyed by the same public property names QML
    // already binds to (via QObject::setProperty(), which honors MEMBER
    // properties' NOTIFY signals), so this needs no knowledge of the private
    // SER_ key table in the .cpp file.
    void rememberCliOverride(const QByteArray& propertyName, const QVariant& value);
    Q_INVOKABLE void reapplyCliOverrides();

    enum AudioConfig
    {
        AC_STEREO,
        AC_51_SURROUND,
        AC_71_SURROUND
    };
    Q_ENUM(AudioConfig)

    enum VideoCodecConfig
    {
        VCC_AUTO,
        VCC_FORCE_H264,
        VCC_FORCE_HEVC,
        VCC_FORCE_HEVC_HDR_DEPRECATED, // Kept for backwards compatibility
        VCC_FORCE_AV1
    };
    Q_ENUM(VideoCodecConfig)

    enum VideoDecoderSelection
    {
        VDS_AUTO,
        VDS_FORCE_HARDWARE,
        VDS_FORCE_SOFTWARE
    };
    Q_ENUM(VideoDecoderSelection)

    enum WindowMode
    {
        WM_FULLSCREEN,
        WM_FULLSCREEN_DESKTOP,
        WM_WINDOWED
    };
    Q_ENUM(WindowMode)

    enum UIDisplayMode
    {
        UI_WINDOWED,
        UI_MAXIMIZED,
        UI_FULLSCREEN
    };
    Q_ENUM(UIDisplayMode)

    // New entries must go at the end of the enum
    // to avoid renumbering existing entries (which
    // would affect existing user preferences).
    enum Language
    {
        LANG_AUTO,
        LANG_EN,
        LANG_FR,
        LANG_ZH_CN,
        LANG_DE,
        LANG_NB_NO,
        LANG_RU,
        LANG_ES,
        LANG_JA,
        LANG_VI,
        LANG_TH,
        LANG_KO,
        LANG_HU,
        LANG_NL,
        LANG_SV,
        LANG_TR,
        LANG_UK,
        LANG_ZH_TW,
        LANG_PT,
        LANG_PT_BR,
        LANG_EL,
        LANG_IT,
        LANG_HI,
        LANG_PL,
        LANG_CS,
        LANG_HE,
        LANG_CKB,
        LANG_LT,
        LANG_ET,
        LANG_BG,
        LANG_EO,
        LANG_TA,
    };
    Q_ENUM(Language);

    enum CaptureSysKeysMode
    {
        CSK_OFF,
        CSK_FULLSCREEN,
        CSK_ALWAYS,
    };
    Q_ENUM(CaptureSysKeysMode);

    Q_PROPERTY(int width MEMBER width NOTIFY displayModeChanged)
    Q_PROPERTY(int height MEMBER height NOTIFY displayModeChanged)
    Q_PROPERTY(int fps MEMBER fps NOTIFY displayModeChanged)
    Q_PROPERTY(int bitrateKbps MEMBER bitrateKbps NOTIFY bitrateChanged)
    Q_PROPERTY(bool unlockBitrate MEMBER unlockBitrate NOTIFY unlockBitrateChanged)
    Q_PROPERTY(bool autoAdjustBitrate MEMBER autoAdjustBitrate NOTIFY autoAdjustBitrateChanged)
    Q_PROPERTY(bool enableVsync MEMBER enableVsync NOTIFY enableVsyncChanged)
    Q_PROPERTY(bool gameOptimizations MEMBER gameOptimizations NOTIFY gameOptimizationsChanged)
    Q_PROPERTY(bool playAudioOnHost MEMBER playAudioOnHost NOTIFY playAudioOnHostChanged)
    Q_PROPERTY(bool multiController MEMBER multiController NOTIFY multiControllerChanged)
    Q_PROPERTY(bool enableMdns MEMBER enableMdns NOTIFY enableMdnsChanged)
    Q_PROPERTY(bool quitAppAfter MEMBER quitAppAfter NOTIFY quitAppAfterChanged)
    Q_PROPERTY(bool absoluteMouseMode MEMBER absoluteMouseMode NOTIFY absoluteMouseModeChanged)
    Q_PROPERTY(bool absoluteTouchMode MEMBER absoluteTouchMode NOTIFY absoluteTouchModeChanged)
    Q_PROPERTY(bool localCursor MEMBER localCursor NOTIFY localCursorChanged)
    Q_PROPERTY(bool framePacing MEMBER framePacing NOTIFY framePacingChanged)
    Q_PROPERTY(bool connectionWarnings MEMBER connectionWarnings NOTIFY connectionWarningsChanged)
    Q_PROPERTY(bool configurationWarnings MEMBER configurationWarnings NOTIFY configurationWarningsChanged)
    Q_PROPERTY(bool richPresence MEMBER richPresence NOTIFY richPresenceChanged)
    Q_PROPERTY(bool gamepadMouse MEMBER gamepadMouse NOTIFY gamepadMouseChanged)
    Q_PROPERTY(bool detectNetworkBlocking MEMBER detectNetworkBlocking NOTIFY detectNetworkBlockingChanged)
    Q_PROPERTY(bool showPerformanceOverlay MEMBER showPerformanceOverlay NOTIFY showPerformanceOverlayChanged)
    Q_PROPERTY(AudioConfig audioConfig MEMBER audioConfig NOTIFY audioConfigChanged)
    Q_PROPERTY(VideoCodecConfig videoCodecConfig MEMBER videoCodecConfig NOTIFY videoCodecConfigChanged)
    Q_PROPERTY(bool enableHdr MEMBER enableHdr NOTIFY enableHdrChanged)
    Q_PROPERTY(bool enableYUV444 MEMBER enableYUV444 NOTIFY enableYUV444Changed)
    Q_PROPERTY(VideoDecoderSelection videoDecoderSelection MEMBER videoDecoderSelection NOTIFY videoDecoderSelectionChanged)
    // No UI binds this (design §1: "not in owner list, no UI, default 0 (auto)");
    // exposed as a real Q_PROPERTY solely so the generic CLI-override/
    // setCurrentHost machinery above can address it uniformly with every other
    // PER-HOST field via the Qt meta-property system.
    Q_PROPERTY(int packetSize MEMBER packetSize NOTIFY packetSizeChanged)
    Q_PROPERTY(QString connectionAddress MEMBER connectionAddress NOTIFY hostOptionsChanged)
    Q_PROPERTY(QString desktopScale MEMBER desktopScale NOTIFY hostOptionsChanged)
    Q_PROPERTY(QString viewScale MEMBER viewScale NOTIFY hostOptionsChanged)
    Q_PROPERTY(bool automaticResolution MEMBER automaticResolution NOTIFY displayModeChanged)
    Q_PROPERTY(bool automaticFrameRate MEMBER automaticFrameRate NOTIFY displayModeChanged)
    Q_PROPERTY(bool automaticPointer MEMBER automaticPointer NOTIFY absoluteMouseModeChanged)
    Q_PROPERTY(QString displayTarget MEMBER displayTarget NOTIFY hostOptionsChanged)
    Q_PROPERTY(bool adaptiveBitrate MEMBER adaptiveBitrate NOTIFY hostOptionsChanged)
    // Whether this host's resolution, frame rate, codec and bandwidth are the
    // person's own choices (Manual) or chosen for them (Automatic, the default).
    Q_PROPERTY(bool manualStream MEMBER manualStream NOTIFY hostOptionsChanged)
    // Likewise remote text size, view scale and host display (Display), and the
    // pointer and local cursor (Input). Automatic streams with the automatic
    // values whatever is saved; the saved ones come back with Manual.
    Q_PROPERTY(bool manualDisplay MEMBER manualDisplay NOTIFY hostOptionsChanged)
    Q_PROPERTY(bool manualInput MEMBER manualInput NOTIFY hostOptionsChanged)
    Q_PROPERTY(bool localTextInput MEMBER localTextInput NOTIFY hostOptionsChanged)
    Q_PROPERTY(WindowMode windowMode MEMBER windowMode NOTIFY windowModeChanged)
    Q_PROPERTY(WindowMode recommendedFullScreenMode MEMBER recommendedFullScreenMode CONSTANT)
    Q_PROPERTY(UIDisplayMode uiDisplayMode MEMBER uiDisplayMode NOTIFY uiDisplayModeChanged)
    Q_PROPERTY(bool swapMouseButtons MEMBER swapMouseButtons NOTIFY mouseButtonsChanged)
    Q_PROPERTY(bool muteOnFocusLoss MEMBER muteOnFocusLoss NOTIFY muteOnFocusLossChanged)
    Q_PROPERTY(bool backgroundGamepad MEMBER backgroundGamepad NOTIFY backgroundGamepadChanged)
    Q_PROPERTY(bool reverseScrollDirection MEMBER reverseScrollDirection NOTIFY reverseScrollDirectionChanged)
    Q_PROPERTY(bool swapFaceButtons MEMBER swapFaceButtons NOTIFY swapFaceButtonsChanged)
    Q_PROPERTY(bool keepAwake MEMBER keepAwake NOTIFY keepAwakeChanged)
    Q_PROPERTY(CaptureSysKeysMode lastEnabledCaptureSysKeysMode MEMBER lastEnabledCaptureSysKeysMode NOTIFY captureSysKeysModeChanged)
    Q_PROPERTY(CaptureSysKeysMode captureSysKeysMode MEMBER captureSysKeysMode NOTIFY captureSysKeysModeChanged)
    Q_PROPERTY(Language language MEMBER language NOTIFY languageChanged);

    Q_INVOKABLE QVariantMap changeLanguage(int language);
    Q_INVOKABLE bool retranslate();

    // Directly accessible members for preferences
    int width;
    int height;
    int fps;
    int bitrateKbps;
    bool unlockBitrate;
    bool autoAdjustBitrate;
    bool enableVsync;
    bool gameOptimizations;
    bool playAudioOnHost;
    bool multiController;
    bool enableMdns;
    bool quitAppAfter;
    bool absoluteMouseMode;
    bool absoluteTouchMode;
    // Keep the local (client-side) cursor visible while in absolute-mouse
    // mode, instead of hiding it in favor of the host-rendered cursor.
    // Pairs with a host-side capture_cursor=disabled setting for a
    // zero-latency local pointer. See also MOONLIGHT_LOCAL_CURSOR in
    // SdlInputHandler, which can force this on regardless of this pref.
    bool localCursor;
    bool framePacing;
    bool connectionWarnings;
    bool configurationWarnings;
    bool richPresence;
    bool gamepadMouse;
    bool detectNetworkBlocking;
    bool showPerformanceOverlay;
    bool swapMouseButtons;
    bool muteOnFocusLoss;
    bool backgroundGamepad;
    bool reverseScrollDirection;
    bool swapFaceButtons;
    bool keepAwake;
    int packetSize;
    QString connectionAddress;
    QString desktopScale = QStringLiteral("auto");
    QString viewScale = QStringLiteral("fit");
    bool automaticResolution = false;
    bool automaticFrameRate = false;
    bool automaticPointer = false;
    QString displayTarget = QStringLiteral("host");
    bool adaptiveBitrate = true;
    bool manualStream = false;
    bool manualDisplay = false;
    bool manualInput = false;
    bool localTextInput = false;
    AudioConfig audioConfig;
    VideoCodecConfig videoCodecConfig;
    bool enableHdr;
    bool enableYUV444;
    VideoDecoderSelection videoDecoderSelection;
    WindowMode windowMode;
    WindowMode recommendedFullScreenMode;
    UIDisplayMode uiDisplayMode;
    Language language;
    CaptureSysKeysMode captureSysKeysMode;
    CaptureSysKeysMode lastEnabledCaptureSysKeysMode;

signals:
    void displayModeChanged();
    void bitrateChanged();
    void unlockBitrateChanged();
    void autoAdjustBitrateChanged();
    void enableVsyncChanged();
    void gameOptimizationsChanged();
    void playAudioOnHostChanged();
    void multiControllerChanged();
    void unsupportedFpsChanged();
    void enableMdnsChanged();
    void quitAppAfterChanged();
    void absoluteMouseModeChanged();
    void absoluteTouchModeChanged();
    void localCursorChanged();
    void audioConfigChanged();
    void videoCodecConfigChanged();
    void enableHdrChanged();
    void enableYUV444Changed();
    void videoDecoderSelectionChanged();
    void packetSizeChanged();
    void uiDisplayModeChanged();
    void windowModeChanged();
    void framePacingChanged();
    void connectionWarningsChanged();
    void configurationWarningsChanged();
    void richPresenceChanged();
    void gamepadMouseChanged();
    void detectNetworkBlockingChanged();
    void showPerformanceOverlayChanged();
    void mouseButtonsChanged();
    void muteOnFocusLossChanged();
    void backgroundGamepadChanged();
    void reverseScrollDirectionChanged();
    void swapFaceButtonsChanged();
    void captureSysKeysModeChanged();
    void keepAwakeChanged();
    void languageChanged();
    void saveCompleted();
    void hostOptionsChanged();

private:
    explicit StreamingPreferences(QQmlEngine *qmlEngine, std::unique_ptr<QSettings> settings = {});
    StreamingPreferences(const StreamingPreferences& source, bool safeSettings);
    QVariantMap connectionSnapshot() const;
    bool applyConnectionSnapshot(const QVariantMap& values);
    QString hostPrefix() const;
    // Re-derives Automatic or Manual for each named mode key ("manualstream",
    // "manualdisplay", "manualinput") from the values themselves.
    void sanitizeLiveStreamValues(const QStringList& derivedModes = {});

    QQmlEngine* m_QmlEngine;
    std::unique_ptr<QSettings> m_Settings;
    std::unique_ptr<LanguageWorkflow> m_LanguageWorkflow;

    // Per-host settings support. m_TemplateValues is a frozen snapshot of the
    // PER-HOST key subset taken at the end of reload(), used by setCurrentHost()
    // as the fallback for any key a host hasn't stored yet. It is deliberately
    // NOT re-derived from the live fields on every call: the live fields get
    // overwritten by whichever host is currently overlaid, so re-reading them
    // would leak one host's value into another host's fallback after two or
    // more setCurrentHost() calls. See docs/design/2026-07-14-per-host-settings.md §3.
    QVariantHash m_TemplateValues;
    QString m_CurrentHostUuid;
    bool m_CurrentHostUuidValid = false;

    // See rememberCliOverride()/reapplyCliOverrides() above.
    QHash<QByteArray, QVariant> m_CliOverrides;
};
