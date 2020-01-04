// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include <boost/serialization/shared_ptr.hpp>
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
    std::shared_ptr<BackingMem> reserve_buffer;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& permissions;
        ar& size;
        ar& source_address;
        ar& target_address;
        ar& buffer;
        ar& reserve_buffer;
    }
    friend class boost::serialization::access;
};

/// Performs IPC command buffer translation from one process to another.
ResultCode TranslateCommandBuffer(KernelSystem& system, Memory::MemorySystem& memory,
                                  std::shared_ptr<Thread> src_thread,
                                  std::shared_ptr<Thread> dst_thread, VAddr src_address,
                                  VAddr dst_address,
                                  std::vector<MappedBufferContext>& mapped_buffer_context,
                                  bool reply);
} // namespace Kernel
