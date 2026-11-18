#include "runtimebitrate.h"
#include <QElapsedTimer>

RuntimeBitrate::RuntimeBitrate(NvComputer* computer, int ceiling, bool adaptive,
                             std::function<Observation()> observe, int audioChannels) :
    m_Computer(computer), m_InitialCeiling(qBound(500, ceiling, 500000)),
    m_Adaptive(adaptive), m_Observe(std::move(observe)), m_AudioChannels(audioChannels) { setObjectName("Runtime bitrate"); }
RuntimeBitrate::~RuntimeBitrate() { stopAndWait(); }
void RuntimeBitrate::requestBitrate(int bitrate) {
    QMutexLocker lock(&m_Mutex);
    m_Pending = qBound(500, bitrate, 500000);
    m_Wake.wakeAll();
}
void RuntimeBitrate::stopAndWait() {
    m_Cancel.cancel();
    { QMutexLocker lock(&m_Mutex); m_Wake.wakeAll(); }
    if (isRunning()) wait();
}
void RuntimeBitrate::run() {
    NvHTTP http(m_Computer);
    http.setCancelToken(&m_Cancel);
    int current = m_InitialCeiling, ceiling = current;
    try {
        const auto caps = http.openConnectionRaw(http.m_BaseUrlHttps, "api/abr/capabilities", "", 2000,
            {}, NvHTTP::NVLL_ERROR, &m_Cancel, 16384);
        if (caps.httpStatusCode != 200 || !RuntimeBitratePolicy::supported(caps.body)) {
            if (!m_Cancel.isCancelled()) emit changed(false, current, tr("This host does not support live bitrate changes."));
            return;
        }
    } catch (...) {
        if (!m_Cancel.isCancelled()) emit changed(false, current, tr("Live quality control is unavailable; the initial stream quality is still in use."));
        return;
    }
    emit changed(true, current, {});
    QElapsedTimer clock; clock.start();
    qint64 lastChange = 0, retryAfter = 0;
    int failures = 0;
    while (!m_Cancel.isCancelled()) {
        int manual = 0;
        { QMutexLocker lock(&m_Mutex); manual = m_Pending; m_Pending = 0; }
        const auto now = clock.elapsed();
        auto observation = m_Observe();
        if (manual && (now < retryAfter || !observation.freshFrames)) {
            QMutexLocker lock(&m_Mutex);
            if (!m_Pending) m_Pending = manual;
            m_Wake.wait(&m_Mutex, observation.freshFrames ? 1000 : 250);
            continue;
        }
        if (manual) ceiling = manual;
        const int target = manual ? manual : m_Adaptive && now >= retryAfter ?
            RuntimeBitratePolicy::next(current, ceiling, observation.poor, observation.freshFrames, now-lastChange) : current;
        // The host's encoder doesn't compare: the same value again still restarts it.
        const int encoder = qMax(500, RuntimeBitratePolicy::encoderKbps(target, m_AudioChannels));
        if (encoder != RuntimeBitratePolicy::encoderKbps(current, m_AudioChannels) && now >= retryAfter
                && observation.freshFrames) {
            try {
                const auto response = http.openConnectionRaw(http.m_BaseUrlHttps, "bitrate",
                    "bitrate=" + QString::number(encoder), 2000, {}, NvHTTP::NVLL_ERROR, &m_Cancel, 16384);
                const int confirmed = response.httpStatusCode == 200 ? RuntimeBitratePolicy::applied(response.body) : 0;
                if (!confirmed) throw std::runtime_error("bitrate rejected");
                current = RuntimeBitratePolicy::wireKbps(confirmed, m_AudioChannels);
                if (confirmed < encoder) ceiling = qMin(ceiling, current);
                lastChange = clock.elapsed(); failures = 0;
                emit applied(current);
                emit changed(true, current, {});
            } catch (...) {
                if (m_Cancel.isCancelled()) break;
                retryAfter = clock.elapsed() + 5000;
                if (++failures >= 3) {
                    emit changed(false, current, tr("The host stopped accepting live quality changes. Reconnect to try again."));
                    break;
                }
                emit changed(true, current, tr("The host did not confirm the quality change. The previous confirmed setting is shown."));
            }
        }
        QMutexLocker lock(&m_Mutex);
        if (!m_Cancel.isCancelled() && !m_Pending) m_Wake.wait(&m_Mutex, 1000);
    }
}
