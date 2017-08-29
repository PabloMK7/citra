// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <core/hle/lock.h>

namespace HLE {
std::recursive_mutex g_hle_lock;
}
