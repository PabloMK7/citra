// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <set>

#include "common/common.h"

#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Timer : public WaitObject {
public:
    std::string GetTypeName() const override { return "Timer"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::Timer;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    ResetType reset_type;                   ///< The ResetType of this timer

    bool signaled;                          ///< Whether the timer has been signaled or not
    std::string name;                       ///< Name of timer (optional)

    u64 initial_delay;                      ///< The delay until the timer fires for the first time
    u64 interval_delay;                     ///< The delay until the timer fires after the first time

    ResultVal<bool> Wait(bool wait_thread) override {
        bool wait = !signaled;
        if (wait && wait_thread) {
            AddWaitingThread(GetCurrentThread());
            Kernel::WaitCurrentThread_WaitSynchronization(WAITTYPE_TIMER, this);
        }
        return MakeResult<bool>(wait);
    }

    ResultVal<bool> Acquire() override {
        return MakeResult<bool>(true);
    }
};

/**
 * Creates a timer.
 * @param handle Reference to handle for the newly created timer
 * @param reset_type ResetType describing how to create timer
 * @param name Optional name of timer
 * @return Newly created Timer object
 */
Timer* CreateTimer(Handle& handle, const ResetType reset_type, const std::string& name) {
    Timer* timer = new Timer;

    handle = Kernel::g_handle_table.Create(timer).ValueOr(INVALID_HANDLE);

    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = name;
    timer->initial_delay = 0;
    timer->interval_delay = 0;
    return timer;
}

ResultCode CreateTimer(Handle* handle, const ResetType reset_type, const std::string& name) {
    CreateTimer(*handle, reset_type, name);
    return RESULT_SUCCESS;
}

ResultCode ClearTimer(Handle handle) {
    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);
    
    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->signaled = false;
    return RESULT_SUCCESS;
}

/// The event type of the generic timer callback event
static int TimerCallbackEventType = -1;

/// The timer callback event, called when a timer is fired
static void TimerCallback(u64 timer_handle, int cycles_late) {
    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(timer_handle);

    if (timer == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid timer %u", timer_handle);
        return;
    }

    LOG_TRACE(Kernel, "Timer %u fired", timer_handle);

    timer->signaled = true;

    // Resume all waiting threads
    timer->ReleaseAllWaitingThreads();

    if (timer->reset_type == RESETTYPE_ONESHOT)
        timer->signaled = false;

    if (timer->interval_delay != 0) {
        // Reschedule the timer with the interval delay
        u64 interval_microseconds = timer->interval_delay / 1000;
        CoreTiming::ScheduleEvent(usToCycles(interval_microseconds) - cycles_late, 
                TimerCallbackEventType, timer_handle);
    }
}

ResultCode SetTimer(Handle handle, s64 initial, s64 interval) {
    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);

    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->initial_delay = initial;
    timer->interval_delay = interval;

    u64 initial_microseconds = initial / 1000;
    CoreTiming::ScheduleEvent(usToCycles(initial_microseconds), TimerCallbackEventType, handle);
    return RESULT_SUCCESS;
}

ResultCode CancelTimer(Handle handle) {
    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);

    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    CoreTiming::UnscheduleEvent(TimerCallbackEventType, handle);
    return RESULT_SUCCESS;
}

void TimersInit() {
    TimerCallbackEventType = CoreTiming::RegisterEvent("TimerCallback", TimerCallback);
}

void TimersShutdown() {
}

} // namespace
