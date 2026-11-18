#pragma once

#include "backend/nvhttp.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QWaitCondition>
#include <QXmlStreamReader>
#include <functional>

// Conservative client-side control, separate from Skylight's feedback protocol.
// It never raises the user's ceiling or treats an idle/frozen picture as proof
// that more bandwidth is available. Targets become current only after host ACK.
struct RuntimeBitratePolicy {
    static int next(int current, int ceiling, bool poor, bool freshFrames, qint64 sinceChange) {
        const int floor = qMin(1000, ceiling);
        if (poor && sinceChange >= 3000) return qMax(floor, current * 3 / 4);
        if (!poor && freshFrames && sinceChange >= 10000)
            return qMin(ceiling, current + qMax(500, current / 10));
        return current;
    }
    static bool supported(const QByteArray& body) {
        const auto object = QJsonDocument::fromJson(body).object();
        return object.value("version").toInt() == 1 &&
               object.value("features").toArray().contains(QStringLiteral("runtime_bitrate"));
    }
    static int applied(const QByteArray& body) {
        QXmlStreamReader xml(body);
        if (!xml.readNextStartElement() || xml.name() != QLatin1String("root") ||
                xml.attributes().value("status_code") != QLatin1String("200")) return 0;
        int bitrate = 0;
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("bitrate")) bitrate = xml.readElementText().toInt();
            else xml.skipCurrentElement();
        }
        return !xml.hasError() && bitrate >= 500 && bitrate <= 500000 ? bitrate : 0;
    }
    // What the host's encoder makes of a bitrate asked for at launch. Launch asks for a
    // budget on the wire and the host takes FEC, audio and packet overhead out of it
    // (Vibepollo rtsp.cpp); /bitrate goes to the encoder as it is. So everything here
    // is in launch units, the one people choose, and only /bitrate is in these.
    static int encoderKbps(int wireKbps, int audioChannels = 2) {
        // FEC 20%, audio up to a fifth, overhead up to a tenth; counted in tenths so
        // that a higher launch value never gives a lower encoder value.
        const qint64 wire = wireKbps, audio = 256 * qMax(2, audioChannels);
        return int((wire * 8 - qMin(audio * 10, wire * 2) - qMin<qint64>(5000, wire)) / 10);
    }
    // The launch bitrate that gives the encoder at least this much.
    static int wireKbps(int encoderKbps, int audioChannels = 2) {
        int low = encoderKbps, high = encoderKbps * 2 + 4000;
        while (low < high) {
            const int middle = low + (high - low) / 2;
            if (RuntimeBitratePolicy::encoderKbps(middle, audioChannels) >= encoderKbps) high = middle;
            else low = middle + 1;
        }
        return low;
    }
};

class RuntimeBitrate : public QThread {
    Q_OBJECT
public:
    // freshFrames: video has started and is still arriving. A host given a bitrate
    // before its first frame keeps it for some later encoder restart instead.
    struct Observation { bool poor; bool freshFrames; };
    // Bitrates in and out are in launch units, whatever the host is told.
    RuntimeBitrate(NvComputer* computer, int ceiling, bool adaptive,
                   std::function<Observation()> observe, int audioChannels = 2);
    ~RuntimeBitrate() override;
    void requestBitrate(int bitrate);
    void stopAndWait();
signals:
    void changed(bool supported, int bitrate, const QString& notice);
    // The host confirmed a new bitrate. Emitted on this thread, as soon as it answers.
    void applied(int bitrate);
protected:
    void run() override;
private:
    NvComputer* m_Computer;
    const int m_InitialCeiling;
    const bool m_Adaptive;
    std::function<Observation()> m_Observe;
    const int m_AudioChannels;
    NvHTTP::CancelToken m_Cancel;
    QMutex m_Mutex;
    QWaitCondition m_Wake;
    int m_Pending = 0;
};
