// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

/// The event type of the generic timer callback event
static int timer_callback_event_type;
// TODO(yuriks): This can be removed if Timer objects are explicitly pooled in the future, allowing
//               us to simply use a pool index or similar.
static Kernel::HandleTable timer_callback_handle_table;

Timer::Timer() {}
Timer::~Timer() {}

SharedPtr<Timer> Timer::Create(ResetType reset_type, std::string name) {
    SharedPtr<Timer> timer(new Timer);

    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = std::move(name);
    timer->initial_delay = 0;
    timer->interval_delay = 0;
    timer->callback_handle = timer_callback_handle_table.Create(timer).MoveFrom();

    return timer;
}

bool Timer::ShouldWait() {
    return !signaled;
}

void Timer::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void Timer::Set(s64 initial, s64 interval) {
    // Ensure we get rid of any previous scheduled event
    Cancel();

    initial_delay = initial;
    interval_delay = interval;

    u64 initial_microseconds = initial / 1000;
    CoreTiming::ScheduleEvent(usToCycles(initial_microseconds), timer_callback_event_type,
                              callback_handle);

    HLE::Reschedule(__func__);
}

void Timer::Cancel() {
    CoreTiming::UnscheduleEvent(timer_callback_event_type, callback_handle);

    HLE::Reschedule(__func__);
}

void Timer::Clear() {
    signaled = false;
}

/// The timer callback event, called when a timer is fired
static void TimerCallback(u64 timer_handle, int cycles_late) {
    SharedPtr<Timer> timer =
        timer_callback_handle_table.Get<Timer>(static_cast<Handle>(timer_handle));

    if (timer == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid timer %08" PRIx64, timer_handle);
        return;
    }

    LOG_TRACE(Kernel, "Timer %08" PRIx64 " fired", timer_handle);

    timer->signaled = true;

    // Resume all waiting threads
    timer->WakeupAllWaitingThreads();

    if (timer->interval_delay != 0) {
        // Reschedule the timer with the interval delay
        u64 interval_microseconds = timer->interval_delay / 1000;
        CoreTiming::ScheduleEvent(usToCycles(interval_microseconds) - cycles_late,
                                  timer_callback_event_type, timer_handle);
    }
}

void TimersInit() {
    timer_callback_handle_table.Clear();
    timer_callback_event_type = CoreTiming::RegisterEvent("TimerCallback", TimerCallback);
}

void TimersShutdown() {}

} // namespace
