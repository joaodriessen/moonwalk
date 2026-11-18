#pragma once
#include <Limelight.h>

namespace SessionRetryPolicy {
inline bool retryTermination(int error, bool established) {
    if (!established) return false;
    switch (error) {
    case ML_ERROR_GRACEFUL_TERMINATION:
    case ML_ERROR_PROTECTED_CONTENT:
    case ML_ERROR_FRAME_CONVERSION:
    case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
        return false;
    default:
        return error == -1 || error == ML_ERROR_NO_VIDEO_TRAFFIC ||
               error == ML_ERROR_NO_VIDEO_FRAME || error > 0;
    }
}
inline bool retryStage(int stage) {
    return stage == STAGE_NAME_RESOLUTION || stage == STAGE_RTSP_HANDSHAKE ||
           stage == STAGE_CONTROL_STREAM_INIT || stage == STAGE_CONTROL_STREAM_START ||
           stage == STAGE_VIDEO_STREAM_START || stage == STAGE_AUDIO_STREAM_START ||
           stage == STAGE_INPUT_STREAM_START;
}
inline bool sameTarget(int current, int expected) { return expected > 0 && current == expected; }
}
