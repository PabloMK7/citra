// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Service {
namespace BOSS {

/// Initialize BOSS service(s)
void Init();

/// Shutdown BOSS service(s)
void Shutdown();

} // namespace BOSS
} // namespace Service
