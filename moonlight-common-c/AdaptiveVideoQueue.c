// Keep the upstream submodule pinned and unmodified. This wrapper observes the
// pinned queue's completed blocks; all packet acceptance/reassembly stays upstream.
// Pinned RtpVideoQueue.c SHA256:
// bd61187bdacf049b954638d2de79408a6a41dfd9dc7bbedf0fcc2664e3a0fc4b
#include "adaptivefeedback.h"

#define RtpvInitializeQueue StarlightOriginalRtpvInitializeQueue
#define RtpvCleanupQueue StarlightOriginalRtpvCleanupQueue
#define RtpvAddPacket StarlightOriginalRtpvAddPacket
#define connectionSendFrameFecStatus StarlightForwardFrameFecStatus
#include "moonlight-common-c/src/RtpVideoQueue.c"
#undef RtpvInitializeQueue
#undef RtpvCleanupQueue
#undef RtpvAddPacket
#undef connectionSendFrameFecStatus

void connectionSendFrameFecStatus(PSS_FRAME_FEC_STATUS status);

// common-c owns one active connection/receive queue. These observations are
// receive-thread owned; the enabled flag is changed only before start/after join.
static bool adaptiveFeedbackEnabled;
static bool observedFrame;
static uint32_t observedFrameIndex;
static bool frameHadLoss;
static bool sentHealthyReport;
static uint64_t lastHealthyReportUs;
static bool sendingHealthyObservation;

static StarlightFrameObserver frameObserver;
static void* frameObserverContext;
static bool arrivalStarted;
static uint32_t arrivalFrameIndex;
static StarlightFrameObservation arrival;
static uint16_t arrivalBlockData[4];
static uint16_t arrivalBlockSeen[4];
static uint32_t arrivalCarriedLoss;
static uint32_t arrivalTypicalPackets;

static void finishArrival(void) {
    uint32_t lost = arrivalCarriedLoss;
    for (int i = 0; i < 4; i++) {
        if (arrivalBlockData[i] > arrivalBlockSeen[i]) lost += arrivalBlockData[i] - arrivalBlockSeen[i];
    }
    arrival.lostPackets = lost;
    arrivalCarriedLoss = 0;
    arrivalTypicalPackets = arrivalTypicalPackets ? (arrivalTypicalPackets * 15 + arrival.dataPackets) / 16 : arrival.dataPackets;
    frameObserver(&arrival, frameObserverContext);
}

// Runs before submission, since that can transfer or free the packet. The queue may
// never see a frame's trailing FEC when encryption is on, so loss is judged from
// video data alone: what each FEC block said it held against what came.
static void observeArrival(const RTP_PACKET* packet, int length) {
    const int offset = sizeof(*packet) + ((packet->header & FLAG_EXTENSION) ? 4 : 0);
    NV_VIDEO_PACKET metadata;
    if (length < offset + (int)sizeof(metadata)) return;
    memcpy(&metadata, (const char*)packet + offset, sizeof(metadata));
    const uint32_t frame = LE32(metadata.frameIndex);
    const uint32_t fecInfo = LE32(metadata.fecInfo);
    const uint64_t now = PltGetMicroseconds();

    if (!arrivalStarted || (int32_t)(frame - arrivalFrameIndex) > 0) {
        if (arrivalStarted) {
            finishArrival();
            const uint32_t skipped = frame - arrivalFrameIndex - 1;
            if (skipped < 600) arrivalCarriedLoss = skipped * (arrivalTypicalPackets ? arrivalTypicalPackets : 1);
        }
        arrivalStarted = true;
        arrivalFrameIndex = frame;
        memset(&arrival, 0, sizeof(arrival));
        memset(arrivalBlockData, 0, sizeof(arrivalBlockData));
        memset(arrivalBlockSeen, 0, sizeof(arrivalBlockSeen));
        arrival.firstArrivalUs = now;
        arrival.rtpTimestamp = packet->timestamp;
        arrival.sendStamp = packet->ssrc;
    }
    else if (frame != arrivalFrameIndex) return;

    arrival.lastArrivalUs = now;
    const unsigned block = (metadata.multiFecBlocks >> 4) & 3;
    const unsigned fecIndex = (fecInfo & 0x3FF000) >> 12;
    const unsigned dataShards = (fecInfo & 0xFFC00000) >> 22;
    arrivalBlockData[block] = (uint16_t)dataShards;
    if (fecIndex < dataShards) {
        arrivalBlockSeen[block]++;
        arrival.dataPackets++;
        arrival.payloadBytes += (uint32_t)(length - offset - (int)sizeof(metadata));
        if ((metadata.flags & FLAG_SOF) && block == 0 && fecIndex == 0 &&
                length >= offset + (int)sizeof(metadata) + 3) {
            const unsigned char* header = (const unsigned char*)packet + offset + sizeof(metadata);
            arrival.hostLatency = (uint16_t)(header[1] | (header[2] << 8));
            // Frame type 2 is an IDR, as upstream's depacketizer reads it.
            arrival.keyframe = length >= offset + (int)sizeof(metadata) + 4 && header[3] == 2;
        }
    }
}

void StarlightSetFrameObserver(StarlightFrameObserver observer, void* context) {
    frameObserver = observer;
    frameObserverContext = context;
    arrivalStarted = false;
    arrivalCarriedLoss = 0;
    arrivalTypicalPackets = 0;
}

static void resetAdaptiveObservations(void) {
    observedFrame = false;
    frameHadLoss = false;
    sentHealthyReport = false;
    lastHealthyReportUs = 0;
    sendingHealthyObservation = false;
}

void StarlightForwardFrameFecStatus(PSS_FRAME_FEC_STATUS status) {
    // The pinned queue's missing-block path can report the previous completed
    // block's counters under the next block index. Those full counters are not
    // evidence that this frame arrived. In adaptive mode only our witnessed
    // complete-frame path may emit a clean status. Preserve all original loss
    // reports, and preserve every original byte when adaptation is disabled.
    if (adaptiveFeedbackEnabled && !sendingHealthyObservation &&
        status->receivedDataPackets == status->totalDataPackets &&
        BE16(status->missingPacketsBeforeHighestReceived) == 0) return;
    connectionSendFrameFecStatus(status);
}

void StarlightSetAdaptiveFeedbackEnabled(bool enabled) {
    adaptiveFeedbackEnabled = enabled;
    resetAdaptiveObservations();
}

void RtpvInitializeQueue(PRTP_VIDEO_QUEUE queue) {
    resetAdaptiveObservations();
    StarlightOriginalRtpvInitializeQueue(queue);
}

void RtpvCleanupQueue(PRTP_VIDEO_QUEUE queue) {
    StarlightOriginalRtpvCleanupQueue(queue);
    adaptiveFeedbackEnabled = false;
    StarlightSetFrameObserver(NULL, NULL);
    resetAdaptiveObservations();
}

int RtpvAddPacket(PRTP_VIDEO_QUEUE queue, PRTP_PACKET packet, int length, PRTPV_QUEUE_ENTRY entry) {
    if (frameObserver != NULL && IS_SUNSHINE()) observeArrival(packet, length);
    if (!adaptiveFeedbackEnabled || !IS_SUNSHINE())
        return StarlightOriginalRtpvAddPacket(queue, packet, length, entry);
    // Submission may transfer/free the packet. Preserve only its bounded metadata
    // before calling upstream, never read packet/entry after that ownership change.
    NV_VIDEO_PACKET metadata;
    const int offset = sizeof(*packet) + ((packet->header & FLAG_EXTENSION) ? 4 : 0);
    const bool observe = length >= offset + (int)sizeof(metadata);
    if (observe) memcpy(&metadata, (const char*)packet + offset, sizeof(metadata));

    const int result = StarlightOriginalRtpvAddPacket(queue, packet, length, entry);
    if (!observe || result != RTPF_RET_QUEUED) return result;

    const uint32_t frame = LE32(metadata.frameIndex);
    const unsigned block = queue->multiFecCapable ? (metadata.multiFecBlocks >> 4) & 3 : 0;
    if (!observedFrame || observedFrameIndex != frame) {
        observedFrame = true;
        observedFrameIndex = frame;
        frameHadLoss = false;
    }

    const bool completeFrame = queue->currentFrameNumber == frame + 1;
    const bool completeBlock = queue->pendingFecBlockList.count == 0 &&
                               (completeFrame || (queue->currentFrameNumber == frame &&
                                                  queue->multiFecCurrentBlockNumber == block + 1));
    if (!completeBlock || !queue->bufferDataPackets) return result;
    if (queue->receivedDataPackets != queue->bufferDataPackets || queue->missingPackets)
        frameHadLoss = true;
    if (!completeFrame || frameHadLoss) return result;

    // At most four fresh, fully received frame observations per second. Existing
    // loss reports retain their upstream path and queue capacity. Silence,
    // partial frames, FEC recovery and duplicate/rejected packets never count.
    const uint64_t now = PltGetMicroseconds();
    if (sentHealthyReport && now - lastHealthyReportUs < 250000) return result;
    RTP_VIDEO_QUEUE snapshot = *queue;
    snapshot.currentFrameNumber = frame;
    snapshot.multiFecCurrentBlockNumber = (uint8_t)block;
    sendingHealthyObservation = true;
    reportFinalFrameFecStatus(&snapshot);
    sendingHealthyObservation = false;
    sentHealthyReport = true;
    lastHealthyReportUs = now;
    return result;
}
