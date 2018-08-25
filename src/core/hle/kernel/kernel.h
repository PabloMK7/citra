// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

/// Initialize the kernel with the specified system mode.
void Init(u32 system_mode);

/// Shutdown the kernel
void Shutdown();

} // namespace Kernel
