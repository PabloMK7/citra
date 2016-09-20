// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <cstddef>
#include <vector>
#include "common/synchronized_wrapper.h"

namespace Common {
namespace Profiling {

using Clock = std::chrono::high_resolution_clock;
using Duration = Clock::duration;

struct ProfilingFrameResult {
    /// Time since the last delivered frame
    Duration interframe_time;

    /// Time spent processing a frame, excluding VSync
    Duration frame_time;
};

class ProfilingManager final {
public:
    ProfilingManager();

    /// This should be called after swapping screen buffers.
    void BeginFrame();
    /// This should be called before swapping screen buffers.
    void FinishFrame();

    /// Get the timing results from the previous frame. This is updated when you call FinishFrame().
    const ProfilingFrameResult& GetPreviousFrameResults() const {
        return results;
    }

private:
    Clock::time_point last_frame_end;
    Clock::time_point this_frame_start;

    ProfilingFrameResult results;
};

struct AggregatedDuration {
    Duration avg, min, max;
};

struct AggregatedFrameResult {
    /// Time since the last delivered frame
    AggregatedDuration interframe_time;

    /// Time spent processing a frame, excluding VSync
    AggregatedDuration frame_time;

    float fps;
};

class TimingResultsAggregator final {
public:
    TimingResultsAggregator(size_t window_size);

    void Clear();

    void AddFrame(const ProfilingFrameResult& frame_result);

    AggregatedFrameResult GetAggregatedResults() const;

    size_t max_window_size;
    size_t window_size;
    size_t cursor;

    std::vector<Duration> interframe_times;
    std::vector<Duration> frame_times;
};

ProfilingManager& GetProfilingManager();
SynchronizedRef<TimingResultsAggregator> GetTimingResultsAggregator();

} // namespace Profiling
} // namespace Common
