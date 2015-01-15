// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/svc.h"

namespace Kernel {

/**
 * Changes whether an event is locked or not
 * @param handle Handle to event to change
 * @param locked Boolean locked value to set event
 */
ResultCode SetEventLocked(const Handle handle, const bool locked);

/**
 * Signals an event
 * @param handle Handle to event to signal
 */
ResultCode SignalEvent(const Handle handle);

/**
 * Clears an event
 * @param handle Handle to event to clear
 */
ResultCode ClearEvent(Handle handle);

/**
 * Creates an event
 * @param reset_type ResetType describing how to create event
 * @param name Optional name of event
 * @return Handle to newly created Event object
 */
Handle CreateEvent(const ResetType reset_type, const std::string& name="Unknown");

} // namespace
