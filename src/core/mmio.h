// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"

namespace Memory {

/**
 * Represents a device with memory mapped IO.
 * A device may be mapped to multiple regions of memory.
 */
class MMIORegion {
public:
    virtual ~MMIORegion() = default;

    virtual bool IsValidAddress(VAddr addr) = 0;

    virtual u8 Read8(VAddr addr) = 0;
    virtual u16 Read16(VAddr addr) = 0;
    virtual u32 Read32(VAddr addr) = 0;
    virtual u64 Read64(VAddr addr) = 0;

    virtual bool ReadBlock(VAddr src_addr, void* dest_buffer, std::size_t size) = 0;

    virtual void Write8(VAddr addr, u8 data) = 0;
    virtual void Write16(VAddr addr, u16 data) = 0;
    virtual void Write32(VAddr addr, u32 data) = 0;
    virtual void Write64(VAddr addr, u64 data) = 0;

    virtual bool WriteBlock(VAddr dest_addr, const void* src_buffer, std::size_t size) = 0;
};

using MMIORegionPointer = std::shared_ptr<MMIORegion>;
}; // namespace Memory
