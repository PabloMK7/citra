// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/common_types.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/thread.h"

namespace Memory {
class MemorySystem;
}

namespace Kernel {

class KernelSystem;

struct MappedBufferContext {
    IPC::MappedBufferPermissions permissions;
    u32 size;
    VAddr source_address;
    VAddr target_address;

    std::shared_ptr<BackingMem> buffer;
};

/// Performs IPC command buffer translation from one process to another.
Result TranslateCommandBuffer(KernelSystem& system, Memory::MemorySystem& memory,
                              std::shared_ptr<Thread> src_thread,
                              std::shared_ptr<Thread> dst_thread, VAddr src_address,
                              VAddr dst_address,
                              std::vector<MappedBufferContext>& mapped_buffer_context, bool reply);
} // namespace Kernel
