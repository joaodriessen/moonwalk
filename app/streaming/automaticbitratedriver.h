#pragma once

#include "automaticbitrate.h"
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <functional>

// Runs the controller for one stream: measurements come from the receive thread,
// and ten times a second what it decides goes to whoever can act on it.
class AutomaticBitrateDriver : public QThread {
    Q_OBJECT
public:
    // Handed each decision that's different, or due again. Called on this thread.
    using Deliver = std::function<void(const AutomaticBitrate::Decision&)>;

    AutomaticBitrateDriver(int startKbps, int ceilingKbps, int framesPerSecond, int floorKbps,
                           bool hostStampsEveryFrame, Deliver deliver, bool changesRebuild = false);
    ~AutomaticBitrateDriver() override;

    // Takes the common-c frame observer. Only while the connection is stopped.
    void observeConnection();
    void stopAndWait();
    AutomaticBitrate::Status status() const { return m_Controller.status(); }
    // The bitrate this stream settled on, for starting the next one. None when it only
    // ran down to the floor without real loss: that says nothing about the link.
    int settledKbps() const;
    // The host confirmed a change. Safe from any thread.
    void changeApplied();
    AutomaticBitrate::Controller& controller() { return m_Controller; }

    // On the wire to a host that takes the report directly. Little-endian.
    static QByteArray report(const AutomaticBitrate::Decision& decision);
    static constexpr quint16 ReportType = 0x5581;

signals:
    void statusChanged(int targetKbps);
    // How a change the host confirmed landed: how long video stopped after it, and
    // whether it came back with a keyframe nobody asked for.
    void changeMeasured(int stallMs, bool keyframe);

protected:
    void run() override;

private:
    void onObservation(const void* frame);

    AutomaticBitrate::Controller m_Controller;
    const int m_InitialKbps;
    const bool m_HostStampsEveryFrame;
    Deliver m_Deliver;
    bool m_Stopping = false;
    QMutex m_Mutex;
    QWaitCondition m_Wake;
    double m_Settled = 0;
    int m_Seconds = 0;
    bool m_HitFloor = false;
    bool m_CutForLoss = false;
    bool m_Rebuilt = false;
    // 90 kHz clocks wrap every 13 hours.
    struct Unwrapped {
        bool started = false;
        uint32_t last = 0;
        uint64_t high = 0;
        uint64_t next(uint32_t value);
    };
    Unwrapped m_Rtp, m_Stamp;
};
