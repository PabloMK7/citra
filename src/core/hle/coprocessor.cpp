// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/coprocessor.h"
#include "core/hle/hle.h"
#include "core/mem_map.h"
#include "core/core.h"

namespace HLE {

enum {
    CMD_GX_REQUEST_DMA  = 0x00000000,
};

/// Data synchronization barrier
u32 DataSynchronizationBarrier(u32* command_buffer) {
    u32 command = command_buffer[0];

    switch (command) {

    case CMD_GX_REQUEST_DMA:
        {
            u32* src = (u32*)Memory::GetPointer(command_buffer[1]);
            u32* dst = (u32*)Memory::GetPointer(command_buffer[2]);
            u32 size = command_buffer[3];
            memcpy(dst, src, size);
        }
        break;

    default:
        ERROR_LOG(OSHLE, "MRC::DataSynchronizationBarrier unknown command 0x%08X", command);
        return -1;
    }

    return 0;
}

/// Returns the coprocessor (in this case, syscore) command buffer pointer
Addr GetThreadCommandBuffer() {
    // Called on insruction: mrc p15, 0, r0, c13, c0, 3
    // Returns an address in OSHLE memory for the CPU to read/write to
    RETURN(CMD_BUFFER_ADDR);
    return CMD_BUFFER_ADDR;
}

/// Call an MCR (move to coprocessor from ARM register) instruction in HLE
s32 CallMCR(u32 instruction, u32 value) {
    CoprocessorOperation operation = (CoprocessorOperation)((instruction >> 20) & 0xFF);
    ERROR_LOG(OSHLE, "unimplemented MCR instruction=0x%08X, operation=%02X, value=%08X", 
        instruction, operation, value);
    return -1;
}

/// Call an MRC (move to ARM register from coprocessor) instruction in HLE
s32 CallMRC(u32 instruction) {
    CoprocessorOperation operation = (CoprocessorOperation)((instruction >> 20) & 0xFF);

    switch (operation) {

    case DATA_SYNCHRONIZATION_BARRIER:
        return DataSynchronizationBarrier((u32*)Memory::GetPointer(PARAM(0)));

    case CALL_GET_THREAD_COMMAND_BUFFER:
        return GetThreadCommandBuffer();

    default:
        ERROR_LOG(OSHLE, "unimplemented MRC instruction 0x%08X", instruction);
        break;
    }
    return -1;
}

} // namespace
