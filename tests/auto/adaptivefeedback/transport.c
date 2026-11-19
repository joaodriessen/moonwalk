#include "transport.h"
// Preserve actual queueing, serialization and AES-GCM. Only the ENet socket
// boundary is inert: capture/free its owned packets rather than send them.
#define enet_peer_send AdaptiveCapturePeerSend
#define enet_host_service AdaptiveCaptureHostService
#include "../../../moonlight-common-c/AdaptiveControlStream.c"
#undef enet_peer_send
#undef enet_host_service
#include "../../../moonlight-common-c/adaptivefeedback.h"
#include <rs.h>

int AppVersionQuad[4];
STREAM_CONFIGURATION StreamConfig;
CONNECTION_LISTENER_CALLBACKS ListenerCallbacks;
DECODER_RENDERER_CALLBACKS VideoCallbacks;
int NegotiatedVideoFormat;
uint32_t SunshineFeatureFlags;
bool AudioEncryptionEnabled;
uint32_t EncryptionFeaturesEnabled;
uint32_t EncryptionFeaturesSupported;
uint32_t EncryptionFeaturesRequested;
bool ReferenceFrameInvalidationSupported;
atomic_bool ConnectionInterrupted;
struct sockaddr_storage RemoteAddr, LocalAddr;
SOCKADDR_LEN AddrLen;
uint16_t ControlPortNumber;
uint32_t ControlConnectData;

static RTP_VIDEO_QUEUE receiver;
static ENetPeer inertPeer;
static uint64_t clockUs;
static unsigned sequence, submitted;
static unsigned wireCount;
static unsigned char wire[8][256];
static unsigned wireLengths[8];

uint64_t AdaptiveTestClock(void) { return clockUs; }
void AdaptiveTestTime(uint64_t time) { clockUs = time; }
void queueRtpPacket(PRTPV_QUEUE_ENTRY entry) { ++submitted; free(entry->packet); }
void notifyFrameLost(uint32_t frame, bool speculative) { (void)frame; (void)speculative; }

int AdaptiveCapturePeerSend(ENetPeer* target, enet_uint8 channel, ENetPacket* packet) {
    (void)target; (void)channel;
    if (wireCount >= 8 || packet->dataLength > sizeof(wire[0])) return -1;
    memcpy(wire[wireCount], packet->data, packet->dataLength);
    wireLengths[wireCount++] = (unsigned)packet->dataLength;
    enet_packet_destroy(packet);
    return 0;
}
int AdaptiveCaptureHostService(ENetHost* host, ENetEvent* event, enet_uint32 timeout) {
    (void)host; (void)event; (void)timeout;
    return 0;
}

void AdaptiveTestBegin(bool enabled, bool sunshine) {
    memset(&StreamConfig, 0, sizeof(StreamConfig));
    StreamConfig.packetSize = 64;
    for (unsigned i = 0; i < sizeof(StreamConfig.remoteInputAesKey); ++i)
        StreamConfig.remoteInputAesKey[i] = (char)i;
    AppVersionQuad[0] = 7; AppVersionQuad[1] = 1; AppVersionQuad[2] = 431;
    AppVersionQuad[3] = sunshine ? -1 : 0;
    EncryptionFeaturesEnabled = SS_ENC_CONTROL_V2;
    memset(&inertPeer, 0, sizeof(inertPeer));
    inertPeer.channelCount = CTRL_CHANNEL_COUNT;
    inertPeer.state = ENET_PEER_STATE_CONNECTED;
    peer = &inertPeer;
    sequence = 1; submitted = 0; clockUs = 1000000; wireCount = 0;
    initializeControlStream();
    StarlightSetAdaptiveFeedbackEnabled(enabled);
    RtpvInitializeQueue(&receiver);
}
void AdaptiveTestEnd(void) {
    RtpvCleanupQueue(&receiver);
    stopping = true;
    destroyControlStream();
    peer = NULL;
}
void AdaptiveTestRestartQueueWithoutNegotiation(void) {
    RtpvCleanupQueue(&receiver);
    RtpvInitializeQueue(&receiver);
    sequence = 1;
}
unsigned AdaptiveTestSubmitted(void) { return submitted; }
unsigned AdaptiveTestNextSequence(void) { return sequence; }

static unsigned char* packetFor(uint32_t frame, unsigned block, unsigned blocks,
                                unsigned data, unsigned fec, unsigned index, unsigned seq) {
    const int size = StreamConfig.packetSize + MAX_RTP_HEADER_SIZE;
    unsigned char* bytes = calloc(1, size + sizeof(RTPV_QUEUE_ENTRY));
    PRTP_PACKET packet = (PRTP_PACKET)bytes;
    packet->header = FLAG_EXTENSION;
    packet->sequenceNumber = (uint16_t)seq;
    packet->timestamp = 90000 + frame * 1500;
    PNV_VIDEO_PACKET video = (PNV_VIDEO_PACKET)(bytes + MAX_RTP_HEADER_SIZE);
    video->frameIndex = LE32(frame);
    video->streamPacketIndex = LE32(seq << 8);
    video->multiFecFlags = 0x10;
    video->multiFecBlocks = (uint8_t)((block << 4) | ((blocks - 1) << 6));
    video->fecInfo = LE32((data << 22) | (index << 12) | (fec << 4));
    video->flags = FLAG_CONTAINS_PIC_DATA | (index == 0 ? FLAG_SOF : 0) | (index == data - 1 ? FLAG_EOF : 0);
    return bytes;
}
static int feed(unsigned char* bytes) {
    const int size = StreamConfig.packetSize + MAX_RTP_HEADER_SIZE;
    int result = RtpvAddPacket(&receiver, (PRTP_PACKET)bytes, size, (PRTPV_QUEUE_ENTRY)(bytes + size));
    if (result != RTPF_RET_QUEUED) free(bytes);
    return result;
}
int AdaptiveTestPacket(uint32_t frame, unsigned block, unsigned blocks, unsigned data,
                       unsigned fec, unsigned index, unsigned seq) {
    return feed(packetFor(frame, block, blocks, data, fec, index, seq));
}
int AdaptiveTestBlock(uint32_t frame, unsigned block, unsigned blocks, unsigned data,
                      unsigned fec, unsigned receivedMask) {
    unsigned parity = (data * fec + 99) / 100, total = data + parity;
    if (total > 8 || !data || !blocks) return -10;
    unsigned char* shards[8] = {0};
    for (unsigned i = 0; i < total; ++i)
        shards[i] = packetFor(frame, block, blocks, data, fec, i, sequence + i);
    if (parity) {
        reed_solomon* rs = reed_solomon_new(data, parity);
        if (!rs) return -11;
        reed_solomon_encode(rs, shards, total, StreamConfig.packetSize + MAX_RTP_HEADER_SIZE);
        reed_solomon_release(rs);
        // The transport headers of parity shards carry routing/FEC metadata;
        // the encoded flags/picture bytes remain the actual parity values.
        for (unsigned i = data; i < total; ++i) {
            unsigned char* header = packetFor(frame, block, blocks, data, fec, i, sequence + i);
            unsigned char flags = shards[i][MAX_RTP_HEADER_SIZE + offsetof(NV_VIDEO_PACKET, flags)];
            memcpy(shards[i], header, MAX_RTP_HEADER_SIZE + sizeof(NV_VIDEO_PACKET));
            shards[i][MAX_RTP_HEADER_SIZE + offsetof(NV_VIDEO_PACKET, flags)] = flags;
            free(header);
        }
    }
    int result = 0;
    for (unsigned i = 0; i < total; ++i) {
        if (receivedMask & (1u << i)) result = feed(shards[i]);
        else free(shards[i]);
    }
    sequence += total;
    return result;
}
int AdaptiveTestDrain(void) {
    wireCount = 0;
    PQUEUED_FRAME_FEC_STATUS status;
    while (LbqPollQueueElement(&frameFecStatusQueue, (void**)&status) == LBQ_SUCCESS) {
        bool sent = sendMessageEnet(SS_FRAME_FEC_PTYPE, sizeof(status->fecStatus), &status->fecStatus,
                                   CTRL_CHANNEL_GENERIC, ENET_PACKET_FLAG_UNSEQUENCED, true);
        free(status);
        if (!sent) return -1;
    }
    return (int)wireCount;
}
int AdaptiveTestWire(unsigned index, unsigned char* output, int capacity) {
    if (index >= wireCount || capacity < (int)wireLengths[index]) return -1;
    memcpy(output, wire[index], wireLengths[index]);
    return (int)wireLengths[index];
}
int AdaptiveTestDecrypt(unsigned index, unsigned char* output, int capacity) {
    if (index >= wireCount) return -1;
    const PNVCTL_ENCRYPTED_PACKET_HEADER encrypted = (PNVCTL_ENCRYPTED_PACKET_HEADER)wire[index];
    unsigned char iv[12] = {0};
    memcpy(iv, &encrypted->seq, sizeof(encrypted->seq));
    iv[10] = 'C'; iv[11] = 'C';
    int size = (int)wireLengths[index] - sizeof(*encrypted) - AES_GCM_TAG_LENGTH;
    if (size > capacity || size < 0) return -1;
    return PltDecryptMessage(decryptionCtx, ALGORITHM_AES_GCM, 0,
                            CTRL_CHANNEL_GENERIC, LE32(encrypted->seq),
                            (unsigned char*)StreamConfig.remoteInputAesKey, sizeof(StreamConfig.remoteInputAesKey),
                            iv, sizeof(iv), (unsigned char*)(encrypted + 1), AES_GCM_TAG_LENGTH,
                            (unsigned char*)(encrypted + 1) + AES_GCM_TAG_LENGTH, size, output, &size) ? size : -1;
}

int AdaptiveTestEncryption(bool audio, bool video) {
    AudioEncryptionEnabled = audio;
    EncryptionFeaturesEnabled = SS_ENC_CONTROL_V2 | (video ? SS_ENC_VIDEO : 0);
    return StarlightGetEncryptionFlags();
}
