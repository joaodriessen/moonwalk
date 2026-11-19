#include "streamingpreferences.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <algorithm>
#include "../application/languageworkflow.h"
#include "../application/preferencesworkflow.h"
#include "utils.h"
#include "../streaming/streamplan.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QReadWriteLock>
#include <QtDebug>
#include <QMetaProperty>
#include <QUuid>
#include <QUrl>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_UNLOCK_BITRATE "unlockbitrate"
#define SER_AUTOADJUSTBITRATE "autoadjustbitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_VIDEOCFG "videocfg"
#define SER_HDR "hdr"
#define SER_YUV444 "yuv444"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_MDNS "mdns"
#define SER_QUITAPPAFTER "quitAppAfter"
#define SER_ABSMOUSEMODE "mouseacceleration"
#define SER_ABSTOUCHMODE "abstouchmode"
#define SER_LOCALCURSOR "localcursor"
#define SER_STARTWINDOWED "startwindowed"
#define SER_FRAMEPACING "framepacing"
#define SER_CONNWARNINGS "connwarnings"
#define SER_CONFWARNINGS "confwarnings"
#define SER_UIDISPLAYMODE "uidisplaymode"
#define SER_RICHPRESENCE "richpresence"
#define SER_GAMEPADMOUSE "gamepadmouse"
#define SER_DEFAULTVER "defaultver"
#define SER_PACKETSIZE "packetsize"
#define SER_DETECTNETBLOCKING "detectnetblocking"
#define SER_SHOWPERFOVERLAY "showperfoverlay"
#define SER_SWAPMOUSEBUTTONS "swapmousebuttons"
#define SER_MUTEONFOCUSLOSS "muteonfocusloss"
#define SER_BACKGROUNDGAMEPAD "backgroundgamepad"
#define SER_REVERSESCROLL "reversescroll"
#define SER_SWAPFACEBUTTONS "swapfacebuttons"
#define SER_CAPTURESYSKEYS "capturesyskeys"
#define SER_LASTENABLEDCAPTURESYSKEYS "lastenabledcapturesyskeys"
#define SER_KEEPAWAKE "keepawake"
#define SER_LANGUAGE "language"
#define SER_STARLIGHT_PREFS_VERSION "starlightPrefsVersion"

// Per-host settings (docs/design/2026-07-14-per-host-settings.md §2). A
// QSettings group keyed by NvComputer::uuid, distinct from the "hosts" /
// "hostsbackup" array groups ComputerManager owns and from the flat global
// keys above. Locally-defined (not shared with computermanager.cpp/
// nvcomputer.cpp) to avoid coupling this file to those headers, matching this
// codebase's existing convention of each file owning its own SER_ string
// constants for the keys it touches.
#define SER_HOSTSETTINGS_GROUP "hostsettings"
#define SER_HOSTS "hosts"
#define SER_HOSTS_BACKUP "hostsbackup"
#define SER_HOST_UUID "uuid"

#define CURRENT_DEFAULT_VER 2

// The pointer is drawn here by default, so it never lags; hosts that can hide
// their own (Moonwalk Host, via clientSideCursor) do. Only affects fresh
// installs -- a stored value always wins.
static const bool DEFAULT_LOCAL_CURSOR = true;

static StreamingPreferences* s_GlobalPrefs;

Q_GLOBAL_STATIC(QReadWriteLock, s_GlobalPrefsLock)

namespace {

// One PER-HOST connection-setting key, with typed get/set against a
// StreamingPreferences instance's live fields via QVariant. This table is the
// single source of truth for the PER-HOST subset (design §1) -- setCurrentHost(),
// save(), and reload()'s migration all iterate it, so the three can never
// disagree about which keys are per-host.
struct PerHostKey
{
    const char* serKey;
    QVariant (*get)(const StreamingPreferences*);
    void (*set)(StreamingPreferences*, const QVariant&);
};

const PerHostKey k_PerHostKeys[] = {
    { "displaytarget", [](const StreamingPreferences* p) { return QVariant(p->displayTarget); },
      [](StreamingPreferences* p, const QVariant& v) { p->displayTarget = v.toString(); } },
    { "automaticresolution", [](const StreamingPreferences* p) { return QVariant(p->automaticResolution); },
      [](StreamingPreferences* p, const QVariant& v) { p->automaticResolution = v.toBool(); } },
    { "automaticframerate", [](const StreamingPreferences* p) { return QVariant(p->automaticFrameRate); },
      [](StreamingPreferences* p, const QVariant& v) { p->automaticFrameRate = v.toBool(); } },
    { "automaticpointer", [](const StreamingPreferences* p) { return QVariant(p->automaticPointer); },
      [](StreamingPreferences* p, const QVariant& v) { p->automaticPointer = v.toBool(); } },
    { SER_WIDTH,
      [](const StreamingPreferences* p) { return QVariant(p->width); },
      [](StreamingPreferences* p, const QVariant& v) { p->width = v.toInt(); } },
    { SER_HEIGHT,
      [](const StreamingPreferences* p) { return QVariant(p->height); },
      [](StreamingPreferences* p, const QVariant& v) { p->height = v.toInt(); } },
    { SER_FPS,
      [](const StreamingPreferences* p) { return QVariant(p->fps); },
      [](StreamingPreferences* p, const QVariant& v) { p->fps = v.toInt(); } },
    { SER_BITRATE,
      [](const StreamingPreferences* p) { return QVariant(p->bitrateKbps); },
      [](StreamingPreferences* p, const QVariant& v) { p->bitrateKbps = v.toInt(); } },
    { SER_UNLOCK_BITRATE,
      [](const StreamingPreferences* p) { return QVariant(p->unlockBitrate); },
      [](StreamingPreferences* p, const QVariant& v) { p->unlockBitrate = v.toBool(); } },
    { SER_AUTOADJUSTBITRATE,
      [](const StreamingPreferences* p) { return QVariant(p->autoAdjustBitrate); },
      [](StreamingPreferences* p, const QVariant& v) { p->autoAdjustBitrate = v.toBool(); } },
    { SER_VSYNC,
      [](const StreamingPreferences* p) { return QVariant(p->enableVsync); },
      [](StreamingPreferences* p, const QVariant& v) { p->enableVsync = v.toBool(); } },
    { SER_FRAMEPACING,
      [](const StreamingPreferences* p) { return QVariant(p->framePacing); },
      [](StreamingPreferences* p, const QVariant& v) { p->framePacing = v.toBool(); } },
    { SER_LOCALCURSOR,
      [](const StreamingPreferences* p) { return QVariant(p->localCursor); },
      [](StreamingPreferences* p, const QVariant& v) { p->localCursor = v.toBool(); } },
    { SER_ABSMOUSEMODE,
      [](const StreamingPreferences* p) { return QVariant(p->absoluteMouseMode); },
      [](StreamingPreferences* p, const QVariant& v) { p->absoluteMouseMode = v.toBool(); } },
    { SER_AUDIOCFG,
      [](const StreamingPreferences* p) { return QVariant(static_cast<int>(p->audioConfig)); },
      [](StreamingPreferences* p, const QVariant& v) { p->audioConfig = static_cast<StreamingPreferences::AudioConfig>(v.toInt()); } },
    { SER_VIDEOCFG,
      [](const StreamingPreferences* p) { return QVariant(static_cast<int>(p->videoCodecConfig)); },
      [](StreamingPreferences* p, const QVariant& v) { p->videoCodecConfig = static_cast<StreamingPreferences::VideoCodecConfig>(v.toInt()); } },
    { SER_HDR,
      [](const StreamingPreferences* p) { return QVariant(p->enableHdr); },
      [](StreamingPreferences* p, const QVariant& v) { p->enableHdr = v.toBool(); } },
    { SER_YUV444,
      [](const StreamingPreferences* p) { return QVariant(p->enableYUV444); },
      [](StreamingPreferences* p, const QVariant& v) { p->enableYUV444 = v.toBool(); } },
    { SER_VIDEODEC,
      [](const StreamingPreferences* p) { return QVariant(static_cast<int>(p->videoDecoderSelection)); },
      [](StreamingPreferences* p, const QVariant& v) { p->videoDecoderSelection = static_cast<StreamingPreferences::VideoDecoderSelection>(v.toInt()); } },
    { SER_PACKETSIZE,
      [](const StreamingPreferences* p) { return QVariant(p->packetSize); },
      [](StreamingPreferences* p, const QVariant& v) { p->packetSize = v.toInt(); } },
    { "connectionaddress", [](const StreamingPreferences* p) { return QVariant(p->connectionAddress); },
      [](StreamingPreferences* p, const QVariant& v) { p->connectionAddress = v.toString(); } },
    { "viewscale", [](const StreamingPreferences* p) { return QVariant(p->viewScale); },
      [](StreamingPreferences* p, const QVariant& v) { p->viewScale = v.toString(); } },
    { "desktopscale", [](const StreamingPreferences* p) { return QVariant(p->desktopScale); },
      [](StreamingPreferences* p, const QVariant& v) { p->desktopScale = v.toString(); } },
    { "adaptivebitrate", [](const StreamingPreferences* p) { return QVariant(p->adaptiveBitrate); },
      [](StreamingPreferences* p, const QVariant& v) { p->adaptiveBitrate = v.toBool(); } },
    { "localtextinput", [](const StreamingPreferences* p) { return QVariant(p->localTextInput); },
      [](StreamingPreferences* p, const QVariant& v) { p->localTextInput = v.toBool(); } },
    { "manualstream", [](const StreamingPreferences* p) { return QVariant(p->manualStream); },
      [](StreamingPreferences* p, const QVariant& v) { p->manualStream = v.toBool(); } },
    { "manualdisplay", [](const StreamingPreferences* p) { return QVariant(p->manualDisplay); },
      [](StreamingPreferences* p, const QVariant& v) { p->manualDisplay = v.toBool(); } },
    { "manualinput", [](const StreamingPreferences* p) { return QVariant(p->manualInput); },
      [](StreamingPreferences* p, const QVariant& v) { p->manualInput = v.toBool(); } },
};

const char* const k_ModeKeys[] = { "manualstream", "manualdisplay", "manualinput" };

// A record from before Automatic and Manual existed: any resolution, frame rate,
// bitrate or codec of the person's own makes it Manual, with those values kept.
// Older versions saved the display's own size and 60 fps as if they had been
// chosen, so only a value Automatic wouldn't pick counts as a Manual choice.
bool derivedManualStream(const QVariantMap& values)
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QSize native = screen ? (screen->size() * screen->devicePixelRatio()) : QSize();
    const int refresh = screen ? qRound(screen->refreshRate()) : 60;
    const QSize size(values.value(QStringLiteral(SER_WIDTH), 0).toInt(), values.value(QStringLiteral(SER_HEIGHT), 0).toInt());
    const int fps = values.value(QStringLiteral(SER_FPS), 0).toInt();
    const bool fixedSize = !values.value(QStringLiteral("automaticresolution"), false).toBool()
        && !(size.isValid() && (size == native || size == native.transposed()));
    const bool fixedRate = !values.value(QStringLiteral("automaticframerate"), false).toBool()
        && fps != 60 && qAbs(fps - refresh) > 1;
    return fixedSize || fixedRate
        || !values.value(QStringLiteral(SER_AUTOADJUSTBITRATE), true).toBool()
        || values.value(QStringLiteral(SER_VIDEOCFG), 0).toInt() != StreamingPreferences::VCC_AUTO;
}

// A record from before Display had Automatic and Manual: a text size or view
// scale of the person's own, or a host display other than Automatic, makes it
// Manual. "Use host setting" was every host's default before Automatic existed,
// and is what Automatic does on a host that can't make a display of its own.
bool derivedManualDisplay(const QVariantMap& values)
{
    const QString target = values.value(QStringLiteral("displaytarget"), QStringLiteral("auto")).toString();
    return values.value(QStringLiteral("desktopscale"), QStringLiteral("auto")).toString() != QLatin1String("auto")
        || values.value(QStringLiteral("viewscale"), QStringLiteral("fit")).toString() == QLatin1String("actual")
        || target == QLatin1String("virtual") || target == QLatin1String("physical");
}

// Likewise Input: a game pointer someone chose, or the local cursor off. A desktop
// pointer saved before Automatic existed is what Automatic gives a desktop anyway.
bool derivedManualInput(const QVariantMap& values)
{
    const bool chosenGamePointer = !values.value(QStringLiteral("automaticpointer"), false).toBool()
        && !values.value(QStringLiteral(SER_ABSMOUSEMODE), false).toBool();
    return chosenGamePointer || !values.value(QStringLiteral(SER_LOCALCURSOR), DEFAULT_LOCAL_CURSOR).toBool();
}

// Choices that can only make a stream worse on a supported setup (V-sync off,
// a forced decoder, the bitrate unlock) and the ones now made automatically
// (frame pacing, HDR, 4:4:4 colour, audio channels) load as their automatic
// values, wherever they come from: an old profile, a saved preset or the last
// working settings. Command-line flags still override them, for diagnostics.
void sanitizeStreamValues(QVariantMap& values)
{
    if (!values.contains(QStringLiteral("manualstream")))
        values.insert(QStringLiteral("manualstream"), derivedManualStream(values));
    if (!values.contains(QStringLiteral("manualdisplay")))
        values.insert(QStringLiteral("manualdisplay"), derivedManualDisplay(values));
    if (!values.contains(QStringLiteral("manualinput")))
        values.insert(QStringLiteral("manualinput"), derivedManualInput(values));
    if (values.value(QStringLiteral(SER_VIDEOCFG)).toInt() == StreamingPreferences::VCC_FORCE_HEVC_HDR_DEPRECATED)
        values.insert(QStringLiteral(SER_VIDEOCFG), int(StreamingPreferences::VCC_AUTO));
    values.insert(QStringLiteral(SER_VSYNC), true);
    values.insert(QStringLiteral(SER_VIDEODEC), int(StreamingPreferences::VDS_AUTO));
    values.insert(QStringLiteral(SER_UNLOCK_BITRATE), false);
    values.insert(QStringLiteral(SER_FRAMEPACING), false);
    values.insert(QStringLiteral(SER_YUV444), false);
    values.insert(QStringLiteral(SER_HDR), false);
    values.insert(QStringLiteral(SER_AUDIOCFG), int(StreamingPreferences::AC_STEREO));
    // Without the unlock, its range goes too. Out-of-range records stay malformed.
    const int bitrate = values.value(QStringLiteral(SER_BITRATE), 0).toInt();
    if (bitrate > 150000 && bitrate <= 500000) values.insert(QStringLiteral(SER_BITRATE), 150000);
}

// A stored record with the defaults of fields added since it was written.
QVariantMap normalizedConnectionValues(const QVariantMap& stored)
{
    QVariantMap values = stored;
    if (!values.contains("viewscale")) values.insert("viewscale", QStringLiteral("fit"));
    if (!values.contains("automaticresolution")) values.insert("automaticresolution", false);
    if (!values.contains("automaticframerate")) values.insert("automaticframerate", false);
    if (!values.contains("automaticpointer")) values.insert("automaticpointer", false);
    if (!values.contains("displaytarget")) values.insert("displaytarget", QStringLiteral("host"));
    sanitizeStreamValues(values);
    return values;
}

class StreamingLanguageState final : public LanguageWorkflow::StatePort
{
public:
    explicit StreamingLanguageState(StreamingPreferences& preferences)
        : m_Preferences(preferences)
    {
    }

    int currentLanguage() const override
    {
        return static_cast<int>(m_Preferences.language);
    }

    void commitLanguage(int language) override
    {
        m_Preferences.language =
            static_cast<StreamingPreferences::Language>(language);
        emit m_Preferences.languageChanged();
    }

private:
    StreamingPreferences& m_Preferences;
};

} // anonymous namespace

StreamingPreferences::StreamingPreferences(QQmlEngine *qmlEngine, std::unique_ptr<QSettings> settings)
    : m_QmlEngine(qmlEngine),
      m_Settings(std::move(settings)),
      m_LanguageWorkflow(std::make_unique<LanguageWorkflow>())
{
    if (!m_Settings) {
        m_Settings = std::make_unique<QSettings>();
    }

    reload();
}

StreamingPreferences::~StreamingPreferences() = default;

StreamingPreferences::StreamingPreferences(const StreamingPreferences& source, bool safeSettings)
    : m_QmlEngine(nullptr), m_LanguageWorkflow(std::make_unique<LanguageWorkflow>()),
      m_TemplateValues(source.m_TemplateValues), m_CurrentHostUuid(source.m_CurrentHostUuid),
      m_CurrentHostUuidValid(source.m_CurrentHostUuidValid)
{
    // MEMBER properties include the application-wide input/window choices too.
    // This constructor deliberately never constructs QSettings or runs migrations.
    const auto* meta = source.metaObject();
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty property = meta->property(i);
        if (property.isWritable()) property.write(this, property.read(&source));
    }
    recommendedFullScreenMode = source.recommendedFullScreenMode;
    if (safeSettings) {
        width = 1920; height = 1080; fps = 60; bitrateKbps = 10000;
        autoAdjustBitrate = false; unlockBitrate = false;
        videoCodecConfig = VCC_AUTO; videoDecoderSelection = VDS_AUTO;
        enableHdr = false; enableYUV444 = false; audioConfig = AC_STEREO;
        adaptiveBitrate = false; desktopScale = QStringLiteral("auto"); viewScale = QStringLiteral("fit");
        automaticResolution = false;
        automaticFrameRate = false;
        manualStream = true;
    }
    // Automatic leaves the stream to the plan; Manual values stay saved for when
    // they are chosen again.
    else if (!manualStream) {
        automaticResolution = true;
        automaticFrameRate = true;
        autoAdjustBitrate = true;
        adaptiveBitrate = true;
        videoCodecConfig = VCC_AUTO;
    }
    // The same for Display and Input.
    if (!manualDisplay) {
        desktopScale = QStringLiteral("auto");
        viewScale = QStringLiteral("fit");
        displayTarget = QStringLiteral("auto");
        // SOPS is also Sunshine-family hosts' ordinary display-mode request.
        // An obsolete, hidden game-optimization preference must not veto it.
        // GFE's supported-resolution safety check still runs before launch.
        gameOptimizations = true;
    }
    if (!manualInput) {
        automaticPointer = true;
        absoluteMouseMode = true;
        localCursor = true;
    }
    // Explicit command-line choices apply to this connection after Automatic
    // policy, just as they apply after the saved host overlay. Safe retries
    // deliberately keep their bounded settings instead of replaying bad ones.
    if (!safeSettings) {
        m_CliOverrides = source.m_CliOverrides;
        reapplyCliOverrides();
    }
}

std::unique_ptr<StreamingPreferences> StreamingPreferences::cloneForSession(bool safeSettings) const
{
    return std::unique_ptr<StreamingPreferences>(new StreamingPreferences(*this, safeSettings));
}

std::unique_ptr<StreamingPreferences> StreamingPreferences::cloneWithLastSuccessfulSettings() const
{
    const auto* persistent = m_Settings ? this : get();
    if (!persistent || !persistent->m_Settings || hostPrefix().isEmpty()) return nullptr;
    auto result = cloneForSession();
    if (!result->applyConnectionSnapshot(
                persistent->m_Settings->value(hostPrefix() + "lastsuccessful").toMap())) return nullptr;
    return result;
}

QString StreamingPreferences::hostPrefix() const
{
    return m_CurrentHostUuidValid && !m_CurrentHostUuid.isEmpty()
            ? QStringLiteral("hostsettings/") + m_CurrentHostUuid + '/' : QString();
}

QVariantMap StreamingPreferences::connectionSnapshot() const
{
    QVariantMap values;
    for (const auto& key : k_PerHostKeys) values.insert(QString::fromLatin1(key.serKey), key.get(this));
    return values;
}

bool StreamingPreferences::applyConnectionSnapshot(const QVariantMap& stored)
{
    // Bounds below still reject a malformed record; sanitising only replaces
    // values the interface no longer offers.
    for (const char* key : {SER_AUDIOCFG, SER_VIDEODEC})
        if (stored.contains(key) && (stored.value(key).toInt() < 0 || stored.value(key).toInt() > 2)) return false;
    const QVariantMap values = normalizedConnectionValues(stored);
    // Validate the complete record before touching any live value. Profiles may
    // be edited outside the app; partial/unknown records must not half-apply.
    if (values.size() != int(std::size(k_PerHostKeys))) return false;
    for (const auto& key : k_PerHostKeys) if (!values.contains(QString::fromLatin1(key.serKey))) return false;
    auto bounded = [&](const char* key, int minimum, int maximum) {
        bool ok = false; const int value = values.value(key).toInt(&ok);
        return ok && value >= minimum && value <= maximum;
    };
    if (!bounded(SER_WIDTH, 256, 8192) || !bounded(SER_HEIGHT, 256, 8192)
            || !bounded(SER_FPS, 10, 9999) || !bounded(SER_BITRATE, 500, 500000)
            || !bounded(SER_AUDIOCFG, 0, 2) || !bounded(SER_VIDEODEC, 0, 2)
            || !bounded(SER_VIDEOCFG, 0, 4) || !bounded(SER_PACKETSIZE, 0, 65535)) return false;
    const QString view = values.value("viewscale").toString();
    if (!QStringList({"host", "auto", "virtual", "physical"}).contains(values.value("displaytarget").toString())) return false;
    if (view != "fit" && view != "actual") return false;
    const QString scale = values.value("desktopscale").toString();
    if (scale != "auto" && scale != "1" && scale != "2") return false;
    for (const auto& key : k_PerHostKeys) key.set(this, values.value(QString::fromLatin1(key.serKey)));
    bitrateKbps = effectiveBitrate(bitrateKbps);
    emit displayModeChanged(); emit bitrateChanged(); emit unlockBitrateChanged();
    emit autoAdjustBitrateChanged(); emit enableVsyncChanged(); emit framePacingChanged();
    emit localCursorChanged(); emit absoluteMouseModeChanged(); emit audioConfigChanged();
    emit videoCodecConfigChanged(); emit enableHdrChanged(); emit enableYUV444Changed();
    emit videoDecoderSelectionChanged(); emit packetSizeChanged(); emit hostOptionsChanged();
    return true;
}

bool StreamingPreferences::keepSessionSettings(const StreamingPreferences& trial)
{
    if (!m_Settings || trial.hostPrefix().isEmpty()) return false;
    auto checked = cloneForSession();
    if (!checked->applyConnectionSnapshot(trial.connectionSnapshot())) return false;
    for (const auto& key : k_PerHostKeys)
        m_Settings->setValue(trial.hostPrefix() + key.serKey, key.get(checked.get()));
    m_Settings->sync();
    if (m_Settings->status() != QSettings::NoError) return false;
    if (m_CurrentHostUuid == trial.m_CurrentHostUuid)
        applyConnectionSnapshot(checked->connectionSnapshot());
    return true;
}

void StreamingPreferences::rememberSuccessfulSettings()
{
    if (!m_Settings) {
        // A session copy records success through the singleton but never saves
        // its transient values as the active preferences.
        auto* persistent = get();
        if (persistent && !hostPrefix().isEmpty()) {
            persistent->m_Settings->setValue(hostPrefix() + "lastsuccessful", connectionSnapshot());
            persistent->m_Settings->setValue(hostPrefix() + "lastconnected", QDateTime::currentSecsSinceEpoch());
            emit persistent->hostOptionsChanged();
        }
    } else if (!hostPrefix().isEmpty()) {
        m_Settings->setValue(hostPrefix() + "lastsuccessful", connectionSnapshot());
        m_Settings->setValue(hostPrefix() + "lastconnected", QDateTime::currentSecsSinceEpoch());
        emit hostOptionsChanged();
    }
}

qint64 StreamingPreferences::lastConnectedAt() const
{
    if (!m_Settings || hostPrefix().isEmpty()) return 0;
    return m_Settings->value(hostPrefix() + "lastconnected", 0).toLongLong();
}

bool StreamingPreferences::restoreLastSuccessfulSettings()
{
    if (!m_Settings || hostPrefix().isEmpty()
            || !applyConnectionSnapshot(m_Settings->value(hostPrefix() + "lastsuccessful").toMap())) return false;
    save(); return true;
}

QVariantList StreamingPreferences::hostPresets() const
{
    QVariantList result;
    if (!m_Settings || hostPrefix().isEmpty()) return result;
    m_Settings->beginGroup(hostPrefix() + "presets");
    for (const QString& id : m_Settings->childGroups()) {
        const QString name = m_Settings->value(id + "/name").toString();
        if (!name.isEmpty()) result.append(QVariantMap{{"id", id}, {"name", name}});
    }
    m_Settings->endGroup();
    return result;
}

QString StreamingPreferences::activePresetName() const
{
    if (!m_Settings || hostPrefix().isEmpty()) return QString();
    const QString id = m_Settings->value(hostPrefix() + "activepreset").toString();
    const QString prefix = hostPrefix() + "presets/" + id;
    const QString name = m_Settings->value(prefix + "/name").toString();
    if (name.isEmpty()) return tr("Custom");
    return normalizedConnectionValues(m_Settings->value(prefix + "/values").toMap()) == connectionSnapshot()
            ? name : tr("%1 (modified)").arg(name);
}

bool StreamingPreferences::saveHostPreset(const QString& name)
{
    const QString normalized = name.trimmed();
    if (!m_Settings || hostPrefix().isEmpty() || normalized.isEmpty() || normalized.size() > 80) return false;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_Settings->setValue(hostPrefix() + "presets/" + id + "/name", normalized);
    m_Settings->setValue(hostPrefix() + "presets/" + id + "/values", connectionSnapshot());
    m_Settings->setValue(hostPrefix() + "activepreset", id);
    emit hostOptionsChanged(); return true;
}

bool StreamingPreferences::applyHostPreset(const QString& id)
{
    if (!m_Settings || hostPrefix().isEmpty() || id.contains('/') || id.contains('\\')) return false;
    if (!applyConnectionSnapshot(m_Settings->value(hostPrefix() + "presets/" + id + "/values").toMap())) return false;
    m_Settings->setValue(hostPrefix() + "activepreset", id);
    save(); return true;
}

bool StreamingPreferences::renameHostPreset(const QString& id, const QString& name)
{
    if (!m_Settings || hostPrefix().isEmpty() || id.contains('/') || id.contains('\\')
            || name.trimmed().isEmpty() || name.trimmed().size() > 80
            || !m_Settings->contains(hostPrefix() + "presets/" + id + "/values")) return false;
    m_Settings->setValue(hostPrefix() + "presets/" + id + "/name", name.trimmed());
    emit hostOptionsChanged(); return true;
}

bool StreamingPreferences::removeHostPreset(const QString& id)
{
    if (!m_Settings || hostPrefix().isEmpty() || id.isEmpty() || id.contains('/') || id.contains('\\')) return false;
    m_Settings->remove(hostPrefix() + "presets/" + id);
    if (m_Settings->value(hostPrefix() + "activepreset").toString() == id)
        m_Settings->remove(hostPrefix() + "activepreset");
    emit hostOptionsChanged(); return true;
}

bool StreamingPreferences::duplicateHostPreset(const QString& id, const QString& name)
{
    if (!m_Settings || hostPrefix().isEmpty() || id.isEmpty() || id.contains('/') || id.contains('\\')
            || name.trimmed().isEmpty() || name.trimmed().size() > 80) return false;
    const QVariantMap values = m_Settings->value(hostPrefix() + "presets/" + id + "/values").toMap();
    auto checked = cloneForSession();
    if (!checked->applyConnectionSnapshot(values)) return false;
    const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_Settings->setValue(hostPrefix() + "presets/" + newId + "/name", name.trimmed());
    m_Settings->setValue(hostPrefix() + "presets/" + newId + "/values", values);
    emit hostOptionsChanged(); return true;
}

StreamingPreferences* StreamingPreferences::get(QQmlEngine *qmlEngine)
{
    {
        QReadLocker readGuard(s_GlobalPrefsLock);

        // If we have a preference object and it's associated with a QML engine or
        // if the caller didn't specify a QML engine, return the existing object.
        if (s_GlobalPrefs && (s_GlobalPrefs->m_QmlEngine || !qmlEngine)) {
            // The lifetime logic here relies on the QML engine also being a singleton.
            Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            return s_GlobalPrefs;
        }
    }

    {
        QWriteLocker writeGuard(s_GlobalPrefsLock);

        // If we already have an preference object but the QML engine is now available,
        // associate the QML engine with the preferences.
        if (s_GlobalPrefs) {
            if (!s_GlobalPrefs->m_QmlEngine) {
                s_GlobalPrefs->m_QmlEngine = qmlEngine;
            }
            else {
                // We could reach this codepath if another thread raced with us
                // and created the object while we were outside the pref lock.
                Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            }
        }
        else {
            s_GlobalPrefs = new StreamingPreferences(qmlEngine);
        }

        return s_GlobalPrefs;
    }
}

StreamingPreferences* StreamingPreferences::getForTesting(std::unique_ptr<QSettings> settings)
{
    if (!settings) {
        qFatal("StreamingPreferences test seam requires an explicit QSettings store");
    }

    QWriteLocker writeGuard(s_GlobalPrefsLock);
    delete s_GlobalPrefs;
    s_GlobalPrefs = new StreamingPreferences(nullptr, std::move(settings));
    return s_GlobalPrefs;
}

void StreamingPreferences::resetForTesting()
{
    QWriteLocker writeGuard(s_GlobalPrefsLock);
    delete s_GlobalPrefs;
    s_GlobalPrefs = nullptr;
}

void StreamingPreferences::reload()
{
    QSettings& settings = *m_Settings;

    int defaultVer = settings.value(SER_DEFAULTVER, 0).toInt();

#ifdef Q_OS_DARWIN
    recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
#else
    // Wayland doesn't support modesetting, so use fullscreen desktop mode
    // unless we have a slow GPU (which can take advantage of wp_viewporter
    // to reduce GPU load with lower resolution video streams).
    if (WMUtils::isRunningWayland() && !WMUtils::isGpuSlow()) {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
    }
    else {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN;
    }
#endif

    automaticResolution = settings.value("automaticresolution", false).toBool();
    automaticFrameRate = settings.value("automaticframerate", false).toBool();
    // A saved legacy pointer mode still wins. An absent mode leaves the
    // choice to the stream, even on a host with its own display settings.
    automaticPointer = settings.value("automaticpointer", !settings.contains(SER_ABSMOUSEMODE)).toBool();
    displayTarget = settings.value("displaytarget", QStringLiteral("host")).toString();
    if (!QStringList({"host", "auto", "virtual", "physical"}).contains(displayTarget)) displayTarget = QStringLiteral("host");
    width = settings.value(SER_WIDTH, 1280).toInt();
    height = settings.value(SER_HEIGHT, 720).toInt();
    fps = settings.value(SER_FPS, 60).toInt();
    enableYUV444 = settings.value(SER_YUV444, false).toBool();
    bitrateKbps = settings.value(SER_BITRATE, getDefaultBitrate(width, height, fps, enableYUV444)).toInt();
    unlockBitrate = settings.value(SER_UNLOCK_BITRATE, false).toBool();
    bitrateKbps = PreferencesWorkflow::effectiveBitrate(bitrateKbps, unlockBitrate);
    autoAdjustBitrate = settings.value(SER_AUTOADJUSTBITRATE, true).toBool();
    enableVsync = settings.value(SER_VSYNC, true).toBool();
    gameOptimizations = settings.value(SER_GAMEOPTS, true).toBool();
    playAudioOnHost = settings.value(SER_HOSTAUDIO, false).toBool();
    multiController = settings.value(SER_MULTICONT, true).toBool();
    enableMdns = settings.value(SER_MDNS, true).toBool();
    quitAppAfter = settings.value(SER_QUITAPPAFTER, false).toBool();
    absoluteMouseMode = settings.value(SER_ABSMOUSEMODE, false).toBool();
    absoluteTouchMode = settings.value(SER_ABSTOUCHMODE, true).toBool();
    localCursor = settings.value(SER_LOCALCURSOR, DEFAULT_LOCAL_CURSOR).toBool();
    framePacing = settings.value(SER_FRAMEPACING, false).toBool();
    connectionWarnings = settings.value(SER_CONNWARNINGS, true).toBool();
    configurationWarnings = settings.value(SER_CONFWARNINGS, true).toBool();
    richPresence = settings.value(SER_RICHPRESENCE, true).toBool();
    gamepadMouse = settings.value(SER_GAMEPADMOUSE, true).toBool();
    detectNetworkBlocking = settings.value(SER_DETECTNETBLOCKING, true).toBool();
    showPerformanceOverlay = settings.value(SER_SHOWPERFOVERLAY, false).toBool();
    packetSize = settings.value(SER_PACKETSIZE, 0).toInt();
    connectionAddress = settings.value("connectionaddress", QString()).toString();
    viewScale = settings.value("viewscale", QStringLiteral("fit")).toString();
    if (viewScale != "actual") viewScale = QStringLiteral("fit");
    desktopScale = settings.value("desktopscale", QStringLiteral("auto")).toString();
    if (desktopScale != "1" && desktopScale != "2") desktopScale = QStringLiteral("auto");
    adaptiveBitrate = settings.value("adaptivebitrate", true).toBool();
    // The template seeds hosts seen for the first time, which start Automatic.
    manualStream = settings.value("manualstream", false).toBool();
    manualDisplay = settings.value("manualdisplay", false).toBool();
    manualInput = settings.value("manualinput", false).toBool();
    localTextInput = settings.value("localtextinput", false).toBool();
    swapMouseButtons = settings.value(SER_SWAPMOUSEBUTTONS, false).toBool();
    muteOnFocusLoss = settings.value(SER_MUTEONFOCUSLOSS, false).toBool();
    backgroundGamepad = settings.value(SER_BACKGROUNDGAMEPAD, false).toBool();
    reverseScrollDirection = settings.value(SER_REVERSESCROLL, false).toBool();
    swapFaceButtons = settings.value(SER_SWAPFACEBUTTONS, false).toBool();
    keepAwake = settings.value(SER_KEEPAWAKE, true).toBool();
    enableHdr = settings.value(SER_HDR, false).toBool();
    // CSK_FULLSCREEN out of the box (owner ruling 2026-07-19; upstream default
    // was CSK_OFF): a remote-desktop session is unusable without modifier
    // shortcuts reaching the host, and fullscreen-only capture keeps the local
    // desktop's own shortcuts working outside the stream. Only affects fresh
    // installs -- a stored value always wins.
    captureSysKeysMode = static_cast<CaptureSysKeysMode>(settings.value(SER_CAPTURESYSKEYS,
                                                         static_cast<int>(CaptureSysKeysMode::CSK_FULLSCREEN)).toInt());
    lastEnabledCaptureSysKeysMode = static_cast<CaptureSysKeysMode>(settings.value(
        SER_LASTENABLEDCAPTURESYSKEYS, captureSysKeysMode == CSK_ALWAYS ? CSK_ALWAYS : CSK_FULLSCREEN).toInt());
    if (lastEnabledCaptureSysKeysMode != CSK_FULLSCREEN && lastEnabledCaptureSysKeysMode != CSK_ALWAYS)
        lastEnabledCaptureSysKeysMode = CSK_FULLSCREEN;
    audioConfig = static_cast<AudioConfig>(settings.value(SER_AUDIOCFG,
                                                  static_cast<int>(AudioConfig::AC_STEREO)).toInt());
    videoCodecConfig = static_cast<VideoCodecConfig>(settings.value(SER_VIDEOCFG,
                                                  static_cast<int>(VideoCodecConfig::VCC_AUTO)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(settings.value(SER_VIDEODEC,
                                                  static_cast<int>(VideoDecoderSelection::VDS_AUTO)).toInt());
    windowMode = static_cast<WindowMode>(settings.value(SER_WINDOWMODE,
                                                        // Try to load from the old preference value too
                                                        static_cast<int>(settings.value(SER_FULLSCREEN, true).toBool() ?
                                                                             recommendedFullScreenMode : WindowMode::WM_WINDOWED)).toInt());
    uiDisplayMode = static_cast<UIDisplayMode>(settings.value(SER_UIDISPLAYMODE,
                                               static_cast<int>(settings.value(SER_STARTWINDOWED, true).toBool() ? UIDisplayMode::UI_WINDOWED
                                                                                                                 : UIDisplayMode::UI_MAXIMIZED)).toInt());
    language = static_cast<Language>(settings.value(SER_LANGUAGE,
                                                    static_cast<int>(Language::LANG_AUTO)).toInt());


    // Perform default settings updates as required based on last default version
    if (defaultVer < 1) {
#ifdef Q_OS_DARWIN
        // Update window mode setting on macOS from full-screen (old default) to borderless windowed (new default)
        if (windowMode == WindowMode::WM_FULLSCREEN) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
#endif
    }
    if (defaultVer < 2) {
        if (windowMode == WindowMode::WM_FULLSCREEN && WMUtils::isRunningWayland()) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
    }

    // Fixup VCC value to the new settings format with codec and HDR separate
    if (videoCodecConfig == VCC_FORCE_HEVC_HDR_DEPRECATED) {
        videoCodecConfig = VCC_AUTO;
        enableHdr = true;
    }

    // Version 1 originally forced VSync on and frame pacing off. Keep the
    // marker for later migrations, but honor imported settings.
    int starlightPrefsVersion = settings.value(SER_STARLIGHT_PREFS_VERSION, 0).toInt();
    if (starlightPrefsVersion < 1) {
        settings.setValue(SER_STARLIGHT_PREFS_VERSION, 1);
    }

    // Version 2 copies the loaded connection defaults into missing per-host
    // keys. Checking each key makes a partial migration safe to repeat.
    // See docs/design/2026-07-14-per-host-settings.md.
    if (starlightPrefsVersion < 2) {
        QStringList hostUuids;

        // Enumerate hosts directly from the SAME QSettings store (no
        // ComputerManager dependency -- safe to run here, inside reload(), at
        // construction). Prefer the crash-recovery backup array exactly like
        // ComputerManager's own startup path does, so a mid-write crash before
        // this migration ran still seeds from the same host list ComputerManager
        // itself would recover.
        int hostCount = settings.beginReadArray(SER_HOSTS_BACKUP);
        if (hostCount == 0) {
            settings.endArray();
            hostCount = settings.beginReadArray(SER_HOSTS);
        }
        for (int i = 0; i < hostCount; i++) {
            settings.setArrayIndex(i);
            QString uuid = settings.value(SER_HOST_UUID).toString();
            if (!uuid.isEmpty()) {
                hostUuids.append(uuid);
            }
        }
        settings.endArray();

        for (const QString& uuid : std::as_const(hostUuids)) {
            QString prefix = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/';
            for (const PerHostKey& key : k_PerHostKeys) {
                QString hostKey = prefix + QString::fromLatin1(key.serKey);
                if (!settings.contains(hostKey)) {
                    settings.setValue(hostKey, key.get(this));
                }
            }
        }

        settings.setValue(SER_STARLIGHT_PREFS_VERSION, 2);
    }

    // Automatic bitrate became the recommended mode. Everyone who hadn't picked a
    // bitrate of their own moves to it, once; a chosen Manual bitrate stays.
    if (settings.value(SER_STARLIGHT_PREFS_VERSION, 0).toInt() < 3) {
        if (autoAdjustBitrate) adaptiveBitrate = true;
        settings.beginGroup(SER_HOSTSETTINGS_GROUP);
        const QStringList hosts = settings.childGroups();
        settings.endGroup();
        for (const QString& uuid : hosts) {
            const QString prefix = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/';
            if (settings.value(prefix + SER_AUTOADJUSTBITRATE, true).toBool())
                settings.setValue(prefix + QStringLiteral("adaptivebitrate"), true);
        }
        settings.setValue(SER_STARLIGHT_PREFS_VERSION, 3);
    }

    // Every host became Automatic or Manual, and the settings that could only
    // make a stream worse went away. A host with a resolution, frame rate,
    // bitrate or codec of its own becomes Manual with those values kept; the
    // removed settings fall back to their automatic values, silently.
    if (settings.value(SER_STARLIGHT_PREFS_VERSION, 0).toInt() < 4) {
        settings.beginGroup(SER_HOSTSETTINGS_GROUP);
        const QStringList hosts = settings.childGroups();
        settings.endGroup();
        for (const QString& uuid : hosts) {
            const QString prefix = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/';
            QVariantMap stored;
            for (const PerHostKey& key : k_PerHostKeys) {
                const QString name = QString::fromLatin1(key.serKey);
                if (settings.contains(prefix + name)) stored.insert(name, settings.value(prefix + name));
            }
            // Only the per-host seeding above, in this same run, can have written it.
            stored.remove(QStringLiteral("manualstream"));
            if (stored.isEmpty()) continue;
            QVariantMap values = stored;
            sanitizeStreamValues(values);
            for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
                if ((stored.contains(it.key()) || it.key() == QLatin1String("manualstream"))
                        && stored.value(it.key()) != it.value())
                    settings.setValue(prefix + it.key(), it.value());
            }
        }
        settings.setValue(SER_STARLIGHT_PREFS_VERSION, 4);
    }

    // Version 5 recorded the Display and Input modes for existing hosts.
    // Version 6 clears a synthesized Manual input mode on display-only records.
    const int groupVersion = settings.value(SER_STARLIGHT_PREFS_VERSION, 0).toInt();
    if (groupVersion < 6) {
        settings.beginGroup(SER_HOSTSETTINGS_GROUP);
        const QStringList hosts = settings.childGroups();
        settings.endGroup();
        for (const QString& uuid : hosts) {
            const QString prefix = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/';
            const QString pointerKey = prefix + QStringLiteral("automaticpointer");
            const bool hasPointerMode = settings.contains(prefix + SER_ABSMOUSEMODE);
            const bool hasAutomaticPointer = settings.contains(pointerKey);
            const bool hasInput = hasPointerMode || hasAutomaticPointer || settings.contains(prefix + SER_LOCALCURSOR);
            const QString inputModeKey = prefix + QStringLiteral("manualinput");
            if (hasPointerMode && !hasAutomaticPointer) settings.setValue(pointerKey, false);
            if (!hasInput && settings.value(inputModeKey).toBool()) settings.remove(inputModeKey);
            if (groupVersion >= 5) continue;

            QVariantMap values;
            bool stored = false;
            for (const PerHostKey& key : k_PerHostKeys) {
                const QString name = QString::fromLatin1(key.serKey);
                // Only the per-host seeding above, in this same run, can have written these.
                if (name == QLatin1String("manualdisplay") || name == QLatin1String("manualinput")) continue;
                stored = stored || settings.contains(prefix + name);
                values.insert(name, settings.value(prefix + name, key.get(this)));
            }
            if (!stored) continue;
            settings.setValue(prefix + QStringLiteral("manualdisplay"), derivedManualDisplay(values));
            if (hasInput) settings.setValue(inputModeKey, derivedManualInput(values));
        }
        settings.setValue(SER_STARLIGHT_PREFS_VERSION, 6);
    }
    sanitizeLiveStreamValues();

    // Snapshot the PER-HOST key template now, after all one-time fixups above,
    // for setCurrentHost() to use as its fallback (see m_TemplateValues'
    // declaration in the header for why this must be a frozen snapshot rather
    // than a re-read of the live fields on every call).
    m_TemplateValues.clear();
    for (const PerHostKey& key : k_PerHostKeys) {
        m_TemplateValues.insert(QString::fromLatin1(key.serKey), key.get(this));
    }
}

// THREAD/REENTRANCY INVARIANT (documented per audit, not enforced in code):
// this mutates the shared singleton's per-host overlay fields with no lock.
// It is safe ONLY because every caller lives on the GUI thread and the UI
// makes the two hazardous overlaps unreachable: the sidebar (the only
// selection source) isn't interactable mid-stream, and a Session snapshots
// its preferences at construction. If either invariant changes (e.g. a
// background host-switcher or multi-session support), this needs a lock or a
// per-session preferences copy instead.
QString StreamingPreferences::lastSelectedHost() const
{
    return m_Settings ? m_Settings->value(QStringLiteral("lastselectedhost")).toString() : QString();
}

int StreamingPreferences::rememberedAutomaticBitrate(const QString& uuid, const QString& context)
{
    QSettings settings;
    const QString key = QStringLiteral("automaticbitrate/") + uuid;
    if (!context.isEmpty()) {
        const QString sample = key + QStringLiteral("/samples/") + QString::fromLatin1(
            QCryptographicHash::hash(context.toUtf8(), QCryptographicHash::Sha256).toHex());
        const qint64 age = QDateTime::currentSecsSinceEpoch() - settings.value(sample + QStringLiteral("/measuredAt"), 0).toLongLong();
        if (age >= 0 && age <= StreamPlanning::SampleLifetimeSeconds && settings.contains(sample + QStringLiteral("/kbps")))
            return settings.value(sample + QStringLiteral("/kbps")).toInt();
    }
    const qint64 age = QDateTime::currentSecsSinceEpoch() - settings.value(key + QStringLiteral("/measuredAt"), 0).toLongLong();
    if (!context.isEmpty() && (context != settings.value(key + QStringLiteral("/context")).toString()
                              || age < 0 || age > StreamPlanning::SampleLifetimeSeconds)) return 0;
    return settings.value(key, 0).toInt();
}

void StreamingPreferences::rememberAutomaticBitrate(const QString& uuid, int kbps, const QString& context)
{
    QSettings settings;
    const QString key = QStringLiteral("automaticbitrate/") + uuid;
    settings.setValue(key, kbps);
    settings.setValue(key + QStringLiteral("/context"), context);
    settings.setValue(key + QStringLiteral("/measuredAt"), QDateTime::currentSecsSinceEpoch());
    if (!context.isEmpty()) {
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(context.toUtf8(), QCryptographicHash::Sha256).toHex());
        settings.beginGroup(key + QStringLiteral("/samples"));
        settings.setValue(hash + QStringLiteral("/kbps"), kbps);
        settings.setValue(hash + QStringLiteral("/measuredAt"), QDateTime::currentSecsSinceEpoch());
        auto groups = settings.childGroups();
        std::sort(groups.begin(), groups.end(), [&](const QString& a, const QString& b) {
            if (a == b) return false;
            if (a == hash || b == hash) return a == hash;
            return settings.value(a + "/measuredAt").toLongLong() > settings.value(b + "/measuredAt").toLongLong();
        });
        for (int i = 8; i < groups.size(); ++i) settings.remove(groups[i]);
    }

}

void StreamingPreferences::setLastSelectedHost(const QString& uuid)
{
    if (m_Settings && lastSelectedHost() != uuid)
        m_Settings->setValue(QStringLiteral("lastselectedhost"), uuid);
}

void StreamingPreferences::setCurrentHost(const QString& uuid)
{
    if (!m_Settings) return;
    if (m_CurrentHostUuidValid && m_CurrentHostUuid == uuid) {
        // Idempotent no-op. The live fields can't have drifted underneath us:
        // save() only ever writes back to the currently-selected host's own
        // hostsettings block (or to the flat template when no host is
        // selected), so there is nothing to reload or re-notify.
        return;
    }

    QSettings& settings = *m_Settings;
    QString prefix = uuid.isEmpty() ? QString() : (QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/');

    for (const PerHostKey& key : k_PerHostKeys) {
        QString keyStr = QString::fromLatin1(key.serKey);
        QVariant fallback = m_TemplateValues.value(keyStr);
        QVariant value = prefix.isEmpty() ? fallback : settings.value(prefix + keyStr, fallback);
        key.set(this, value);
    }

    QStringList derivedModes;
    for (const char* mode : k_ModeKeys)
        if (!prefix.isEmpty() && !settings.contains(prefix + mode)) derivedModes.append(QString::fromLatin1(mode));
    sanitizeLiveStreamValues(derivedModes);

    m_CurrentHostUuid = uuid;
    m_CurrentHostUuidValid = true;

    // Fire every connection-scope NOTIFY signal so QML bindings re-evaluate
    // against the newly-overlaid values (design §3). App-scope signals are
    // untouched -- this call never affects them.
    bitrateKbps = PreferencesWorkflow::effectiveBitrate(bitrateKbps, unlockBitrate);
    emit displayModeChanged();
    emit bitrateChanged();
    emit unlockBitrateChanged();
    emit autoAdjustBitrateChanged();
    emit enableVsyncChanged();
    emit framePacingChanged();
    emit localCursorChanged();
    emit absoluteMouseModeChanged();
    emit audioConfigChanged();
    emit videoCodecConfigChanged();
    emit enableHdrChanged();
    emit enableYUV444Changed();
    emit videoDecoderSelectionChanged();
    emit packetSizeChanged();
    emit hostOptionsChanged();
}

void StreamingPreferences::sanitizeLiveStreamValues(const QStringList& derivedModes)
{
    QVariantMap values = connectionSnapshot();
    for (const QString& mode : derivedModes) values.remove(mode);
    sanitizeStreamValues(values);
    for (const PerHostKey& key : k_PerHostKeys) key.set(this, values.value(QString::fromLatin1(key.serKey)));
}

bool StreamingPreferences::hasStoredResolution(const QString& uuid) const
{
    if (uuid.isEmpty()) {
        // No host selected -- nothing for the caller to auto-default.
        return true;
    }

    QSettings& settings = *m_Settings;
    QString key = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/' + QString::fromLatin1(SER_WIDTH);
    return settings.contains(key);
}

bool StreamingPreferences::initializeHostDisplayMode(const QString& uuid,
                                                     int nativeWidth,
                                                     int nativeHeight,
                                                     int nativeRefreshRate)
{
    if (uuid.isEmpty() || nativeWidth < 256 || nativeWidth > 8192 ||
            nativeHeight < 256 || nativeHeight > 8192) {
        return false;
    }

    setCurrentHost(uuid);

    QSettings& settings = *m_Settings;
    const QString prefix = QString(SER_HOSTSETTINGS_GROUP) + '/' + uuid + '/';
    const QString widthKey = prefix + QString::fromLatin1(SER_WIDTH);
    const QString heightKey = prefix + QString::fromLatin1(SER_HEIGHT);
    const QString fpsKey = prefix + QString::fromLatin1(SER_FPS);
    const bool hasWidth = settings.contains(widthKey);
    const bool hasHeight = settings.contains(heightKey);
    const bool hasFps = settings.contains(fpsKey);

    bool displayModeWasChanged = false;
    if ((!hasWidth && !hasHeight) || automaticResolution) {
        displayModeWasChanged = width != nativeWidth || height != nativeHeight || !hasWidth || !hasHeight;
        width = nativeWidth;
        height = nativeHeight;
        settings.setValue(widthKey, width);
        settings.setValue(heightKey, height);
        automaticResolution = true;
        settings.setValue(prefix + "automaticresolution", true);
        if (!hasWidth && !hasHeight && !settings.contains(prefix + "displaytarget")) {
            displayTarget = QStringLiteral("auto");
            settings.setValue(prefix + "displaytarget", displayTarget);
        }
        // Existing explicit input choices remain authoritative. Fresh hosts use
        // absolute desktop input; Session selects relative input for games.
        if (!hasWidth && !hasHeight && !settings.contains(prefix + SER_ABSMOUSEMODE)) {
            automaticPointer = true;
            absoluteMouseMode = true;
            settings.setValue(prefix + "automaticpointer", true);
            settings.setValue(prefix + SER_ABSMOUSEMODE, true);
            emit absoluteMouseModeChanged();
        }
        // A host seen for the first time starts Automatic, whatever an older
        // template chose for the hosts before it.
        if (!hasWidth && !hasHeight && !settings.contains(prefix + "manualstream")) {
            manualStream = false;
            autoAdjustBitrate = true;
            adaptiveBitrate = true;
            videoCodecConfig = VCC_AUTO;
            settings.setValue(prefix + "manualstream", false);
            settings.setValue(prefix + SER_AUTOADJUSTBITRATE, true);
            settings.setValue(prefix + "adaptivebitrate", true);
            settings.setValue(prefix + SER_VIDEOCFG, int(VCC_AUTO));
            emit autoAdjustBitrateChanged();
            emit videoCodecConfigChanged();
            emit hostOptionsChanged();
        }
        // Likewise the local cursor: on for a host seen for the first time,
        // even when this profile's template still carries the old "off"
        // default. A host that already has a saved choice keeps it.
        if (!hasWidth && !hasHeight && !settings.contains(prefix + SER_LOCALCURSOR)) {
            localCursor = true;
            settings.setValue(prefix + SER_LOCALCURSOR, true);
            emit localCursorChanged();
        }
        // Display and input start Automatic too, unless a choice of this host's
        // own is already saved.
        if (!hasWidth && !hasHeight && !settings.contains(prefix + "manualdisplay")) {
            if (!settings.contains(prefix + "desktopscale")) desktopScale = QStringLiteral("auto");
            if (!settings.contains(prefix + "viewscale")) viewScale = QStringLiteral("fit");
            manualDisplay = derivedManualDisplay(connectionSnapshot());
            settings.setValue(prefix + "desktopscale", desktopScale);
            settings.setValue(prefix + "viewscale", viewScale);
            settings.setValue(prefix + "manualdisplay", manualDisplay);
            emit hostOptionsChanged();
        }
        if (!hasWidth && !hasHeight && !settings.contains(prefix + "manualinput")) {
            manualInput = derivedManualInput(connectionSnapshot());
            settings.setValue(prefix + "manualinput", manualInput);
            emit hostOptionsChanged();
        }
    }
    if (!hasFps || automaticFrameRate) {
        const int localFps = qBound(10, nativeRefreshRate > 0 ? nativeRefreshRate : 60, 480);
        displayModeWasChanged = displayModeWasChanged || fps != localFps || !hasFps;
        fps = localFps;
        automaticFrameRate = true;
        settings.setValue(fpsKey, fps);
        settings.setValue(prefix + "automaticframerate", true);
    }

    if (!displayModeWasChanged) {
        if (hostPresets().isEmpty()) saveHostPreset(tr("Default"));
        return false;
    }

    bool bitrateWasChanged = false;
    if (autoAdjustBitrate) {
        const int adjustedBitrate = effectiveBitrate(getDefaultBitrate(width, height, fps, enableYUV444));
        bitrateWasChanged = bitrateKbps != adjustedBitrate;
        bitrateKbps = adjustedBitrate;
        settings.setValue(prefix + QString::fromLatin1(SER_BITRATE), bitrateKbps);
    }

    if (hostPresets().isEmpty()) saveHostPreset(tr("Default"));
    emit displayModeChanged();
    if (bitrateWasChanged) {
        emit bitrateChanged();
    }
    emit saveCompleted();
    return true;
}

void StreamingPreferences::rememberCliOverride(const QByteArray& propertyName, const QVariant& value)
{
    m_CliOverrides.insert(propertyName, value);
}

void StreamingPreferences::reapplyCliOverrides()
{
    // QObject::setProperty() on a MEMBER property both writes the field and
    // fires its NOTIFY signal, so this doesn't need the PerHostKey table --
    // it just needs the same public property names QML already binds to.
    for (auto it = m_CliOverrides.constBegin(); it != m_CliOverrides.constEnd(); ++it) {
        setProperty(it.key().constData(), it.value());
    }
    const int effective = effectiveBitrate(bitrateKbps);
    if (effective != bitrateKbps) { bitrateKbps = effective; emit bitrateChanged(); }
}

std::optional<bool> StreamingPreferences::cliFramePacingOverride() const
{
    const QVariant pacing = m_CliOverrides.value(QByteArrayLiteral("framePacing"));
    if (!pacing.isValid()) return std::nullopt;
    return pacing.toBool();
}

bool StreamingPreferences::retranslate()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (m_QmlEngine != nullptr) {
        return false;
    }
#endif

    const bool applied =
        m_LanguageWorkflow->applyTranslation(static_cast<int>(language));
    if (applied && m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        m_QmlEngine->retranslate();
#endif
    }
    return applied;
}

QVariantMap StreamingPreferences::changeLanguage(int requestedLanguage)
{
    static_assert(static_cast<int>(LANG_AUTO) == LanguageWorkflow::Automatic);
    static_assert(static_cast<int>(LANG_EN) == LanguageWorkflow::English);
    static_assert(static_cast<int>(LANG_TA) == LanguageWorkflow::LastLanguage);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    constexpr bool dynamicRetranslationSupported = true;
#else
    const bool dynamicRetranslationSupported = m_QmlEngine == nullptr;
#endif

    StreamingLanguageState state(*this);
    const LanguageWorkflow::Decision decision =
        m_LanguageWorkflow->changeLanguage(
            state,
            requestedLanguage,
            dynamicRetranslationSupported);

    QVariantMap result {
        { QStringLiteral("changed"), false },
        { QStringLiteral("appliedImmediately"), false },
        { QStringLiteral("clearOnBack"), decision.clearOnBack },
        { QStringLiteral("showRestartNotice"), decision.showRestartNotice },
    };
    if (decision.status == LanguageWorkflow::Status::InvalidInput ||
            decision.status == LanguageWorkflow::Status::Unchanged) {
        return result;
    }

    result.insert(QStringLiteral("changed"), true);

    if (decision.retranslate) {
        if (m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            m_QmlEngine->retranslate();
#endif
        }
        result.insert(QStringLiteral("appliedImmediately"), true);
    }

    return result;
}

void StreamingPreferences::save()
{
    if (!m_Settings) return; // Immutable session/trial copy, never production state.
    const int effective = PreferencesWorkflow::effectiveBitrate(bitrateKbps, unlockBitrate);
    if (effective != bitrateKbps) { bitrateKbps = effective; emit bitrateChanged(); }
    if (captureSysKeysMode != CSK_OFF) lastEnabledCaptureSysKeysMode = captureSysKeysMode;
    QSettings& settings = *m_Settings;

    // PER-HOST subset (design §3): goes to hostsettings/<uuid>/* while a host
    // is selected. If no host is selected, fall back to the flat template keys
    // -- today's pre-feature behavior -- and keep the frozen template snapshot
    // in sync with what was just written, so a later setCurrentHost() call in
    // this same run sees the freshly-saved value instead of a stale one.
    bool hasHost = m_CurrentHostUuidValid && !m_CurrentHostUuid.isEmpty();
    QString prefix = hasHost ? (QString(SER_HOSTSETTINGS_GROUP) + '/' + m_CurrentHostUuid + '/') : QString();

    for (const PerHostKey& key : k_PerHostKeys) {
        QVariant value = key.get(this);
        QString keyStr = QString::fromLatin1(key.serKey);
        if (hasHost) {
            settings.setValue(prefix + keyStr, value);
        }
        else {
            settings.setValue(keyStr, value);
            m_TemplateValues.insert(keyStr, value);
        }
    }

    // GLOBAL subset (design §1): unchanged, flat keys as always.
    settings.setValue(SER_GAMEOPTS, gameOptimizations);
    settings.setValue(SER_HOSTAUDIO, playAudioOnHost);
    settings.setValue(SER_MULTICONT, multiController);
    settings.setValue(SER_MDNS, enableMdns);
    settings.setValue(SER_QUITAPPAFTER, quitAppAfter);
    settings.setValue(SER_ABSTOUCHMODE, absoluteTouchMode);
    settings.setValue(SER_CONNWARNINGS, connectionWarnings);
    settings.setValue(SER_CONFWARNINGS, configurationWarnings);
    settings.setValue(SER_RICHPRESENCE, richPresence);
    settings.setValue(SER_GAMEPADMOUSE, gamepadMouse);
    settings.setValue(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    settings.setValue(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    settings.setValue(SER_WINDOWMODE, static_cast<int>(windowMode));
    settings.setValue(SER_UIDISPLAYMODE, static_cast<int>(uiDisplayMode));
    settings.setValue(SER_LANGUAGE, static_cast<int>(language));
    settings.setValue(SER_DEFAULTVER, CURRENT_DEFAULT_VER);
    settings.setValue(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    settings.setValue(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    settings.setValue(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    settings.setValue(SER_REVERSESCROLL, reverseScrollDirection);
    settings.setValue(SER_SWAPFACEBUTTONS, swapFaceButtons);
    settings.setValue(SER_CAPTURESYSKEYS, captureSysKeysMode);
    settings.setValue(SER_LASTENABLEDCAPTURESYSKEYS, lastEnabledCaptureSysKeysMode);
    settings.setValue(SER_KEEPAWAKE, keepAwake);
    emit saveCompleted();
    emit hostOptionsChanged();
}

bool StreamingPreferences::setAutomaticResolution(int nativeWidth, int nativeHeight)
{
    if (nativeWidth < 256 || nativeWidth > 8192 || nativeHeight < 256 || nativeHeight > 8192) return false;
    const bool changed = !automaticResolution || width != nativeWidth || height != nativeHeight;
    automaticResolution = true;
    width = nativeWidth; height = nativeHeight;
    if (autoAdjustBitrate) {
        bitrateKbps = effectiveBitrate(getDefaultBitrate(width, height, fps, enableYUV444));
        emit bitrateChanged();
    }
    if (changed) { emit displayModeChanged(); save(); }
    return changed;
}

bool StreamingPreferences::resetLearnedPerformance()
{
    if (!m_Settings || m_CurrentHostUuid.isEmpty()) return false;
    m_Settings->remove(HostCapacity::key(m_CurrentHostUuid));
    m_Settings->remove(QStringLiteral("automaticbitrate/") + m_CurrentHostUuid);
    m_Settings->remove(QStringLiteral("automaticpacing/") + m_CurrentHostUuid);
    emit hostOptionsChanged();
    return true;
}

int StreamingPreferences::effectiveBitrate(int value) const
{
    return PreferencesWorkflow::effectiveBitrate(value, unlockBitrate);
}

QVariantMap StreamingPreferences::streamPlan(const QString& hostName, int nativeWidth, int nativeHeight,
                                             int displayHz, int serverCodecModeSupport, int width, int height,
                                             int fps, int bitrateKbps, const QString& codec, double displayScale, bool integerDesktopScale, const QString& performanceContext, const QString& screenName) const
{
    StreamPlanInput input;
    input.hostName = hostName;
    input.nativeWidth = nativeWidth;
    input.nativeHeight = nativeHeight;
    input.displayHz = displayHz;
    const QScreen* screen = QGuiApplication::primaryScreen();
    input.displayScale = displayScale > 0 ? displayScale : screen ? screen->devicePixelRatio() : 0;
    input.integerDesktopScale = integerDesktopScale;
    if (!performanceContext.isEmpty()) input.performanceContext = StreamPlanning::viewingContext(performanceContext,
        nativeWidth,nativeHeight,displayHz,input.displayScale);
    input.serverCodecModeSupport = quint32(serverCodecModeSupport);
    input.width = width;
    input.height = height;
    input.fps = fps;
    input.bitrateKbps = bitrateKbps;
    input.codec = codec;
    // As the last stream found them; until then, only H.264 is taken for granted.
    QSettings decodes;
    decodes.beginGroup(QStringLiteral("clientdecode"));
    const QString context = StreamPlanning::decoderContext(screenName.isEmpty() && screen ? screen->name() : screenName,
        width > 0 ? width : nativeWidth, height > 0 ? height : nativeHeight,
        fps > 0 ? fps : displayHz, input.displayScale, int(videoDecoderSelection));
    const qint64 age = QDateTime::currentSecsSinceEpoch() - decodes.value(QStringLiteral("measuredAt"),0).toLongLong();
    const bool qualified = context == decodes.value(QStringLiteral("context")).toString() && age >= 0 && age <= 86400;
    input.h264 = !qualified || decodes.value(QStringLiteral("h264"), true).toBool();
    input.hevc = qualified && decodes.value(QStringLiteral("hevc"), false).toBool();
    input.av1 = qualified && decodes.value(QStringLiteral("av1"), false).toBool();
    if (qualified) input.capacity = HostCapacity::load(m_CurrentHostUuid);

    const StreamPlan plan = planStream(input);
    return {{QStringLiteral("width"), plan.width}, {QStringLiteral("height"), plan.height},
            {QStringLiteral("fps"), plan.fps}, {QStringLiteral("codec"), plan.codec},
            {QStringLiteral("startBitrateKbps"), plan.startBitrateKbps},
            {QStringLiteral("ceilingKbps"), plan.ceilingKbps}, {QStringLiteral("floorKbps"), plan.floorKbps},
            {QStringLiteral("limitedByHost"), plan.limitedByHost}, {QStringLiteral("limitReason"), plan.limitReason},
            {QStringLiteral("resolutionWarning"), plan.resolutionWarning},
            {QStringLiteral("fpsWarning"), plan.fpsWarning},
            {QStringLiteral("codecWarning"), plan.codecWarning},
            {QStringLiteral("bitrateWarning"), plan.bitrateWarning}};
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps, bool yuv444)
{
    return PreferencesWorkflow::defaultBitrate(width, height, fps, yuv444);
}

bool StreamingPreferences::setResolution(int newWidth, int newHeight)
{
    return PreferencesWorkflow::applyResolution(*this, { newWidth, newHeight });
}

bool StreamingPreferences::setFrameRate(int newFps)
{
    return PreferencesWorkflow::applyFrameRate(*this, newFps);
}

bool StreamingPreferences::setAutomaticFrameRate(int nativeRefreshRate)
{
    const int localFps = qBound(10, nativeRefreshRate > 0 ? nativeRefreshRate : 60, 480);
    const bool changed = !automaticFrameRate || fps != localFps;
    automaticFrameRate = true;
    fps = localFps;
    if (autoAdjustBitrate) {
        bitrateKbps = effectiveBitrate(getDefaultBitrate(width, height, fps, enableYUV444));
        emit bitrateChanged();
    }
    if (changed) { emit displayModeChanged(); save(); }
    return changed;
}

// These concrete compatibility-adapter definitions live beside
// StreamingPreferences so pure workflow test links remain independent of the
// singleton. The command and exactly-one-save ordering are still owned by
// PreferencesWorkflow; the singleton methods above are one-line facades.
bool PreferencesWorkflow::applyResolution(
    StreamingPreferences& preferences,
    const QSize& resolution)
{
    if (resolution.width() < 256 || resolution.height() < 256 ||
            resolution.width() > 8192 || resolution.height() > 8192) return false;
    const bool wasAutomatic = preferences.automaticResolution;
    preferences.automaticResolution = false;
    if (commit(preferences, changeResolution(stateOf(preferences), resolution))) return true;
    if (wasAutomatic) { emit preferences.displayModeChanged(); preferences.save(); return true; }
    return false;
}

bool PreferencesWorkflow::applyFrameRate(
    StreamingPreferences& preferences,
    int frameRate)
{
    if (frameRate < 10 || frameRate > 9999) return false;
    const bool wasAutomatic = preferences.automaticFrameRate;
    preferences.automaticFrameRate = false;
    if (commit(preferences, changeFrameRate(stateOf(preferences), frameRate))) return true;
    if (wasAutomatic) { emit preferences.displayModeChanged(); preferences.save(); return true; }
    return false;
}

PreferencesWorkflow::State PreferencesWorkflow::stateOf(
    const StreamingPreferences& preferences)
{
    return {
        { preferences.width, preferences.height },
        preferences.fps,
        preferences.bitrateKbps,
        preferences.autoAdjustBitrate,
        preferences.enableYUV444,
        preferences.unlockBitrate,
    };
}

bool PreferencesWorkflow::commit(
    StreamingPreferences& preferences,
    const Transaction& transaction)
{
    if (!transaction.saveRequired()) {
        return false;
    }

    const bool displayModeWasChanged =
        preferences.width != transaction.state.resolution.width() ||
        preferences.height != transaction.state.resolution.height() ||
        preferences.fps != transaction.state.frameRate;
    const bool bitrateWasChanged =
        preferences.bitrateKbps != transaction.state.bitrateKbps;

    preferences.width = transaction.state.resolution.width();
    preferences.height = transaction.state.resolution.height();
    preferences.fps = transaction.state.frameRate;
    preferences.bitrateKbps = transaction.state.bitrateKbps;

    if (displayModeWasChanged) {
        emit preferences.displayModeChanged();
    }
    if (bitrateWasChanged) {
        emit preferences.bitrateChanged();
    }

    preferences.save();
    return true;
}

QString StreamingPreferences::displayTargetForHost(const QString& uuid) const
{
    if (m_CurrentHostUuidValid && m_CurrentHostUuid == uuid)
        return manualDisplay ? displayTarget : QStringLiteral("auto");
    const QString prefix = uuid.isEmpty() ? QString() : QStringLiteral("hostsettings/") + uuid + '/';
    QVariantMap values;
    for (const char* key : {"displaytarget", "desktopscale", "viewscale", "manualdisplay"}) {
        const QString name = QString::fromLatin1(key);
        values.insert(name, m_Settings && !prefix.isEmpty() ? m_Settings->value(prefix + name, m_TemplateValues.value(name))
                                                          : m_TemplateValues.value(name));
    }
    if (m_Settings && !prefix.isEmpty() && !m_Settings->contains(prefix + "manualdisplay"))
        values.remove("manualdisplay");
    const bool manual = values.contains("manualdisplay") ? values.value("manualdisplay").toBool()
                                                        : derivedManualDisplay(values);
    return manual ? values.value("displaytarget", QStringLiteral("host")).toString() : QStringLiteral("auto");
}
