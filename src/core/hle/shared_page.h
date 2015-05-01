// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/**
 * The shared page stores various runtime configuration settings. This memory page is
 * read-only for user processes (there is a bit in the header that grants the process
 * write access, according to 3dbrew; this is not emulated)
 */

#include "common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

template <typename T>
void Read(T &var, const u32 addr);

void Set3DSlider(float amount);

void Init();

void Shutdown();

} // namespace
