// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <vector>
#include "common/assert.h"
#include "common/profiler_reporting.h"
#include "common/synchronized_wrapper.h"

namespace Common {
namespace Profiling {

ProfilingManager::ProfilingManager()
    : last_frame_end(Clock::now()), this_frame_start(Clock::now()) {}

void ProfilingManager::BeginFrame() {
    this_frame_start = Clock::now();
}

void ProfilingManager::FinishFrame() {
    Clock::time_point now = Clock::now();

    results.interframe_time = now - last_frame_end;
    results.frame_time = now - this_frame_start;

    last_frame_end = now;
}

TimingResultsAggregator::TimingResultsAggregator(size_t window_size)
    : max_window_size(window_size), window_size(0) {
    interframe_times.resize(window_size, Duration::zero());
    frame_times.resize(window_size, Duration::zero());
}

void TimingResultsAggregator::Clear() {
    window_size = cursor = 0;
}

void TimingResultsAggregator::AddFrame(const ProfilingFrameResult& frame_result) {
    interframe_times[cursor] = frame_result.interframe_time;
    frame_times[cursor] = frame_result.frame_time;

    ++cursor;
    if (cursor == max_window_size)
        cursor = 0;
    if (window_size < max_window_size)
        ++window_size;
}

static AggregatedDuration AggregateField(const std::vector<Duration>& v, size_t len) {
    AggregatedDuration result;
    result.avg = Duration::zero();
    result.min = result.max = (len == 0 ? Duration::zero() : v[0]);

    for (size_t i = 0; i < len; ++i) {
        Duration value = v[i];
        result.avg += value;
        result.min = std::min(result.min, value);
        result.max = std::max(result.max, value);
    }
    if (len != 0)
        result.avg /= len;

    return result;
}

static float tof(Common::Profiling::Duration dur) {
    using FloatMs = std::chrono::duration<float, std::chrono::milliseconds::period>;
    return std::chrono::duration_cast<FloatMs>(dur).count();
}

AggregatedFrameResult TimingResultsAggregator::GetAggregatedResults() const {
    AggregatedFrameResult result;

    result.interframe_time = AggregateField(interframe_times, window_size);
    result.frame_time = AggregateField(frame_times, window_size);

    if (result.interframe_time.avg != Duration::zero()) {
        result.fps = 1000.0f / tof(result.interframe_time.avg);
    } else {
        result.fps = 0.0f;
    }

    return result;
}

ProfilingManager& GetProfilingManager() {
    // Takes advantage of "magic" static initialization for race-free initialization.
    static ProfilingManager manager;
    return manager;
}

SynchronizedRef<TimingResultsAggregator> GetTimingResultsAggregator() {
    static SynchronizedWrapper<TimingResultsAggregator> aggregator(30);
    return SynchronizedRef<TimingResultsAggregator>(aggregator);
}

} // namespace Profiling
} // namespace Common
