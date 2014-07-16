// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"
#include "common/bit_field.h"

#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/gsp.h"

#include "core/hw/gpu.h"

#include "video_core/gpu_debugger.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

// Main graphics debugger object - TODO: Here is probably not the best place for this
GraphicsDebugger g_debugger;

/// GSP shared memory GX command buffer header
union GX_CmdBufferHeader {
    u32 hex;

    // Current command index. This index is updated by GSP module after loading the command data,
    // right before the command is processed. When this index is updated by GSP module, the total
    // commands field is decreased by one as well.
    BitField<0,8,u32>   index;

    // Total commands to process, must not be value 0 when GSP module handles commands. This must be
    // <=15 when writing a command to shared memory. This is incremented by the application when
    // writing a command to shared memory, after increasing this value TriggerCmdReqQueue is only
    // used if this field is value 1.
    BitField<8,8,u32>   number_commands;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

Handle g_event = 0;
Handle g_shared_memory = 0;

u32 g_thread_id = 0;

/// Gets a pointer to the start (header) of a command buffer in GSP shared memory
static inline u8* GX_GetCmdBufferPointer(u32 thread_id, u32 offset=0) {
    return Kernel::GetSharedMemoryPointer(g_shared_memory, 0x800 + (thread_id * 0x200) + offset);
}

/// Finishes execution of a GSP command
void GX_FinishCommand(u32 thread_id) {
    GX_CmdBufferHeader* header = (GX_CmdBufferHeader*)GX_GetCmdBufferPointer(thread_id);

    g_debugger.GXCommandProcessed(GX_GetCmdBufferPointer(thread_id, 0x20 + (header->index * 0x20)));

    header->number_commands = header->number_commands - 1;
    // TODO: Increment header->index?
}

/// Write a GSP GPU hardware register
void WriteHWRegs(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    // TODO: Return proper error codes
    if (reg_addr + size >= 0x420000) {
        ERROR_LOG(GPU, "Write address out of range! (address=0x%08x, size=0x%08x)", reg_addr, size);
        return;
    }

    // size should be word-aligned
    if ((size % 4) != 0) {
        ERROR_LOG(GPU, "Invalid size 0x%08x", size);
        return;
    }

    u32* src = (u32*)Memory::GetPointer(cmd_buff[0x4]);

    while (size > 0) {
        GPU::Write<u32>(reg_addr + 0x1EB00000, *src);

        size -= 4;
        ++src;
        reg_addr += 4;
    }
}

/// Read a GSP GPU hardware register
void ReadHWRegs(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    // TODO: Return proper error codes
    if (reg_addr + size >= 0x420000) {
        ERROR_LOG(GPU, "Read address out of range! (address=0x%08x, size=0x%08x)", reg_addr, size);
        return;
    }

    // size should be word-aligned
    if ((size % 4) != 0) {
        ERROR_LOG(GPU, "Invalid size 0x%08x", size);
        return;
    }

    u32* dst = (u32*)Memory::GetPointer(cmd_buff[0x41]);

    while (size > 0) {
        GPU::Read<u32>(*dst, reg_addr + 0x1EB00000);

        size -= 4;
        ++dst;
        reg_addr += 4;
    }
}

/**
 * GSP_GPU::RegisterInterruptRelayQueue service function
 *  Inputs:
 *      1 : "Flags" field, purpose is unknown
 *      3 : Handle to GSP synchronization event
 *  Outputs:
 *      0 : Result of function, 0 on success, otherwise error code
 *      2 : Thread index into GSP command buffer
 *      4 : Handle to GSP shared memory
 */
void RegisterInterruptRelayQueue(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 flags = cmd_buff[1];
    g_event = cmd_buff[3];

    _assert_msg_(GSP, (g_event != 0), "handle is not valid!");

    Kernel::SetEventLocked(g_event, false);

    // Hack - This function will permanently set the state of the GSP event such that GPU command
    // synchronization barriers always passthrough. Correct solution would be to set this after the
    // GPU as processed all queued up commands, but due to the emulator being single-threaded they
    // will always be ready.
    Kernel::SetPermanentLock(g_event, true);

    cmd_buff[0] = 0;                // Result - no error
    cmd_buff[2] = g_thread_id;      // ThreadID
    cmd_buff[4] = g_shared_memory;  // GSP shared memory
}


/// This triggers handling of the GX command written to the command buffer in shared memory.
void TriggerCmdReqQueue(Service::Interface* self) {

    // Utility function to convert register ID to address
    auto WriteGPURegister = [](u32 id, u32 data) {
        GPU::Write<u32>(0x1EF00000 + 4 * id, data);
    };

    GX_CmdBufferHeader* header = (GX_CmdBufferHeader*)GX_GetCmdBufferPointer(g_thread_id);
    u32* cmd_buff = (u32*)GX_GetCmdBufferPointer(g_thread_id, 0x20 + (header->index * 0x20));

    switch (static_cast<GXCommandId>(cmd_buff[0])) {

    // GX request DMA - typically used for copying memory from GSP heap to VRAM
    case GXCommandId::REQUEST_DMA:
        memcpy(Memory::GetPointer(cmd_buff[2]), Memory::GetPointer(cmd_buff[1]), cmd_buff[3]);
        break;

    case GXCommandId::SET_COMMAND_LIST_LAST:
        WriteGPURegister(GPU::Regs::CommandProcessor + 2, cmd_buff[1] >> 3); // command list data address
        WriteGPURegister(GPU::Regs::CommandProcessor, cmd_buff[2] >> 3);     // command list address
        WriteGPURegister(GPU::Regs::CommandProcessor + 4, 1);                // TODO: Not sure if we are supposed to always write this .. seems to trigger processing though

        // TODO: Move this to GPU
        // TODO: Not sure what units the size is measured in
        g_debugger.CommandListCalled(cmd_buff[1], (u32*)Memory::GetPointer(cmd_buff[1]), cmd_buff[2]);
        break;

    case GXCommandId::SET_MEMORY_FILL:
        WriteGPURegister(GPU::Regs::MemoryFill, cmd_buff[1] >> 3);              // Start 1
        WriteGPURegister(GPU::Regs::MemoryFill + 1, cmd_buff[3] >> 3);          // End 1
        WriteGPURegister(GPU::Regs::MemoryFill + 2, cmd_buff[3] - cmd_buff[1]); // Size 1
        WriteGPURegister(GPU::Regs::MemoryFill + 3, cmd_buff[2]);               // Value 1

        WriteGPURegister(GPU::Regs::MemoryFill + 4, cmd_buff[4] >> 3);          // Start 2
        WriteGPURegister(GPU::Regs::MemoryFill + 5, cmd_buff[6] >> 3);          // End 2
        WriteGPURegister(GPU::Regs::MemoryFill + 6, cmd_buff[6] - cmd_buff[4]); // Size 2
        WriteGPURegister(GPU::Regs::MemoryFill + 7, cmd_buff[5]);               // Value 2
        break;

    // TODO: Check if texture copies are implemented correctly..
    case GXCommandId::SET_DISPLAY_TRANSFER:
    case GXCommandId::SET_TEXTURE_COPY:
        WriteGPURegister(GPU::Regs::DisplayTransfer, cmd_buff[1] >> 3);     // input buffer address
        WriteGPURegister(GPU::Regs::DisplayTransfer + 1, cmd_buff[2] >> 3); // output buffer address
        WriteGPURegister(GPU::Regs::DisplayTransfer + 3, cmd_buff[3]);      // input buffer size
        WriteGPURegister(GPU::Regs::DisplayTransfer + 2, cmd_buff[4]);      // output buffer size
        WriteGPURegister(GPU::Regs::DisplayTransfer + 4, cmd_buff[5]);      // transfer flags

        // TODO: Should this only be ORed with 1 for texture copies?
        WriteGPURegister(GPU::Regs::DisplayTransfer + 6, 1);                // trigger transfer
        break;

    case GXCommandId::SET_COMMAND_LIST_FIRST:
    {
        //u32* buf0_data = (u32*)Memory::GetPointer(cmd_buff[1]);
        //u32* buf1_data = (u32*)Memory::GetPointer(cmd_buff[3]);
        //u32* buf2_data = (u32*)Memory::GetPointer(cmd_buff[5]);
        break;
    }

    default:
        ERROR_LOG(GSP, "unknown command 0x%08X", cmd_buff[0]);
    }

    GX_FinishCommand(g_thread_id);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, WriteHWRegs,                   "WriteHWRegs"},
    {0x00020084, nullptr,                       "WriteHWRegsWithMask"},
    {0x00030082, nullptr,                       "WriteHWRegRepeat"},
    {0x00040080, ReadHWRegs,                    "ReadHWRegs"},
    {0x00050200, nullptr,                       "SetBufferSwap"},
    {0x00060082, nullptr,                       "SetCommandList"},
    {0x000700C2, nullptr,                       "RequestDma"},
    {0x00080082, nullptr,                       "FlushDataCache"},
    {0x00090082, nullptr,                       "InvalidateDataCache"},
    {0x000A0044, nullptr,                       "RegisterInterruptEvents"},
    {0x000B0040, nullptr,                       "SetLcdForceBlack"},
    {0x000C0000, TriggerCmdReqQueue,            "TriggerCmdReqQueue"},
    {0x000D0140, nullptr,                       "SetDisplayTransfer"},
    {0x000E0180, nullptr,                       "SetTextureCopy"},
    {0x000F0200, nullptr,                       "SetMemoryFill"},
    {0x00100040, nullptr,                       "SetAxiConfigQoSMode"},
    {0x00110040, nullptr,                       "SetPerfLogMode"},
    {0x00120000, nullptr,                       "GetPerfLog"},
    {0x00130042, RegisterInterruptRelayQueue,   "RegisterInterruptRelayQueue"},
    {0x00140000, nullptr,                       "UnregisterInterruptRelayQueue"},
    {0x00150002, nullptr,                       "TryAcquireRight"},
    {0x00160042, nullptr,                       "AcquireRight"},
    {0x00170000, nullptr,                       "ReleaseRight"},
    {0x00180000, nullptr,                       "ImportDisplayCaptureInfo"},
    {0x00190000, nullptr,                       "SaveVramSysArea"},
    {0x001A0000, nullptr,                       "RestoreVramSysArea"},
    {0x001B0000, nullptr,                       "ResetGpuCore"},
    {0x001C0040, nullptr,                       "SetLedForceOff"},
    {0x001D0040, nullptr,                       "SetTestCommand"},
    {0x001E0080, nullptr,                       "SetInternalPriorities"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
    g_shared_memory = Kernel::CreateSharedMemory("GSPSharedMem");
}

Interface::~Interface() {
}

} // namespace
