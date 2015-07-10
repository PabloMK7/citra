// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <vector>

#include "common/assert.h"
#include "common/profiler.h"
#include "common/profiler_reporting.h"
#include "common/synchronized_wrapper.h"

#if defined(_MSC_VER) && _MSC_VER <= 1800 // MSVC 2013.
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h> // For QueryPerformanceCounter/Frequency
#endif

namespace Common {
namespace Profiling {

#if ENABLE_PROFILING
thread_local Timer* Timer::current_timer = nullptr;
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1800 // MSVC 2013
QPCClock::time_point QPCClock::now() {
    static LARGE_INTEGER freq;
    // Use this dummy local static to ensure this gets initialized once.
    static BOOL dummy = QueryPerformanceFrequency(&freq);

    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    // This is prone to overflow when multiplying, which is why I'm using micro instead of nano. The
    // correct way to approach this would be to just return ticks as a time_point and then subtract
    // and do this conversion when creating a duration from two time_points, however, as far as I
    // could tell the C++ requirements for these types are incompatible with this approach.
    return time_point(duration(ticks.QuadPart * std::micro::den / freq.QuadPart));
}
#endif

TimingCategory::TimingCategory(const char* name, TimingCategory* parent)
        : accumulated_duration(0) {

    ProfilingManager& manager = GetProfilingManager();
    category_id = manager.RegisterTimingCategory(this, name);
    if (parent != nullptr)
        manager.SetTimingCategoryParent(category_id, parent->category_id);
}

ProfilingManager::ProfilingManager()
        : last_frame_end(Clock::now()), this_frame_start(Clock::now()) {
}

unsigned int ProfilingManager::RegisterTimingCategory(TimingCategory* category, const char* name) {
    TimingCategoryInfo info;
    info.category = category;
    info.name = name;
    info.parent = TimingCategoryInfo::NO_PARENT;

    unsigned int id = (unsigned int)timing_categories.size();
    timing_categories.push_back(std::move(info));

    return id;
}

void ProfilingManager::SetTimingCategoryParent(unsigned int category, unsigned int parent) {
    ASSERT(category < timing_categories.size());
    ASSERT(parent < timing_categories.size());

    timing_categories[category].parent = parent;
}

void ProfilingManager::BeginFrame() {
    this_frame_start = Clock::now();
}

void ProfilingManager::FinishFrame() {
    Clock::time_point now = Clock::now();

    results.interframe_time = now - last_frame_end;
    results.frame_time = now - this_frame_start;

    results.time_per_category.resize(timing_categories.size());
    for (size_t i = 0; i < timing_categories.size(); ++i) {
        results.time_per_category[i] = timing_categories[i].category->GetAccumulatedTime();
    }

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

void TimingResultsAggregator::SetNumberOfCategories(size_t n) {
    size_t old_size = times_per_category.size();
    if (n == old_size)
        return;

    times_per_category.resize(n);

    for (size_t i = old_size; i < n; ++i) {
        times_per_category[i].resize(max_window_size, Duration::zero());
    }
}

void TimingResultsAggregator::AddFrame(const ProfilingFrameResult& frame_result) {
    SetNumberOfCategories(frame_result.time_per_category.size());

    interframe_times[cursor] = frame_result.interframe_time;
    frame_times[cursor] = frame_result.frame_time;
    for (size_t i = 0; i < frame_result.time_per_category.size(); ++i) {
        times_per_category[i][cursor] = frame_result.time_per_category[i];
    }

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

    result.time_per_category.resize(times_per_category.size());
    for (size_t i = 0; i < times_per_category.size(); ++i) {
        result.time_per_category[i] = AggregateField(times_per_category[i], window_size);
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
