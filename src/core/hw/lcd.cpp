// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/kernel/thread.h"
#include "core/hw/lcd.h"

#include "video_core/video_core.h"


namespace LCD {

Registers g_regs;

static const u32 kFrameTicks = 268123480 / 60;  ///< 268MHz / 60 frames per second

u64 g_last_ticks = 0; ///< Last CPU ticks

/**
 * Sets whether the framebuffers are in the GSP heap (FCRAM) or VRAM
 * @param 
 */
void SetFramebufferLocation(const FramebufferLocation mode) {
    switch (mode) {
    case FRAMEBUFFER_LOCATION_FCRAM:
        g_regs.framebuffer_top_left_1   = PADDR_TOP_LEFT_FRAME1;
        g_regs.framebuffer_top_left_2   = PADDR_TOP_LEFT_FRAME2;
        g_regs.framebuffer_top_right_1  = PADDR_TOP_RIGHT_FRAME1;
        g_regs.framebuffer_top_right_2  = PADDR_TOP_RIGHT_FRAME2;
        g_regs.framebuffer_sub_left_1   = PADDR_SUB_FRAME1;
        //g_regs.framebuffer_sub_left_2  = unknown;
        g_regs.framebuffer_sub_right_1  = PADDR_SUB_FRAME2;
        //g_regs.framebufferr_sub_right_2 = unknown;
        break;

    case FRAMEBUFFER_LOCATION_VRAM:
        g_regs.framebuffer_top_left_1   = PADDR_VRAM_TOP_LEFT_FRAME1;
        g_regs.framebuffer_top_left_2   = PADDR_VRAM_TOP_LEFT_FRAME2;
        g_regs.framebuffer_top_right_1  = PADDR_VRAM_TOP_RIGHT_FRAME1;
        g_regs.framebuffer_top_right_2  = PADDR_VRAM_TOP_RIGHT_FRAME2;
        g_regs.framebuffer_sub_left_1   = PADDR_VRAM_SUB_FRAME1;
        //g_regs.framebuffer_sub_left_2  = unknown;
        g_regs.framebuffer_sub_right_1  = PADDR_VRAM_SUB_FRAME2;
        //g_regs.framebufferr_sub_right_2 = unknown;
        break;
    }
}

/**
 * Gets the location of the framebuffers
 * @return Location of framebuffers as FramebufferLocation enum
 */
const FramebufferLocation GetFramebufferLocation() {
    if ((g_regs.framebuffer_top_right_1 & ~Memory::VRAM_MASK) == Memory::VRAM_PADDR) {
        return FRAMEBUFFER_LOCATION_VRAM;
    } else if ((g_regs.framebuffer_top_right_1 & ~Memory::FCRAM_MASK) == Memory::FCRAM_PADDR) {
        return FRAMEBUFFER_LOCATION_FCRAM;
    } else {
        ERROR_LOG(LCD, "unknown framebuffer location!");
    }
    return FRAMEBUFFER_LOCATION_UNKNOWN;
}

/**
 * Gets a read-only pointer to a framebuffer in memory
 * @param address Physical address of framebuffer
 * @return Returns const pointer to raw framebuffer
 */
const u8* GetFramebufferPointer(const u32 address) {
    switch (GetFramebufferLocation()) {
    case FRAMEBUFFER_LOCATION_FCRAM:
        return (const u8*)Memory::GetPointer(Memory::VirtualAddressFromPhysical_FCRAM(address));
    case FRAMEBUFFER_LOCATION_VRAM:
        return (const u8*)Memory::GetPointer(Memory::VirtualAddressFromPhysical_VRAM(address));
    default:
        ERROR_LOG(LCD, "unknown framebuffer location");
    }
    return NULL;
}

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr) {
    case REG_FRAMEBUFFER_TOP_LEFT_1:
        var = g_regs.framebuffer_top_left_1;
        break;

    case REG_FRAMEBUFFER_TOP_LEFT_2:
        var = g_regs.framebuffer_top_left_2;
        break;

    case REG_FRAMEBUFFER_TOP_RIGHT_1:
        var = g_regs.framebuffer_top_right_1;
        break;

    case REG_FRAMEBUFFER_TOP_RIGHT_2:
        var = g_regs.framebuffer_top_right_2;
        break;

    case REG_FRAMEBUFFER_SUB_LEFT_1:
        var = g_regs.framebuffer_sub_left_1;
        break;

    case REG_FRAMEBUFFER_SUB_RIGHT_1:
        var = g_regs.framebuffer_sub_right_1;
        break;

    case CommandListSize:
        var = g_regs.command_list_size;
        break;

    case CommandListAddress:
        var = g_regs.command_list_address;
        break;

    case ProcessCommandList:
        var = g_regs.command_processing_enabled;
        break;

    default:
        ERROR_LOG(LCD, "unknown Read%d @ 0x%08X", sizeof(var) * 8, addr);
        break;
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    switch (addr) {
    case CommandListSize:
        g_regs.command_list_size = data;
        break;

    case CommandListAddress:
        g_regs.command_list_address = data;
        break;

    case ProcessCommandList:
        g_regs.command_processing_enabled = data;
        if (g_regs.command_processing_enabled & 1)
        {
            // u32* buffer = (u32*)Memory::GetPointer(g_regs.command_list_address << 3);
            ERROR_LOG(LCD, "Beginning %x bytes of commands from address %x", g_regs.command_list_size, g_regs.command_list_address << 3);
            // TODO: Process command list!
        }
        break;

    default:
        ERROR_LOG(LCD, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
        break;
    }
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

template void Write<u64>(u32 addr, const u64 data);
template void Write<u32>(u32 addr, const u32 data);
template void Write<u16>(u32 addr, const u16 data);
template void Write<u8>(u32 addr, const u8 data);

/// Update hardware
void Update() {
    u64 current_ticks = Core::g_app_core->GetTicks();

    // Fake a vertical blank
    if ((current_ticks - g_last_ticks) >= kFrameTicks) {
        g_last_ticks = current_ticks;
        VideoCore::g_renderer->SwapBuffers();
        Kernel::WaitCurrentThread(WAITTYPE_VBLANK);
    }
}

/// Initialize hardware
void Init() {
    g_last_ticks = Core::g_app_core->GetTicks();
    SetFramebufferLocation(FRAMEBUFFER_LOCATION_FCRAM);
    NOTICE_LOG(LCD, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(LCD, "shutdown OK");
}

} // namespace
