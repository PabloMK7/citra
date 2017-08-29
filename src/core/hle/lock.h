// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>

namespace HLE {
/*
 * Synchronizes access to the internal HLE kernel structures, it is acquired when a guest
 * application thread performs a syscall. It should be acquired by any host threads that read or
 * modify the HLE kernel state. Note: Any operation that directly or indirectly reads from or writes
 * to the emulated memory is not protected by this mutex, and should be avoided in any threads other
 * than the CPU thread.
 */
extern std::recursive_mutex g_hle_lock;
} // namespace HLE
