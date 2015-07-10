// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "common/profiler.h"
#include "common/synchronized_wrapper.h"

namespace Common {
namespace Profiling {

struct TimingCategoryInfo {
    static const unsigned int NO_PARENT = -1;

    TimingCategory* category;
    const char* name;
    unsigned int parent;
};

struct ProfilingFrameResult {
    /// Time since the last delivered frame
    Duration interframe_time;

    /// Time spent processing a frame, excluding VSync
    Duration frame_time;

    /// Total amount of time spent inside each category in this frame. Indexed by the category id
    std::vector<Duration> time_per_category;
};

class ProfilingManager final {
public:
    ProfilingManager();

    unsigned int RegisterTimingCategory(TimingCategory* category, const char* name);
    void SetTimingCategoryParent(unsigned int category, unsigned int parent);

    const std::vector<TimingCategoryInfo>& GetTimingCategoriesInfo() const {
        return timing_categories;
    }

    /// This should be called after swapping screen buffers.
    void BeginFrame();
    /// This should be called before swapping screen buffers.
    void FinishFrame();

    /// Get the timing results from the previous frame. This is updated when you call FinishFrame().
    const ProfilingFrameResult& GetPreviousFrameResults() const {
        return results;
    }

private:
    std::vector<TimingCategoryInfo> timing_categories;
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

    /// Total amount of time spent inside each category in this frame. Indexed by the category id
    std::vector<AggregatedDuration> time_per_category;
};

class TimingResultsAggregator final {
public:
    TimingResultsAggregator(size_t window_size);

    void Clear();
    void SetNumberOfCategories(size_t n);

    void AddFrame(const ProfilingFrameResult& frame_result);

    AggregatedFrameResult GetAggregatedResults() const;

    size_t max_window_size;
    size_t window_size;
    size_t cursor;

    std::vector<Duration> interframe_times;
    std::vector<Duration> frame_times;
    std::vector<std::vector<Duration>> times_per_category;
};

ProfilingManager& GetProfilingManager();
SynchronizedRef<TimingResultsAggregator> GetTimingResultsAggregator();

} // namespace Profiling
} // namespace Common
