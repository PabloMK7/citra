// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"

#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/service/gsp.h"

#include "core/hw/lcd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

enum {
    REG_FRAMEBUFFER_1   = 0x00400468,
    REG_FRAMEBUFFER_2   = 0x00400494,
};

/// Read a GSP GPU hardware register
void ReadHWRegs(Service::Interface* self) {
    static const u32 framebuffer_1[] = {LCD::PADDR_VRAM_TOP_LEFT_FRAME1, LCD::PADDR_VRAM_TOP_RIGHT_FRAME1};
    static const u32 framebuffer_2[] = {LCD::PADDR_VRAM_TOP_LEFT_FRAME2, LCD::PADDR_VRAM_TOP_RIGHT_FRAME2};

    u32* cmd_buff = (u32*)HLE::GetPointer(HLE::CMD_BUFFER_ADDR + Service::kCommandHeaderOffset);
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];
    u32* dst = (u32*)Memory::GetPointer(cmd_buff[0x41]);
    
    switch (reg_addr) {

    // NOTE: Calling SetFramebufferLocation here is a hack... Not sure the correct way yet to set 
    // whether the framebuffers should be in VRAM or GSP heap, but from what I understand, if the 
    // user application is reading from either of these registers, then its going to be in VRAM.

    // Top framebuffer 1 addresses
    case REG_FRAMEBUFFER_1:
        LCD::SetFramebufferLocation(LCD::FRAMEBUFFER_LOCATION_VRAM);
        memcpy(dst, framebuffer_1, size);
        break;

    // Top framebuffer 2 addresses
    case REG_FRAMEBUFFER_2:
        LCD::SetFramebufferLocation(LCD::FRAMEBUFFER_LOCATION_VRAM);
        memcpy(dst, framebuffer_2, size);
        break;

    default:
        ERROR_LOG(OSHLE, "GSP_GPU::ReadHWRegs unknown register read at address %08X", reg_addr);
    }

}

void RegisterInterruptRelayQueue(Service::Interface* self) {
    u32* cmd_buff = (u32*)HLE::GetPointer(HLE::CMD_BUFFER_ADDR + Service::kCommandHeaderOffset);
    u32 flags = cmd_buff[1];
    u32 event_handle = cmd_buff[3]; // TODO(bunnei): Implement event handling
    cmd_buff[4] = self->NewHandle();

    return;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, NULL,                          "WriteHWRegs"},
    {0x00020084, NULL,                          "WriteHWRegsWithMask"},
    {0x00030082, NULL,                          "WriteHWRegRepeat"},
    {0x00040080, ReadHWRegs,                    "ReadHWRegs"},
    {0x00050200, NULL,                          "SetBufferSwap"},
    {0x00060082, NULL,                          "SetCommandList"},
    {0x000700C2, NULL,                          "RequestDma"},
    {0x00080082, NULL,                          "FlushDataCache"},
    {0x00090082, NULL,                          "InvalidateDataCache"},
    {0x000A0044, NULL,                          "RegisterInterruptEvents"},
    {0x000B0040, NULL,                          "SetLcdForceBlack"},
    {0x000C0000, NULL,                          "TriggerCmdReqQueue"},
    {0x000D0140, NULL,                          "SetDisplayTransfer"},
    {0x000E0180, NULL,                          "SetTextureCopy"},
    {0x000F0200, NULL,                          "SetMemoryFill"},
    {0x00100040, NULL,                          "SetAxiConfigQoSMode"},
    {0x00110040, NULL,                          "SetPerfLogMode"},
    {0x00120000, NULL,                          "GetPerfLog"},
    {0x00130042, RegisterInterruptRelayQueue,   "RegisterInterruptRelayQueue"},
    {0x00140000, NULL,                          "UnregisterInterruptRelayQueue"},
    {0x00150002, NULL,                          "TryAcquireRight"},
    {0x00160042, NULL,                          "AcquireRight"},
    {0x00170000, NULL,                          "ReleaseRight"},
    {0x00180000, NULL,                          "ImportDisplayCaptureInfo"},
    {0x00190000, NULL,                          "SaveVramSysArea"},
    {0x001A0000, NULL,                          "RestoreVramSysArea"},
    {0x001B0000, NULL,                          "ResetGpuCore"},
    {0x001C0040, NULL,                          "SetLedForceOff"},
    {0x001D0040, NULL,                          "SetTestCommand"},
    {0x001E0080, NULL,                          "SetInternalPriorities"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
