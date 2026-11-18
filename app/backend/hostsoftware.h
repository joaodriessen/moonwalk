#pragma once

#include <QMap>
#include <QXmlStreamReader>

// Which host software answered a /serverinfo, and its version when it really
// reports one. The Sunshine family all send a SUNSHINE_* state and the same
// GFE-compatible appversion/GfeVersion constants, so those name no product.
// What does differ (upstream serverinfo handlers, checked 2026-11-03):
// - Moonwalk Host sends <HostOs> and <SkylightVersion> (its src/nvhttp.cpp).
// - Apollo (ClassicOldSong/Apollo adc5c5a, src/nvhttp.cpp:931,943) and its
//   forks always send <Permission>.
// - Nonary's Vibepollo is the Apollo-based fork: from 2.0.0 it adds the frame
//   limiter and HDR virtual display fields (2.0.1 src/nvhttp.cpp:3876-3900),
//   which Apollo (adc5c5a, v0.4.6, v0.4.8) never sends. From 2.0.2 it also sends
//   <HardwareMetricsVersion> (master src/nvhttp.cpp:4240). Vibepollo 1.x looks
//   exactly like Apollo here.
// - Nonary's Vibeshine (9c02f26, src/nvhttp.cpp:4005) is Sunshine-based: the
//   same Vibepollo fields, but no <Permission>.
// - Stock Sunshine (LizardByte/Sunshine 9793d74, src/nvhttp.cpp:1243-1305)
//   sends neither, and none of the family reports its own version.
// - GeForce Experience answers with its MJOLNIR state codename, and its
//   GfeVersion is the installed version; Sunshine's is the constant 3.23.0.74.
struct HostSoftware
{
    QString product; // "Skylight" | "Sunshine" | "Apollo" | "Vibeshine" | "Vibepollo" | "GeForce Experience" | ""
    QString version;

    static HostSoftware identify(const QString& xml)
    {
        QMap<QString, QString> fields;
        QXmlStreamReader reader(xml);
        if (reader.readNextStartElement()) {
            while (reader.readNextStartElement())
                fields.insert(reader.name().toString(), reader.readElementText(QXmlStreamReader::SkipChildElements));
        }
        if (reader.hasError()) return {};
        const QString os = fields.value("HostOs").trimmed().toLower();
        const QString state = fields.value("state");
        HostSoftware software;
        // Only the exact values Moonwalk Host sends count, so a stray tag
        // can't label some other host.
        if (os == "macos" || os == "windows" || os == "linux") {
            software.product = QStringLiteral("Skylight");
            software.version = fields.value("SkylightVersion").trimmed();
        }
        else if (state.startsWith(QStringLiteral("SUNSHINE"))) {
            const bool apollo = fields.contains("Permission");
            const bool vibe = fields.contains("HardwareMetricsVersion") || fields.contains("FrameLimiterSupported")
                || fields.contains("VirtualDisplayHDRCapable");
            if (vibe)
                software.product = apollo ? QStringLiteral("Vibepollo") : QStringLiteral("Vibeshine");
            else
                software.product = apollo ? QStringLiteral("Apollo") : QStringLiteral("Sunshine");
        }
        else if (state.contains(QStringLiteral("MJOLNIR"))) {
            software.product = QStringLiteral("GeForce Experience");
            software.version = fields.value("GfeVersion").trimmed();
        }
        return software;
    }

    // "Moonwalk Host (0.4.0-alpha.15)", "Apollo", ...: the protocol still says
    // "Skylight", but people know the product as Moonwalk Host.
    static QString label(const QString& product, const QString& version)
    {
        if (product.isEmpty()) return QString();
        const QString name = product == QLatin1String("Skylight") ? QStringLiteral("Moonwalk Host") : product;
        return version.isEmpty() ? name : name + QStringLiteral(" (") + version + QLatin1Char(')');
    }
};
