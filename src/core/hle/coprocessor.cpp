// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/coprocessor.h"
#include "core/hle/hle.h"
#include "core/mem_map.h"
#include "core/core.h"

namespace HLE {

/// Data synchronization barrier
u32 DataSynchronizationBarrier() {
    return 0;
}

/// Returns the coprocessor (in this case, syscore) command buffer pointer
Addr GetThreadCommandBuffer() {
    // Called on insruction: mrc p15, 0, r0, c13, c0, 3
    return Memory::KERNEL_MEMORY_VADDR;
}

/// Call an MCR (move to coprocessor from ARM register) instruction in HLE
s32 CallMCR(u32 instruction, u32 value) {
    CoprocessorOperation operation = (CoprocessorOperation)((instruction >> 20) & 0xFF);
    ERROR_LOG(OSHLE, "unimplemented MCR instruction=0x%08X, operation=%02X, value=%08X", 
        instruction, operation, value);
    return 0;
}

/// Call an MRC (move to ARM register from coprocessor) instruction in HLE
s32 CallMRC(u32 instruction) {
    CoprocessorOperation operation = (CoprocessorOperation)((instruction >> 20) & 0xFF);

    switch (operation) {

    case DATA_SYNCHRONIZATION_BARRIER:
        return DataSynchronizationBarrier();

    case CALL_GET_THREAD_COMMAND_BUFFER:
        return GetThreadCommandBuffer();

    default:
        ERROR_LOG(OSHLE, "unimplemented MRC instruction 0x%08X", instruction);
        break;
    }
    return 0;
}

} // namespace
