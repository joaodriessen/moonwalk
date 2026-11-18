#include "preferencesworkflow.h"

#include <QtMath>

bool PreferencesWorkflow::Transaction::saveRequired() const
{
    return status == Status::Applied;
}

PreferencesWorkflow::Transaction PreferencesWorkflow::changeResolution(
    const State& current,
    const QSize& resolution)
{
    if (resolution.width() <= 0 || resolution.height() <= 0) {
        return rejected(Status::InvalidInput, current);
    }
    if (resolution == current.resolution) {
        return rejected(Status::Unchanged, current);
    }

    State updated = current;
    updated.resolution = resolution;
    adjustBitrate(updated);
    return { Status::Applied, updated };
}

PreferencesWorkflow::Transaction PreferencesWorkflow::changeFrameRate(
    const State& current,
    int frameRate)
{
    if (frameRate <= 0) {
        return rejected(Status::InvalidInput, current);
    }
    if (frameRate == current.frameRate) {
        return rejected(Status::Unchanged, current);
    }

    State updated = current;
    updated.frameRate = frameRate;
    adjustBitrate(updated);
    return { Status::Applied, updated };
}

int PreferencesWorkflow::effectiveBitrate(int bitrateKbps, bool unlocked)
{
    return qBound(500, bitrateKbps, unlocked ? 500000 : 150000);
}

int PreferencesWorkflow::defaultBitrate(int width, int height, int frameRate, bool yuv444)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor =
        (frameRate <= 60 ? frameRate : (qSqrt(frameRate / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct ResolutionEntry
    {
        int pixels;
        int factor;
    } resolutionTable[] {
        { 640 * 360, 1 },
        { 854 * 480, 2 },
        { 1280 * 720, 5 },
        { 1920 * 1080, 10 },
        { 2560 * 1440, 20 },
        { 3840 * 2160, 40 },
        { 5120 * 2880, 70 },
        { 6016 * 3384, 100 },
        { -1, -1 },
    };

    float resolutionFactor;
    const int pixels = width * height;
    for (int i = 0;; ++i) {
        if (pixels == resolutionTable[i].pixels) {
            resolutionFactor = resolutionTable[i].factor;
            break;
        }
        if (pixels < resolutionTable[i].pixels) {
            if (i == 0) {
                resolutionFactor = resolutionTable[i].factor;
            }
            else {
                resolutionFactor =
                    (static_cast<float>(pixels - resolutionTable[i - 1].pixels) /
                     (resolutionTable[i].pixels - resolutionTable[i - 1].pixels)) *
                        (resolutionTable[i].factor - resolutionTable[i - 1].factor) +
                    resolutionTable[i - 1].factor;
            }
            break;
        }
        if (resolutionTable[i].pixels == -1) {
            resolutionFactor = resolutionTable[i - 1].factor;
            break;
        }
    }

    if (yuv444) {
        resolutionFactor *= 2;
    }

    return qRound(resolutionFactor * frameRateFactor) * 1000;
}

PreferencesWorkflow::Transaction PreferencesWorkflow::rejected(
    Status status,
    const State& current)
{
    return { status, current };
}

void PreferencesWorkflow::adjustBitrate(State& state)
{
    if (!state.autoAdjustBitrate) {
        return;
    }

    state.bitrateKbps = effectiveBitrate(defaultBitrate(
        state.resolution.width(),
        state.resolution.height(),
        state.frameRate,
        state.enableYuv444), state.unlockBitrate);
}
