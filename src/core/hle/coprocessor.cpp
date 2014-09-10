// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/coprocessor.h"
#include "core/hle/hle.h"
#include "core/mem_map.h"

namespace HLE {

/// Returns the coprocessor (in this case, syscore) command buffer pointer
Addr GetThreadCommandBuffer() {
    // Called on insruction: mrc p15, 0, r0, c13, c0, 3
    return Memory::KERNEL_MEMORY_VADDR;
}

/// Call an MRC (move to ARM register from coprocessor) instruction in HLE
s32 CallMRC(u32 instruction) {
    CoprocessorOperation operation = (CoprocessorOperation)((instruction >> 20) & 0xFF);

    switch (operation) {

    case CALL_GET_THREAD_COMMAND_BUFFER:
        return GetThreadCommandBuffer();

    default:
        DEBUG_LOG(OSHLE, "unknown MRC call 0x%08X", instruction);
        break;
    }
    return -1;
}

} // namespace
