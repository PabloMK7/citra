// Copyright (c) 2012- PPSSPP Project / Dolphin Project.
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

// This is a system to schedule events into the emulated machine's future. Time is measured
// in main CPU clock cycles.

// To schedule an event, you first have to register its type. This is where you pass in the
// callback. You then schedule events using the type id you get back.

// See HW/SystemTimers.cpp for the main part of Dolphin's usage of this scheduler.

// The int cycles_late that the callbacks get is how many cycles late it was.
// So to schedule a new event on a regular basis:
// inside callback:
//   ScheduleEvent(periodInCycles - cycles_late, callback, "whatever")

#include <functional>

#include "common/common_types.h"

extern int g_clock_rate_arm11;

inline s64 msToCycles(int ms) {
    return g_clock_rate_arm11 / 1000 * ms;
}

inline s64 msToCycles(float ms) {
    return (s64)(g_clock_rate_arm11 * ms * (0.001f));
}

inline s64 msToCycles(double ms) {
    return (s64)(g_clock_rate_arm11 * ms * (0.001));
}

inline s64 usToCycles(float us) {
    return (s64)(g_clock_rate_arm11 * us * (0.000001f));
}

inline s64 usToCycles(int us) {
    return (g_clock_rate_arm11 / 1000000 * (s64)us);
}

inline s64 usToCycles(s64 us) {
    return (g_clock_rate_arm11 / 1000000 * us);
}

inline s64 usToCycles(u64 us) {
    return (s64)(g_clock_rate_arm11 / 1000000 * us);
}

inline s64 cyclesToUs(s64 cycles) {
    return cycles / (g_clock_rate_arm11 / 1000000);
}

inline u64 cyclesToMs(s64 cycles) {
    return cycles / (g_clock_rate_arm11 / 1000);
}

namespace CoreTiming
{
void Init();
void Shutdown();

typedef void(*MHzChangeCallback)();
typedef std::function<void(u64 userdata, int cycles_late)> TimedCallback;

u64 GetTicks();
u64 GetIdleTicks();
u64 GetGlobalTimeUs();

/**
 * Registers an event type with the specified name and callback
 * @param name Name of the event type
 * @param callback Function that will execute when this event fires
 * @returns An identifier for the event type that was registered
 */
int RegisterEvent(const char* name, TimedCallback callback);
/// For save states.
void RestoreRegisterEvent(int event_type, const char *name, TimedCallback callback);
void UnregisterAllEvents();

/// userdata MAY NOT CONTAIN POINTERS. userdata might get written and reloaded from disk,
/// when we implement state saves.
/**
 * Schedules an event to run after the specified number of cycles,
 * with an optional parameter to be passed to the callback handler.
 * This must be run ONLY from within the cpu thread.
 * @param cycles_into_future The number of cycles after which this event will be fired
 * @param event_type The event type to fire, as returned from RegisterEvent
 * @param userdata Optional parameter to pass to the callback when fired
 */
void ScheduleEvent(s64 cycles_into_future, int event_type, u64 userdata = 0);

void ScheduleEvent_Threadsafe(s64 cycles_into_future, int event_type, u64 userdata = 0);
void ScheduleEvent_Threadsafe_Immediate(int event_type, u64 userdata = 0);

/**
 * Unschedules an event with the specified type and userdata
 * @param event_type The type of event to unschedule, as returned from RegisterEvent
 * @param userdata The userdata that identifies this event, as passed to ScheduleEvent
 * @returns The remaining ticks until the next invocation of the event callback
 */
s64 UnscheduleEvent(int event_type, u64 userdata);

s64 UnscheduleThreadsafeEvent(int event_type, u64 userdata);

void RemoveEvent(int event_type);
void RemoveThreadsafeEvent(int event_type);
void RemoveAllEvents(int event_type);
bool IsScheduled(int event_type);
/// Runs any pending events and updates downcount for the next slice of cycles
void Advance();
void MoveEvents();
void ProcessFifoWaitEvents();
void ForceCheck();

/// Pretend that the main CPU has executed enough cycles to reach the next event.
void Idle(int maxIdle = 0);

/// Clear all pending events. This should ONLY be done on exit or state load.
void ClearPendingEvents();

void LogPendingEvents();

/// Warning: not included in save states.
void RegisterAdvanceCallback(void(*callback)(int cycles_executed));
void RegisterMHzChangeCallback(MHzChangeCallback callback);

std::string GetScheduledEventsSummary();

void SetClockFrequencyMHz(int cpu_mhz);
int GetClockFrequencyMHz();
extern int g_slice_length;

} // namespace
