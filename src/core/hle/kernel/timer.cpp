// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <unordered_map>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

/// The event type of the generic timer callback event
static CoreTiming::EventType* timer_callback_event_type = nullptr;

static u64 next_timer_callback_id;
static std::unordered_map<u64, Timer*> timer_callback_table;

Timer::Timer(KernelSystem& kernel) : WaitObject(kernel) {}
Timer::~Timer() {
    Cancel();
    timer_callback_table.erase(callback_id);
}

SharedPtr<Timer> KernelSystem::CreateTimer(ResetType reset_type, std::string name) {
    SharedPtr<Timer> timer(new Timer(*this));

    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = std::move(name);
    timer->initial_delay = 0;
    timer->interval_delay = 0;
    timer->callback_id = ++next_timer_callback_id;
    timer_callback_table[timer->callback_id] = timer.get();

    return timer;
}

bool Timer::ShouldWait(Thread* thread) const {
    return !signaled;
}

void Timer::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void Timer::Set(s64 initial, s64 interval) {
    // Ensure we get rid of any previous scheduled event
    Cancel();

    initial_delay = initial;
    interval_delay = interval;

    if (initial == 0) {
        // Immediately invoke the callback
        Signal(0);
    } else {
        CoreTiming::ScheduleEvent(nsToCycles(initial), timer_callback_event_type, callback_id);
    }
}

void Timer::Cancel() {
    CoreTiming::UnscheduleEvent(timer_callback_event_type, callback_id);
}

void Timer::Clear() {
    signaled = false;
}

void Timer::WakeupAllWaitingThreads() {
    WaitObject::WakeupAllWaitingThreads();

    if (reset_type == ResetType::Pulse)
        signaled = false;
}

void Timer::Signal(s64 cycles_late) {
    LOG_TRACE(Kernel, "Timer {} fired", GetObjectId());

    signaled = true;

    // Resume all waiting threads
    WakeupAllWaitingThreads();

    if (interval_delay != 0) {
        // Reschedule the timer with the interval delay
        CoreTiming::ScheduleEvent(nsToCycles(interval_delay) - cycles_late,
                                  timer_callback_event_type, callback_id);
    }
}

/// The timer callback event, called when a timer is fired
static void TimerCallback(u64 callback_id, s64 cycles_late) {
    SharedPtr<Timer> timer = timer_callback_table.at(callback_id);

    if (timer == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid timer {:016x}", callback_id);
        return;
    }

    timer->Signal(cycles_late);
}

void TimersInit() {
    next_timer_callback_id = 0;
    timer_callback_table.clear();
    timer_callback_event_type = CoreTiming::RegisterEvent("TimerCallback", TimerCallback);
}

void TimersShutdown() {}

} // namespace Kernel
