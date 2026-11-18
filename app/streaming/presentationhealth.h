#pragma once
#include <QtGlobal>
#include <optional>

// Owned by the streaming event loop. A first image is provisional; repeated
// presentation (legacy hosts) or explicit healthy capture including idle
// (Skylight) is required before replacing the recovery configuration.
class PresentationHealth {
public:
    quint32 observe(quint32 now, quint64 frames, quint32 lastFrame,
                    bool sourceKnown, bool sourceHealthy) {
        const bool continuous = !previousObservation || quint32(now-*previousObservation)<=2500;
        previousObservation=now;
        if (!continuous) since.reset();
        const bool healthy = frames >= 2 && quint32(now-lastFrame)<=2000 &&
            (!sourceKnown || sourceHealthy);
        if (!healthy) { since.reset(); return 0; }
        if (!since) since = now;
        return now - *since;
    }
private:
    std::optional<quint32> since, previousObservation;
};
