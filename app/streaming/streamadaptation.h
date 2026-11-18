#pragma once

#include "streamplan.h"

// An explicitly applied or next-connection trial, carried through network recovery.
// A live trial is not a claim about durable host capacity.
struct StreamAdaptation {
    StreamPlanning::Rung target {0, 0, 0};
    HostCapacity baseline;
    bool pending = false;
    bool checkTrial = false;
    bool restoring = false;
    bool locked = false;
    bool restored = false;
    bool recoveryAttempted = false;
    int transitions = 0;
    int qualityTrials = 0;
    int encoderCeilingKbps = 0;
    qint64 interruptedMs = 0;
    qint64 transitionAtMs = 0;
    static constexpr qint64 DisruptionBudgetMs = 30000;

    static bool same(StreamPlanning::Rung a, StreamPlanning::Rung b) {
        return a.width == b.width && a.height == b.height && a.fps == b.fps;
    }
    static StreamPlanning::Rung mode(const HostCapacity& sample) {
        return {sample.width, sample.height, sample.fps};
    }
    bool mayChange() const {
        return !locked && !pending && !checkTrial
            && transitions < StreamPlanning::MaximumAdjustments && interruptedMs < DisruptionBudgetMs;
    }
    bool request(StreamPlanning::Rung next, StreamPlanning::Rung current,
                 const HostCapacity& sample, qint64 nowMs) {
        if (!mayChange() || !sample.measured() || same(next,current)
                || next.width <= 0 || next.height <= 0 || next.fps != current.fps) return false;
        target = next;
        baseline = sample;
        pending = checkTrial = true;
        transitionAtMs = nowMs;
        ++transitions;
        return true;
    }
    bool presented(StreamPlanning::Rung actual, qint64 nowMs) {
        if (!pending) return true;
        interruptedMs += qMax<qint64>(0,nowMs-transitionAtMs);
        pending = false;
        if (restoring) {
            restoring = checkTrial = false;
            locked = true;
        }
        if (!same(actual,target)) {
            // The host did not apply the request. Repeating it cannot establish capacity.
            checkTrial = false;
            locked = true;
            return false;
        }
        return true;
    }
    bool restore(StreamPlanning::Rung current, qint64 nowMs) {
        locked = true;
        checkTrial = false;
        if (restored || !baseline.measured() || same(mode(baseline),current)) return false;
        // Reserve one undo even when the episode's normal budget is spent.
        restored = restoring = pending = true;
        target = mode(baseline);
        transitionAtMs = nowMs;
        return true;
    }
    bool mayTryQuality(HostCapacity::BitrateChange cost) const {
        const int allowance = cost == HostCapacity::BitrateChange::Live ? 2
            : cost == HostCapacity::BitrateChange::Unknown ? 1 : 0;
        return !pending && qualityTrials < allowance && interruptedMs < DisruptionBudgetMs;
    }
};
