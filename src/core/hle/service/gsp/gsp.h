// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hle/service/gsp/gsp_lcd.h"

namespace Core {
class System;
}

namespace Service::GSP {
/**
 * Retrieves the framebuffer info stored in the GSP shared memory for the
 * specified screen index and thread id.
 * @param thread_id GSP thread id of the process that accesses the structure that we are requesting.
 * @param screen_index Index of the screen we are requesting (Top = 0, Bottom = 1).
 * @returns FramebufferUpdate Information about the specified framebuffer.
 */
FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index);

/**
 * Signals that the specified interrupt type has occurred to userland code
 * @param interrupt_id ID of interrupt that is being signalled
 */
void SignalInterrupt(InterruptId interrupt_id);

void InstallInterfaces(Core::System& system);
} // namespace Service::GSP
