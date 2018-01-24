// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {
/// Performs IPC command buffer translation from one process to another.
ResultCode TranslateCommandBuffer(SharedPtr<Thread> src_thread, SharedPtr<Thread> dst_thread,
                                  VAddr src_address, VAddr dst_address, bool reply);
} // namespace Kernel
