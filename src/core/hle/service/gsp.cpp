// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"
#include "common/bit_field.h"

#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
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

/// Gets the address of the start (header) of a command buffer in GSP shared memory
static inline u32 GX_GetCmdBufferAddress(u32 thread_id) {
    return (0x10002000 + 0x800 + (thread_id * 0x200));
}

/// Gets a pointer to the start (header) of a command buffer in GSP shared memory
static inline u8* GX_GetCmdBufferPointer(u32 thread_id, u32 offset=0) {
    return Memory::GetPointer(GX_GetCmdBufferAddress(thread_id) + offset);
}

/// Finishes execution of a GSP command
void GX_FinishCommand(u32 thread_id) {
    GX_CmdBufferHeader* header = (GX_CmdBufferHeader*)GX_GetCmdBufferPointer(thread_id);

    g_debugger.GXCommandProcessed(GX_GetCmdBufferPointer(thread_id, 0x20 + (header->index * 0x20)));

    header->number_commands = header->number_commands - 1;
    // TODO: Increment header->index?
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

Handle g_event_handle = 0;
u32 g_thread_id = 0;

enum {
    REG_FRAMEBUFFER_1   = 0x00400468,
    REG_FRAMEBUFFER_2   = 0x00400494,
};

/// Read a GSP GPU hardware register
void ReadHWRegs(Service::Interface* self) {
    static const u32 framebuffer_1[] = {GPU::PADDR_VRAM_TOP_LEFT_FRAME1, GPU::PADDR_VRAM_TOP_RIGHT_FRAME1};
    static const u32 framebuffer_2[] = {GPU::PADDR_VRAM_TOP_LEFT_FRAME2, GPU::PADDR_VRAM_TOP_RIGHT_FRAME2};

    u32* cmd_buff = Service::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];
    u32* dst = (u32*)Memory::GetPointer(cmd_buff[0x41]);

    switch (reg_addr) {

    // NOTE: Calling SetFramebufferLocation here is a hack... Not sure the correct way yet to set 
    // whether the framebuffers should be in VRAM or GSP heap, but from what I understand, if the 
    // user application is reading from either of these registers, then its going to be in VRAM.

    // Top framebuffer 1 addresses
    case REG_FRAMEBUFFER_1:
        GPU::SetFramebufferLocation(GPU::FRAMEBUFFER_LOCATION_VRAM);
        memcpy(dst, framebuffer_1, size);
        break;

    // Top framebuffer 2 addresses
    case REG_FRAMEBUFFER_2:
        GPU::SetFramebufferLocation(GPU::FRAMEBUFFER_LOCATION_VRAM);
        memcpy(dst, framebuffer_2, size);
        break;

    default:
        ERROR_LOG(GSP, "unknown register read at address %08X", reg_addr);
    }

}

void RegisterInterruptRelayQueue(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 flags = cmd_buff[1];
    u32 event_handle = cmd_buff[3];

    _assert_msg_(GSP, (event_handle != 0), "called, but event is nullptr!");

    g_event_handle = event_handle;

    Kernel::SetEventLocked(event_handle, false);

    // Hack - This function will permanently set the state of the GSP event such that GPU command 
    // synchronization barriers always passthrough. Correct solution would be to set this after the 
    // GPU as processed all queued up commands, but due to the emulator being single-threaded they
    // will always be ready.
    Kernel::SetPermanentLock(event_handle, true);

    cmd_buff[2] = g_thread_id;          // ThreadID
}


/// This triggers handling of the GX command written to the command buffer in shared memory.
void TriggerCmdReqQueue(Service::Interface* self) {
    GX_CmdBufferHeader* header = (GX_CmdBufferHeader*)GX_GetCmdBufferPointer(g_thread_id);
    u32* cmd_buff = (u32*)GX_GetCmdBufferPointer(g_thread_id, 0x20 + (header->index * 0x20));

    switch (static_cast<GXCommandId>(cmd_buff[0])) {

    // GX request DMA - typically used for copying memory from GSP heap to VRAM
    case GXCommandId::REQUEST_DMA:
        memcpy(Memory::GetPointer(cmd_buff[2]), Memory::GetPointer(cmd_buff[1]), cmd_buff[3]);
        break;

    case GXCommandId::SET_COMMAND_LIST_LAST:
        GPU::Write<u32>(GPU::Registers::CommandListAddress, cmd_buff[1] >> 3);
        GPU::Write<u32>(GPU::Registers::CommandListSize, cmd_buff[2] >> 3);
        GPU::Write<u32>(GPU::Registers::ProcessCommandList, 1); // TODO: Not sure if we are supposed to always write this

        // TODO: Move this to GPU
        // TODO: Not sure what units the size is measured in
        g_debugger.CommandListCalled(cmd_buff[1], (u32*)Memory::GetPointer(cmd_buff[1]), cmd_buff[2]);
        break;

    case GXCommandId::SET_MEMORY_FILL:
        break;

    case GXCommandId::SET_DISPLAY_TRANSFER:
        break;

    case GXCommandId::SET_TEXTURE_COPY:
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
    {0x00010082, nullptr,                       "WriteHWRegs"},
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
}

Interface::~Interface() {
}

} // namespace
