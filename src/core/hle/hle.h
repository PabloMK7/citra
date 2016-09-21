// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

typedef u32 Handle;
typedef s32 Result;

const Handle INVALID_HANDLE = 0;

namespace HLE {

void Reschedule(const char* reason);
bool IsReschedulePending();
void DoneRescheduling();

void Init();
void Shutdown();

} // namespace
