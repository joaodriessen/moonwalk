#pragma once

#include <QMap>
#include <QXmlStreamReader>

// Live evidence only. A public serverinfo Permission=0 does not describe the
// paired client's rights, and an Apollo-derived protocol is not a product name.
struct HostCapabilities
{
    bool apollo = false;
    bool virtualDisplay = false;
    bool virtualDisplayReady = false;
    bool virtualDisplayHdr = false;
    qint64 permissions = -1;

    enum Permission : quint32 {
        Mouse = 1u << 11, Keyboard = 1u << 12,
        ClipboardWrite = 1u << 16, ClipboardRead = 1u << 17,
        View = (1u << 25) | (1u << 26), Launch = 1u << 26
    };
    bool allows(quint32 mask) const { return permissions < 0 || (quint32(permissions) & mask) != 0; }
    int displayRequest(const QString& target, bool desktop) const {
        if (!virtualDisplay) return -1;
        if (target == "physical") return 0;
        if (target == "virtual") return 1;
        return target == "auto" && desktop && virtualDisplayReady ? 1 : -1;
    }
    static HostCapabilities parse(const QString& xml)
    {
        QMap<QString, QString> fields;
        QXmlStreamReader reader(xml);
        if (reader.readNextStartElement()) {
            while (reader.readNextStartElement())
                fields.insert(reader.name().toString(), reader.readElementText(QXmlStreamReader::SkipChildElements));
        }
        if (reader.hasError()) return {};
        auto flag = [&](const char* key) { const auto value = fields.value(QLatin1String(key)); return value == "1" || value == "true"; };
        HostCapabilities caps;
        caps.apollo = fields.contains("Permission");
        caps.virtualDisplay = flag("VirtualDisplayCapable");
        caps.virtualDisplayReady = flag("VirtualDisplayDriverReady");
        caps.virtualDisplayHdr = flag("VirtualDisplayHDRCapable");
        if (caps.apollo && fields.value("PairStatus") == "1") {
            bool ok = false;
            const auto value = fields.value("Permission").toULongLong(&ok);
            if (ok && value <= 0xffffffffu) caps.permissions = qint64(value);
        }
        return caps;
    }
    bool operator==(const HostCapabilities& other) const {
        return apollo == other.apollo && virtualDisplay == other.virtualDisplay &&
            virtualDisplayReady == other.virtualDisplayReady && virtualDisplayHdr == other.virtualDisplayHdr &&
            permissions == other.permissions;
    }
    bool operator!=(const HostCapabilities& other) const { return !(*this == other); }
};
