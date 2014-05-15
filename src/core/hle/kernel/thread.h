// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

class Thread;

/// Sets up the primary application thread
Handle __KernelSetupMainThread(s32 priority, int stack_size=0x4000);

void __KernelThreadingInit();
void __KernelThreadingShutdown();
