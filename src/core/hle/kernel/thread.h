// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

class Thread;

/// Creates a new thread
Thread *__KernelCreateThread(Handle &handle, const char *name, u32 entry_point, s32 priority, 
    s32 processor_id, u32 stack_top, int stack_size=KERNEL_DEFAULT_STACK_SIZE);

/// Sets up the primary application thread
Handle __KernelSetupMainThread(s32 priority, int stack_size=KERNEL_DEFAULT_STACK_SIZE);

void __KernelThreadingInit();
void __KernelThreadingShutdown();
