// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/svc.h"

namespace Kernel {

/**
 * Cancels a timer
 * @param handle Handle of the timer to cancel
 */
ResultCode CancelTimer(Handle handle);

/**
 * Starts a timer with the specified initial delay and interval
 * @param handle Handle of the timer to start
 * @param initial Delay until the timer is first fired
 * @param interval Delay until the timer is fired after the first time
 */
ResultCode SetTimer(Handle handle, s64 initial, s64 interval);

/**
 * Clears a timer
 * @param handle Handle of the timer to clear
 */
ResultCode ClearTimer(Handle handle);

/**
 * Creates a timer
 * @param handle Handle to the newly created Timer object
 * @param reset_type ResetType describing how to create the timer
 * @param name Optional name of timer
 * @return ResultCode of the error
 */
ResultCode CreateTimer(Handle* handle, const ResetType reset_type, const std::string& name="Unknown");

/// Initializes the required variables for timers
void TimersInit();
/// Tears down the timer variables
void TimersShutdown();
} // namespace
