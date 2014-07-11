// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/kernel/thread.h"
#include "core/hw/gpu.h"

#include "video_core/video_core.h"


namespace GPU {

Registers g_regs;

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
FramebufferLocation GetFramebufferLocation(u32 address) {
    if ((address & ~Memory::VRAM_MASK) == Memory::VRAM_PADDR) {
        return FRAMEBUFFER_LOCATION_VRAM;
    } else if ((address & ~Memory::FCRAM_MASK) == Memory::FCRAM_PADDR) {
        return FRAMEBUFFER_LOCATION_FCRAM;
    } else {
        ERROR_LOG(GPU, "unknown framebuffer location!");
    }
    return FRAMEBUFFER_LOCATION_UNKNOWN;
}

u32 GetFramebufferAddr(const u32 address) {
    switch (GetFramebufferLocation(address)) {
    case FRAMEBUFFER_LOCATION_FCRAM:
        return Memory::VirtualAddressFromPhysical_FCRAM(address);
    case FRAMEBUFFER_LOCATION_VRAM:
        return Memory::VirtualAddressFromPhysical_VRAM(address);
    default:
        ERROR_LOG(GPU, "unknown framebuffer location");
    }
    return 0;
}

/**
 * Gets a read-only pointer to a framebuffer in memory
 * @param address Physical address of framebuffer
 * @return Returns const pointer to raw framebuffer
 */
const u8* GetFramebufferPointer(const u32 address) {
    u32 addr = GetFramebufferAddr(address);
    return (addr != 0) ? Memory::GetPointer(addr) : nullptr;
}

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr) {
    case Registers::MemoryFillStart1:
    case Registers::MemoryFillStart2:
        var = g_regs.memory_fill[(addr - Registers::MemoryFillStart1) / 0x10].address_start;
        break;

    case Registers::MemoryFillEnd1:
    case Registers::MemoryFillEnd2:
        var = g_regs.memory_fill[(addr - Registers::MemoryFillEnd1) / 0x10].address_end;
        break;

    case Registers::MemoryFillSize1:
    case Registers::MemoryFillSize2:
        var = g_regs.memory_fill[(addr - Registers::MemoryFillSize1) / 0x10].size;
        break;

    case Registers::MemoryFillValue1:
    case Registers::MemoryFillValue2:
        var = g_regs.memory_fill[(addr - Registers::MemoryFillValue1) / 0x10].value;
        break;

    case Registers::FramebufferTopSize:
        var = g_regs.top_framebuffer.size;
        break;

    case Registers::FramebufferTopLeft1:
        var = g_regs.framebuffer_top_left_1;
        break;

    case Registers::FramebufferTopLeft2:
        var = g_regs.framebuffer_top_left_2;
        break;

    case Registers::FramebufferTopFormat:
        var = g_regs.top_framebuffer.format;
        break;

    case Registers::FramebufferTopSwapBuffers:
        var = g_regs.top_framebuffer.active_fb;
        break;

    case Registers::FramebufferTopStride:
        var = g_regs.top_framebuffer.stride;
        break;

    case Registers::FramebufferTopRight1:
        var = g_regs.framebuffer_top_right_1;
        break;

    case Registers::FramebufferTopRight2:
        var = g_regs.framebuffer_top_right_2;
        break;

    case Registers::FramebufferSubSize:
        var = g_regs.sub_framebuffer.size;
        break;

    case Registers::FramebufferSubLeft1:
        var = g_regs.framebuffer_sub_left_1;
        break;

    case Registers::FramebufferSubRight1:
        var = g_regs.framebuffer_sub_right_1;
        break;

    case Registers::FramebufferSubFormat:
        var = g_regs.sub_framebuffer.format;
        break;

    case Registers::FramebufferSubSwapBuffers:
        var = g_regs.sub_framebuffer.active_fb;
        break;

    case Registers::FramebufferSubStride:
        var = g_regs.sub_framebuffer.stride;
        break;

    case Registers::FramebufferSubLeft2:
        var = g_regs.framebuffer_sub_left_2;
        break;

    case Registers::FramebufferSubRight2:
        var = g_regs.framebuffer_sub_right_2;
        break;

    case Registers::DisplayInputBufferAddr:
        var = g_regs.display_transfer.input_address;
        break;

    case Registers::DisplayOutputBufferAddr:
        var = g_regs.display_transfer.output_address;
        break;

    case Registers::DisplayOutputBufferSize:
        var = g_regs.display_transfer.output_size;
        break;

    case Registers::DisplayInputBufferSize:
        var = g_regs.display_transfer.input_size;
        break;

    case Registers::DisplayTransferFlags:
        var = g_regs.display_transfer.flags;
        break;

    // Not sure if this is supposed to be readable
    case Registers::DisplayTriggerTransfer:
        var = g_regs.display_transfer.trigger;
        break;

    case Registers::CommandListSize:
        var = g_regs.command_list_size;
        break;

    case Registers::CommandListAddress:
        var = g_regs.command_list_address;
        break;

    case Registers::ProcessCommandList:
        var = g_regs.command_processing_enabled;
        break;

    default:
        ERROR_LOG(GPU, "unknown Read%d @ 0x%08X", sizeof(var) * 8, addr);
        break;
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    switch (static_cast<Registers::Id>(addr)) {
    case Registers::MemoryFillStart1:
    case Registers::MemoryFillStart2:
        g_regs.memory_fill[(addr - Registers::MemoryFillStart1) / 0x10].address_start = data;
        break;

    case Registers::MemoryFillEnd1:
    case Registers::MemoryFillEnd2:
        g_regs.memory_fill[(addr - Registers::MemoryFillEnd1) / 0x10].address_end = data;
        break;

    case Registers::MemoryFillSize1:
    case Registers::MemoryFillSize2:
        g_regs.memory_fill[(addr - Registers::MemoryFillSize1) / 0x10].size = data;
        break;

    case Registers::MemoryFillValue1:
    case Registers::MemoryFillValue2:
    {
        Registers::MemoryFillConfig& config = g_regs.memory_fill[(addr - Registers::MemoryFillValue1) / 0x10];
        config.value = data;

        // TODO: Not sure if this check should be done at GSP level instead
        if (config.address_start) {
            // TODO: Not sure if this algorithm is correct, particularly because it doesn't use the size member at all
            u32* start = (u32*)Memory::GetPointer(config.GetStartAddress());
            u32* end = (u32*)Memory::GetPointer(config.GetEndAddress());
            for (u32* ptr = start; ptr < end; ++ptr)
                *ptr = bswap32(config.value); // TODO: This is just a workaround to missing framebuffer format emulation

            DEBUG_LOG(GPU, "MemoryFill from %x to %x", config.GetStartAddress(), config.GetEndAddress());
        }
        break;
    }

    // TODO: Framebuffer registers!!
    case Registers::FramebufferTopSwapBuffers:
        g_regs.top_framebuffer.active_fb = data;
        // TODO: Not sure if this should only be done upon a change!
        break;

    case Registers::FramebufferSubSwapBuffers:
        g_regs.sub_framebuffer.active_fb = data;
        // TODO: Not sure if this should only be done upon a change!
        break;

    case Registers::DisplayInputBufferAddr:
        g_regs.display_transfer.input_address = data;
        break;

    case Registers::DisplayOutputBufferAddr:
        g_regs.display_transfer.output_address = data;
        break;

    case Registers::DisplayOutputBufferSize:
        g_regs.display_transfer.output_size = data;
        break;

    case Registers::DisplayInputBufferSize:
        g_regs.display_transfer.input_size = data;
        break;

    case Registers::DisplayTransferFlags:
        g_regs.display_transfer.flags = data;
        break;

    case Registers::DisplayTriggerTransfer:
        g_regs.display_transfer.trigger = data;
        if (g_regs.display_transfer.trigger & 1) {
            u8* source_pointer = Memory::GetPointer(g_regs.display_transfer.GetPhysicalInputAddress());
            u8* dest_pointer = Memory::GetPointer(g_regs.display_transfer.GetPhysicalOutputAddress());

            for (int y = 0; y < g_regs.display_transfer.output_height; ++y) {
                // TODO: Why does the register seem to hold twice the framebuffer width?
                for (int x = 0; x < g_regs.display_transfer.output_width / 2; ++x) {
                    int source[4] = { 0, 0, 0, 0}; // rgba;

                    switch (g_regs.display_transfer.input_format) {
                    case Registers::FramebufferFormat::RGBA8:
                    {
                        // TODO: Most likely got the component order messed up.
                        u8* srcptr = source_pointer + x * 4 + y * g_regs.display_transfer.input_width * 4 / 2;
                        source[0] = srcptr[0]; // blue
                        source[1] = srcptr[1]; // green
                        source[2] = srcptr[2]; // red
                        source[3] = srcptr[3]; // alpha
                        break;
                    }

                    default:
                        ERROR_LOG(GPU, "Unknown source framebuffer format %x", (int)g_regs.display_transfer.input_format.Value());
                        break;
                    }

                    switch (g_regs.display_transfer.output_format) {
                    /*case Registers::FramebufferFormat::RGBA8:
                    {
                        // TODO: Untested
                        u8* dstptr = (u32*)(dest_pointer + x * 4 + y * g_regs.display_transfer.output_width * 4);
                        dstptr[0] = source[0];
                        dstptr[1] = source[1];
                        dstptr[2] = source[2];
                        dstptr[3] = source[3];
                        break;
                    }*/

                    case Registers::FramebufferFormat::RGB8:
                    {
                        u8* dstptr = dest_pointer + x * 3 + y * g_regs.display_transfer.output_width * 3 / 2;
                        dstptr[0] = source[0]; // blue
                        dstptr[1] = source[1]; // green
                        dstptr[2] = source[2]; // red
                        break;
                    }

                    default:
                        ERROR_LOG(GPU, "Unknown destination framebuffer format %x", static_cast<int>(g_regs.display_transfer.output_format.Value()));
                        break;
                    }
                }
            }

            DEBUG_LOG(GPU, "DisplayTriggerTransfer: %x bytes from %x(%xx%x)-> %x(%xx%x), dst format %x",
                      g_regs.display_transfer.output_height * g_regs.display_transfer.output_width * 4,
                      g_regs.display_transfer.GetPhysicalInputAddress(), (int)g_regs.display_transfer.input_width, (int)g_regs.display_transfer.input_height,
                      g_regs.display_transfer.GetPhysicalOutputAddress(), (int)g_regs.display_transfer.output_width, (int)g_regs.display_transfer.output_height,
                      (int)g_regs.display_transfer.output_format.Value());
        }
        break;

    case Registers::CommandListSize:
        g_regs.command_list_size = data;
        break;

    case Registers::CommandListAddress:
        g_regs.command_list_address = data;
        break;

    case Registers::ProcessCommandList:
        g_regs.command_processing_enabled = data;
        if (g_regs.command_processing_enabled & 1)
        {
            // u32* buffer = (u32*)Memory::GetPointer(g_regs.command_list_address << 3);
            ERROR_LOG(GPU, "Beginning %x bytes of commands from address %x", g_regs.command_list_size, g_regs.command_list_address << 3);
            // TODO: Process command list!
        }
        break;

    default:
        ERROR_LOG(GPU, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
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
//    SetFramebufferLocation(FRAMEBUFFER_LOCATION_FCRAM);
    SetFramebufferLocation(FRAMEBUFFER_LOCATION_VRAM);

    // TODO: Width should be 240 instead?
    g_regs.top_framebuffer.width = 480;
    g_regs.top_framebuffer.height = 400;
    g_regs.top_framebuffer.stride = 480*3;
    g_regs.top_framebuffer.color_format = Registers::FramebufferFormat::RGB8;
    g_regs.top_framebuffer.active_fb = 0;

    g_regs.sub_framebuffer.width = 480;
    g_regs.sub_framebuffer.height = 400;
    g_regs.sub_framebuffer.stride = 480*3;
    g_regs.sub_framebuffer.color_format = Registers::FramebufferFormat::RGB8;
    g_regs.sub_framebuffer.active_fb = 0;

    NOTICE_LOG(GPU, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(GPU, "shutdown OK");
}

} // namespace
