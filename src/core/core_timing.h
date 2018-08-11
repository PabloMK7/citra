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
#include "common/common_types.h"
#include "common/logging/log.h"

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
    if (us / 1000000 > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (us > MAX_VALUE_TO_MULTIPLY) {
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
    if (ns / 1000000000 > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (ns > MAX_VALUE_TO_MULTIPLY) {
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

namespace CoreTiming {

struct EventType;

using TimedCallback = std::function<void(u64 userdata, int cycles_late)>;

/**
 * CoreTiming begins at the boundary of timing slice -1. An initial call to Advance() is
 * required to end slice -1 and start slice 0 before the first cycle of code is executed.
 */
void Init();
void Shutdown();

/**
 * This should only be called from the emu thread, if you are calling it any other thread, you are
 * doing something evil
 */
u64 GetTicks();
u64 GetIdleTicks();
void AddTicks(u64 ticks);

/**
 * Returns the event_type identifier. if name is not unique, it will assert.
 */
EventType* RegisterEvent(const std::string& name, TimedCallback callback);
void UnregisterAllEvents();

/**
 * After the first Advance, the slice lengths and the downcount will be reduced whenever an event
 * is scheduled earlier than the current values.
 * Scheduling from a callback will not update the downcount until the Advance() completes.
 */
void ScheduleEvent(s64 cycles_into_future, const EventType* event_type, u64 userdata = 0);

/**
 * This is to be called when outside of hle threads, such as the graphics thread, wants to
 * schedule things to be executed on the main thread.
 * Not that this doesn't change slice_length and thus events scheduled by this might be called
 * with a delay of up to MAX_SLICE_LENGTH
 */
void ScheduleEventThreadsafe(s64 cycles_into_future, const EventType* event_type, u64 userdata);

void UnscheduleEvent(const EventType* event_type, u64 userdata);

/// We only permit one event of each type in the queue at a time.
void RemoveEvent(const EventType* event_type);
void RemoveNormalAndThreadsafeEvent(const EventType* event_type);

/** Advance must be called at the beginning of dispatcher loops, not the end. Advance() ends
 * the previous timing slice and begins the next one, you must Advance from the previous
 * slice to the current one before executing any cycles. CoreTiming starts in slice -1 so an
 * Advance() is required to initialize the slice length before the first cycle of emulated
 * instructions is executed.
 */
void Advance();
void MoveEvents();

/// Pretend that the main CPU has executed enough cycles to reach the next event.
void Idle();

/// Clear all pending events. This should ONLY be done on exit.
void ClearPendingEvents();

void ForceExceptionCheck(s64 cycles);

std::chrono::microseconds GetGlobalTimeUs();

s64 GetDowncount();

} // namespace CoreTiming
