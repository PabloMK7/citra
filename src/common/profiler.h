// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <chrono>

#include "common/assert.h"
#include "common/thread.h"

namespace Common {
namespace Profiling {

// If this is defined to 0, it turns all Timers into no-ops.
#ifndef ENABLE_PROFILING
#define ENABLE_PROFILING 1
#endif

#if defined(_MSC_VER) && _MSC_VER <= 1800 // MSVC 2013
// MSVC up to 2013 doesn't use QueryPerformanceCounter for high_resolution_clock, so it has bad
// precision. We manually implement a clock based on QPC to get good results.

struct QPCClock {
    using duration = std::chrono::microseconds;
    using time_point = std::chrono::time_point<QPCClock>;
    using rep = duration::rep;
    using period = duration::period;
    static const bool is_steady = false;

    static time_point now();
};

using Clock = QPCClock;
#else
using Clock = std::chrono::high_resolution_clock;
#endif

using Duration = Clock::duration;

/**
 * Represents a timing category that measured time can be accounted towards. Should be declared as a
 * global variable and passed to Timers.
 */
class TimingCategory final {
public:
    TimingCategory(const char* name, TimingCategory* parent = nullptr);

    unsigned int GetCategoryId() const {
        return category_id;
    }

    /// Adds some time to this category. Can safely be called from multiple threads at the same time.
    void AddTime(Duration amount) {
        std::atomic_fetch_add_explicit(
                &accumulated_duration, amount.count(),
                std::memory_order_relaxed);
    }

    /**
     * Atomically retrieves the accumulated measured time for this category and resets the counter
     * to zero. Can be safely called concurrently with AddTime.
     */
    Duration GetAccumulatedTime() {
        return Duration(std::atomic_exchange_explicit(
                &accumulated_duration, (Duration::rep)0,
                std::memory_order_relaxed));
    }

private:
    unsigned int category_id;
    std::atomic<Duration::rep> accumulated_duration;
};

/**
 * Measures time elapsed between a call to Start and a call to Stop and attributes it to the given
 * TimingCategory. Start/Stop can be called multiple times on the same timer, but each call must be
 * appropriately paired.
 *
 * When a Timer is started, it automatically pauses a previously running timer on the same thread,
 * which is resumed when it is stopped. As such, no special action needs to be taken to avoid
 * double-accounting of time on two categories.
 */
class Timer {
public:
    Timer(TimingCategory& category) : category(category) {
    }

    void Start() {
#if ENABLE_PROFILING
        ASSERT(!running);
        previous_timer = current_timer;
        current_timer = this;
        if (previous_timer != nullptr)
            previous_timer->StopTiming();

        StartTiming();
#endif
    }

    void Stop() {
#if ENABLE_PROFILING
        ASSERT(running);
        StopTiming();

        if (previous_timer != nullptr)
            previous_timer->StartTiming();
        current_timer = previous_timer;
#endif
    }

private:
#if ENABLE_PROFILING
    void StartTiming() {
        start = Clock::now();
        running = true;
    }

    void StopTiming() {
        auto duration = Clock::now() - start;
        running = false;
        category.AddTime(std::chrono::duration_cast<Duration>(duration));
    }

    Clock::time_point start;
    bool running = false;

    Timer* previous_timer;
    static thread_local Timer* current_timer;
#endif

    TimingCategory& category;
};

/**
 * A Timer that automatically starts timing when created and stops at the end of the scope. Should
 * be used in the majority of cases.
 */
class ScopeTimer : public Timer {
public:
    ScopeTimer(TimingCategory& category) : Timer(category) {
        Start();
    }

    ~ScopeTimer() {
        Stop();
    }
};

} // namespace Profiling
} // namespace Common
