// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

class KernelSystem {
public:
    explicit KernelSystem(u32 system_mode);
    ~KernelSystem();
};

} // namespace Kernel
