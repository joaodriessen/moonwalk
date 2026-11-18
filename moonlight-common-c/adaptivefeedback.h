#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set only while the common-c connection is stopped, immediately before starting
// a negotiated adaptive session. Queue cleanup disables feedback for the next
// connection. Existing loss/FEC reporting is unaffected.
void StarlightSetAdaptiveFeedbackEnabled(bool enabled);

// How one frame arrived, reported from the receive thread when the next begins.
typedef struct {
    uint64_t firstArrivalUs;
    uint64_t lastArrivalUs;
    uint32_t rtpTimestamp;
    uint32_t sendStamp;         // When the first packet left, at 90 kHz, from a host that says.
    uint16_t hostLatency;       // In 100us units. 0 when the host gave none for this frame.
    uint32_t payloadBytes;      // Video data received, not FEC.
    uint32_t dataPackets;
    uint32_t lostPackets;       // Video data that never came, including whole lost frames.
    bool keyframe;              // The host's frame header says IDR: a new sequence starts here.
} StarlightFrameObservation;

typedef void (*StarlightFrameObserver)(const StarlightFrameObservation* frame, void* context);

// Set only while the common-c connection is stopped. Queue cleanup clears it.
void StarlightSetFrameObserver(StarlightFrameObserver observer, void* context);

// Negotiated media protection for the active connection, using ENCFLG_* bits.
// Read after connectionStarted and before stopping the connection.
int StarlightGetEncryptionFlags(void);

// Sends on the control stream. Only while the connection is running.
bool StarlightSendControlMessage(uint16_t type, const void* payload, uint16_t length);

#ifdef __cplusplus
}
#endif
