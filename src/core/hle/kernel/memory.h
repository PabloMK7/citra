// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/common_types.h"

namespace Kernel {

struct AddressMapping;
class VMManager;

struct MemoryRegionInfo {
    u32 base; // Not an address, but offset from start of FCRAM
    u32 size;
    u32 used;

    std::shared_ptr<std::vector<u8>> linear_heap_memory;
};

void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping);

} // namespace Kernel
