#pragma once

#include <QSize>

class StreamingPreferences;

class PreferencesWorkflow
{
public:
    struct State
    {
        QSize resolution;
        int frameRate;
        int bitrateKbps;
        bool autoAdjustBitrate;
        bool enableYuv444;
        bool unlockBitrate = false;
    };

    enum class Status
    {
        Applied,
        Unchanged,
        InvalidInput,
    };

    struct Transaction
    {
        Status status;
        State state;

        bool saveRequired() const;
    };

    static Transaction changeResolution(const State& current, const QSize& resolution);
    static Transaction changeFrameRate(const State& current, int frameRate);
    static bool applyResolution(StreamingPreferences& preferences,
                                const QSize& resolution);
    static bool applyFrameRate(StreamingPreferences& preferences, int frameRate);
    static int effectiveBitrate(int bitrateKbps, bool unlocked);
    static int defaultBitrate(int width, int height, int frameRate, bool yuv444);

private:
    static Transaction rejected(Status status, const State& current);
    static void adjustBitrate(State& state);
    static State stateOf(const StreamingPreferences& preferences);
    static bool commit(StreamingPreferences& preferences,
                       const Transaction& transaction);
};
