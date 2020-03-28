// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

/**
 * This is a system to schedule events into the emulated machine's future. Time is measured
 * in main CPU clock cycles.
 *
 * To schedule an event, you first have to register its type. This is where you pass in the
 * callback. You then schedule events using the type id you get back.
 *
 * The int cyclesLate that the callbacks get is how many cycles late it was.
 * So to schedule a new event on a regular basis:
 * inside callback:
 *   ScheduleEvent(periodInCycles - cyclesLate, callback, "whatever")
 */

#include <chrono>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/threadsafe_queue.h"

// The timing we get from the assembly is 268,111,855.956 Hz
// It is possible that this number isn't just an integer because the compiler could have
// optimized the multiplication by a multiply-by-constant division.
// Rounding to the nearest integer should be fine
constexpr u64 BASE_CLOCK_RATE_ARM11 = 268111856;
constexpr u64 MAX_VALUE_TO_MULTIPLY = std::numeric_limits<s64>::max() / BASE_CLOCK_RATE_ARM11;

inline s64 msToCycles(int ms) {
    // since ms is int there is no way to overflow
    return BASE_CLOCK_RATE_ARM11 * static_cast<s64>(ms) / 1000;
}

inline s64 msToCycles(float ms) {
    return static_cast<s64>(BASE_CLOCK_RATE_ARM11 * (0.001f) * ms);
}

inline s64 msToCycles(double ms) {
    return static_cast<s64>(BASE_CLOCK_RATE_ARM11 * (0.001) * ms);
}

inline s64 usToCycles(float us) {
    return static_cast<s64>(BASE_CLOCK_RATE_ARM11 * (0.000001f) * us);
}

inline s64 usToCycles(int us) {
    return (BASE_CLOCK_RATE_ARM11 * static_cast<s64>(us) / 1000000);
}

inline s64 usToCycles(s64 us) {
    if (us / 1000000 > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (us > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return BASE_CLOCK_RATE_ARM11 * (us / 1000000);
    }
    return (BASE_CLOCK_RATE_ARM11 * us) / 1000000;
}

inline s64 usToCycles(u64 us) {
    if (us / 1000000 > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (us > MAX_VALUE_TO_MULTIPLY) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return BASE_CLOCK_RATE_ARM11 * static_cast<s64>(us / 1000000);
    }
    return (BASE_CLOCK_RATE_ARM11 * static_cast<s64>(us)) / 1000000;
}

inline s64 nsToCycles(float ns) {
    return static_cast<s64>(BASE_CLOCK_RATE_ARM11 * (0.000000001f) * ns);
}

inline s64 nsToCycles(int ns) {
    return BASE_CLOCK_RATE_ARM11 * static_cast<s64>(ns) / 1000000000;
}

inline s64 nsToCycles(s64 ns) {
    if (ns / 1000000000 > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (ns > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return BASE_CLOCK_RATE_ARM11 * (ns / 1000000000);
    }
    return (BASE_CLOCK_RATE_ARM11 * ns) / 1000000000;
}

inline s64 nsToCycles(u64 ns) {
    if (ns / 1000000000 > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (ns > MAX_VALUE_TO_MULTIPLY) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return BASE_CLOCK_RATE_ARM11 * (static_cast<s64>(ns) / 1000000000);
    }
    return (BASE_CLOCK_RATE_ARM11 * static_cast<s64>(ns)) / 1000000000;
}

inline u64 cyclesToNs(s64 cycles) {
    return cycles * 1000000000 / BASE_CLOCK_RATE_ARM11;
}

inline s64 cyclesToUs(s64 cycles) {
    return cycles * 1000000 / BASE_CLOCK_RATE_ARM11;
}

inline u64 cyclesToMs(s64 cycles) {
    return cycles * 1000 / BASE_CLOCK_RATE_ARM11;
}

namespace Core {

using TimedCallback = std::function<void(u64 userdata, int cycles_late)>;

struct TimingEventType {
    TimedCallback callback;
    const std::string* name;
};

class Timing {
public:
    struct Event {
        s64 time;
        u64 fifo_order;
        u64 userdata;
        const TimingEventType* type;

        bool operator>(const Event& right) const;
        bool operator<(const Event& right) const;
    };

    static constexpr int MAX_SLICE_LENGTH = 20000;

    class Timer {
    public:
        Timer(double cpu_clock_scale);
        ~Timer();

        s64 GetMaxSliceLength() const;

        void Advance(s64 max_slice_length = MAX_SLICE_LENGTH);

        void Idle();

        u64 GetTicks() const;
        u64 GetIdleTicks() const;

        void AddTicks(u64 ticks);

        s64 GetDowncount() const;

        void ForceExceptionCheck(s64 cycles);

        void MoveEvents();

    private:
        friend class Timing;
        // The queue is a min-heap using std::make_heap/push_heap/pop_heap.
        // We don't use std::priority_queue because we need to be able to serialize, unserialize and
        // erase arbitrary events (RemoveEvent()) regardless of the queue order. These aren't
        // accomodated by the standard adaptor class.
        std::vector<Event> event_queue;
        u64 event_fifo_id = 0;
        // the queue for storing the events from other threads threadsafe until they will be added
        // to the event_queue by the emu thread
        Common::MPSCQueue<Event> ts_queue;
        // Are we in a function that has been called from Advance()
        // If events are sheduled from a function that gets called from Advance(),
        // don't change slice_length and downcount.
        // The time between CoreTiming being intialized and the first call to Advance() is
        // considered the slice boundary between slice -1 and slice 0. Dispatcher loops must call
        // Advance() before executing the first cycle of each slice to prepare the slice length and
        // downcount for that slice.
        bool is_timer_sane = true;

        s64 slice_length = MAX_SLICE_LENGTH;
        s64 downcount = MAX_SLICE_LENGTH;
        s64 executed_ticks = 0;
        u64 idled_cycles = 0;
        // Stores a scaling for the internal clockspeed. Changing this number results in
        // under/overclocking the guest cpu
        double cpu_clock_scale = 1.0;
    };

    explicit Timing(std::size_t num_cores, u32 cpu_clock_percentage);

    ~Timing(){};

    /**
     * Returns the event_type identifier. if name is not unique, it will assert.
     */
    TimingEventType* RegisterEvent(const std::string& name, TimedCallback callback);

    void ScheduleEvent(s64 cycles_into_future, const TimingEventType* event_type, u64 userdata = 0,
                       std::size_t core_id = std::numeric_limits<std::size_t>::max());

    void UnscheduleEvent(const TimingEventType* event_type, u64 userdata);

    /// We only permit one event of each type in the queue at a time.
    void RemoveEvent(const TimingEventType* event_type);

    void SetCurrentTimer(std::size_t core_id);

    s64 GetTicks() const;

    s64 GetGlobalTicks() const;

    void AddToGlobalTicks(s64 ticks) {
        global_timer += ticks;
    }

    /**
     * Updates the value of the cpu clock scaling to the new percentage.
     */
    void UpdateClockSpeed(u32 cpu_clock_percentage);

    std::chrono::microseconds GetGlobalTimeUs() const;

    std::shared_ptr<Timer> GetTimer(std::size_t cpu_id);

private:
    s64 global_timer = 0;

    // unordered_map stores each element separately as a linked list node so pointers to
    // elements remain stable regardless of rehashes/resizing.
    std::unordered_map<std::string, TimingEventType> event_types = {};

    std::vector<std::shared_ptr<Timer>> timers;
    std::shared_ptr<Timer> current_timer;

    // Stores a scaling for the internal clockspeed. Changing this number results in
    // under/overclocking the guest cpu
    double cpu_clock_scale = 1.0;
};

} // namespace Core
