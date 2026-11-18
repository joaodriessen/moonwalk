// Keep the upstream submodule pinned and unmodified. This wrapper adds one way to
// send a Starlight message on the control stream, using its own serializer and
// encryption, plus a negotiated protection snapshot; everything else stays upstream.
#include "adaptivefeedback.h"
#include "moonlight-common-c/src/ControlStream.c"

bool StarlightSendControlMessage(uint16_t type, const void* payload, uint16_t length) {
    if (peer == NULL || stopping || AppVersionQuad[0] < 5) return false;
    return sendMessageAndForget((short)type, (short)length, payload, CTRL_CHANNEL_GENERIC, ENET_PACKET_FLAG_RELIABLE, false);
}

int StarlightGetEncryptionFlags(void) {
    return (AudioEncryptionEnabled ? ENCFLG_AUDIO : 0) |
           ((EncryptionFeaturesEnabled & SS_ENC_VIDEO) ? ENCFLG_VIDEO : 0);
}
