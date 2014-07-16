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

RegisterSet<u32, Regs> g_regs;

u64 g_last_ticks = 0; ///< Last CPU ticks

/**
 * Sets whether the framebuffers are in the GSP heap (FCRAM) or VRAM
 * @param
 */
void SetFramebufferLocation(const FramebufferLocation mode) {
    switch (mode) {
    case FRAMEBUFFER_LOCATION_FCRAM:
    {
        auto& framebuffer_top = g_regs.Get<Regs::FramebufferTop>();
        auto& framebuffer_sub = g_regs.Get<Regs::FramebufferBottom>();

        framebuffer_top.data.address_left1  = PADDR_TOP_LEFT_FRAME1;
        framebuffer_top.data.address_left2  = PADDR_TOP_LEFT_FRAME2;
        framebuffer_top.data.address_right1 = PADDR_TOP_RIGHT_FRAME1;
        framebuffer_top.data.address_right2 = PADDR_TOP_RIGHT_FRAME2;
        framebuffer_sub.data.address_left1  = PADDR_SUB_FRAME1;
        //framebuffer_sub.data.address_left2  = unknown;
        framebuffer_sub.data.address_right1 = PADDR_SUB_FRAME2;
        //framebuffer_sub.data.address_right2 = unknown;
        break;
    }

    case FRAMEBUFFER_LOCATION_VRAM:
    {
        auto& framebuffer_top = g_regs.Get<Regs::FramebufferTop>();
        auto& framebuffer_sub = g_regs.Get<Regs::FramebufferBottom>();

        framebuffer_top.data.address_left1  = PADDR_VRAM_TOP_LEFT_FRAME1;
        framebuffer_top.data.address_left2  = PADDR_VRAM_TOP_LEFT_FRAME2;
        framebuffer_top.data.address_right1 = PADDR_VRAM_TOP_RIGHT_FRAME1;
        framebuffer_top.data.address_right2 = PADDR_VRAM_TOP_RIGHT_FRAME2;
        framebuffer_sub.data.address_left1  = PADDR_VRAM_SUB_FRAME1;
        //framebuffer_sub.data.address_left2  = unknown;
        framebuffer_sub.data.address_right1 = PADDR_VRAM_SUB_FRAME2;
        //framebuffer_sub.data.address_right2 = unknown;
        break;
    }
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
inline void Read(T &var, const u32 raw_addr) {
    u32 addr = raw_addr - 0x1EF00000;
    int index = addr / 4;

    // Reads other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds || !std::is_same<T,u32>::value)
    {
        ERROR_LOG(GPU, "unknown Read%d @ 0x%08X", sizeof(var) * 8, addr);
        return;
    }

    var = g_regs[static_cast<Regs::Id>(addr / 4)];
}

template <typename T>
inline void Write(u32 addr, const T data) {
    addr -= 0x1EF00000;
    int index = addr / 4;

    // Writes other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds || !std::is_same<T,u32>::value)
    {
        ERROR_LOG(GPU, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
        return;
    }

    g_regs[static_cast<Regs::Id>(index)] = data;

    switch (static_cast<Regs::Id>(index)) {

    // Memory fills are triggered once the fill value is written.
    // NOTE: This is not verified.
    case Regs::MemoryFill + 3:
    case Regs::MemoryFill + 7:
    {
        const auto& config = g_regs.Get<Regs::MemoryFill>(static_cast<Regs::Id>(index - 3));

        // TODO: Not sure if this check should be done at GSP level instead
        if (config.data.address_start) {
            // TODO: Not sure if this algorithm is correct, particularly because it doesn't use the size member at all
            u32* start = (u32*)Memory::GetPointer(config.data.GetStartAddress());
            u32* end = (u32*)Memory::GetPointer(config.data.GetEndAddress());
            for (u32* ptr = start; ptr < end; ++ptr)
                *ptr = bswap32(config.data.value); // TODO: This is just a workaround to missing framebuffer format emulation

            DEBUG_LOG(GPU, "MemoryFill from %x to %x", config.data.GetStartAddress(), config.data.GetEndAddress());
        }
        break;
    }

    case Regs::DisplayTransfer + 6:
    {
        const auto& config = g_regs.Get<Regs::DisplayTransfer>();
        if (config.data.trigger & 1) {
            u8* source_pointer = Memory::GetPointer(config.data.GetPhysicalInputAddress());
            u8* dest_pointer = Memory::GetPointer(config.data.GetPhysicalOutputAddress());

            for (int y = 0; y < config.data.output_height; ++y) {
                // TODO: Why does the register seem to hold twice the framebuffer width?
                for (int x = 0; x < config.data.output_width / 2; ++x) {
                    int source[4] = { 0, 0, 0, 0}; // rgba;

                    switch (config.data.input_format) {
                    case Regs::FramebufferFormat::RGBA8:
                    {
                        // TODO: Most likely got the component order messed up.
                        u8* srcptr = source_pointer + x * 4 + y * config.data.input_width * 4 / 2;
                        source[0] = srcptr[0]; // blue
                        source[1] = srcptr[1]; // green
                        source[2] = srcptr[2]; // red
                        source[3] = srcptr[3]; // alpha
                        break;
                    }

                    default:
                        ERROR_LOG(GPU, "Unknown source framebuffer format %x", config.data.input_format.Value());
                        break;
                    }

                    switch (config.data.output_format) {
                    /*case Regs::FramebufferFormat::RGBA8:
                    {
                        // TODO: Untested
                        u8* dstptr = (u32*)(dest_pointer + x * 4 + y * config.data.output_width * 4);
                        dstptr[0] = source[0];
                        dstptr[1] = source[1];
                        dstptr[2] = source[2];
                        dstptr[3] = source[3];
                        break;
                    }*/

                    case Regs::FramebufferFormat::RGB8:
                    {
                        u8* dstptr = dest_pointer + x * 3 + y * config.data.output_width * 3 / 2;
                        dstptr[0] = source[0]; // blue
                        dstptr[1] = source[1]; // green
                        dstptr[2] = source[2]; // red
                        break;
                    }

                    default:
                        ERROR_LOG(GPU, "Unknown destination framebuffer format %x", config.data.output_format.Value());
                        break;
                    }
                }
            }

            DEBUG_LOG(GPU, "DisplayTriggerTransfer: %x bytes from %x(%xx%x)-> %x(%xx%x), dst format %x",
                      config.data.output_height * config.data.output_width * 4,
                      config.data.GetPhysicalInputAddress(), (int)config.data.input_width, (int)config.data.input_height,
                      config.data.GetPhysicalOutputAddress(), (int)config.data.output_width, (int)config.data.output_height,
                      config.data.output_format.Value());
        }
        break;
    }

    case Regs::CommandProcessor + 4:
    {
        const auto& config = g_regs.Get<Regs::CommandProcessor>();
        if (config.data.trigger & 1)
        {
            // u32* buffer = (u32*)Memory::GetPointer(config.data.address << 3);
            ERROR_LOG(GPU, "Beginning %x bytes of commands from address %x", config.data.size, config.data.address << 3);
            // TODO: Process command list!
        }
        break;
    }

    default:
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

    auto& framebuffer_top = g_regs.Get<Regs::FramebufferTop>();
    auto& framebuffer_sub = g_regs.Get<Regs::FramebufferBottom>();
    // TODO: Width should be 240 instead?
    framebuffer_top.data.width = 480;
    framebuffer_top.data.height = 400;
    framebuffer_top.data.stride = 480*3;
    framebuffer_top.data.color_format = Regs::FramebufferFormat::RGB8;
    framebuffer_top.data.active_fb = 0;

    framebuffer_sub.data.width = 480;
    framebuffer_sub.data.height = 400;
    framebuffer_sub.data.stride = 480*3;
    framebuffer_sub.data.color_format = Regs::FramebufferFormat::RGB8;
    framebuffer_sub.data.active_fb = 0;

    NOTICE_LOG(GPU, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(GPU, "shutdown OK");
}

} // namespace
