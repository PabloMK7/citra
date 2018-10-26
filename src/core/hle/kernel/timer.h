// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

namespace Kernel {

class Timer final : public WaitObject {
public:
    std::string GetTypeName() const override {
        return "Timer";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Timer;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    u64 GetInitialDelay() const {
        return initial_delay;
    }

    u64 GetIntervalDelay() const {
        return interval_delay;
    }

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    /**
     * Starts the timer, with the specified initial delay and interval.
     * @param initial Delay until the timer is first fired
     * @param interval Delay until the timer is fired after the first time
     */
    void Set(s64 initial, s64 interval);

    void Cancel();
    void Clear();

    /**
     * Signals the timer, waking up any waiting threads and rescheduling it
     * for the next interval.
     * This method should not be called from outside the timer callback handler,
     * lest multiple callback events get scheduled.
     */
    void Signal(s64 cycles_late);

private:
    explicit Timer(KernelSystem& kernel);
    ~Timer() override;

    ResetType reset_type; ///< The ResetType of this timer

    u64 initial_delay;  ///< The delay until the timer fires for the first time
    u64 interval_delay; ///< The delay until the timer fires after the first time

    bool signaled;    ///< Whether the timer has been signaled or not
    std::string name; ///< Name of timer (optional)

    /// ID used as userdata to reference this object when inserting into the CoreTiming queue.
    u64 callback_id;

    friend class KernelSystem;
};

/// Initializes the required variables for timers
void TimersInit();
/// Tears down the timer variables
void TimersShutdown();

} // namespace Kernel
