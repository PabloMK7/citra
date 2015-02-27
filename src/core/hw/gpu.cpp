// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/arm/arm_interface.h"

#include "core/settings.h"
#include "core/core.h"
#include "core/mem_map.h"
#include "core/core_timing.h"

#include "core/hle/hle.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/dsp_dsp.h"

#include "core/hw/gpu.h"

#include "video_core/command_processor.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"
#include <video_core/color.h>

namespace GPU {

Regs g_regs;

/// True if the current frame was skipped
bool g_skip_frame = false;

/// 268MHz / gpu_refresh_rate frames per second
static u64 frame_ticks;
/// Event id for CoreTiming
static int vblank_event;
/// Total number of frames drawn
static u64 frame_count;
/// True if the last frame was skipped
static bool last_skip_frame = false;

template <typename T>
inline void Read(T &var, const u32 raw_addr) {
    u32 addr = raw_addr - 0x1EF00000;
    u32 index = addr / 4;

    // Reads other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds() || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_GPU, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, addr);
        return;
    }

    var = g_regs[addr / 4];
}

template <typename T>
inline void Write(u32 addr, const T data) {
    addr -= 0x1EF00000;
    u32 index = addr / 4;

    // Writes other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds() || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_GPU, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32)data, addr);
        return;
    }

    g_regs[index] = static_cast<u32>(data);

    switch (index) {

    // Memory fills are triggered once the fill value is written.
    case GPU_REG_INDEX_WORKAROUND(memory_fill_config[0].trigger, 0x00004 + 0x3):
    case GPU_REG_INDEX_WORKAROUND(memory_fill_config[1].trigger, 0x00008 + 0x3):
    {
        const bool is_second_filler = (index != GPU_REG_INDEX(memory_fill_config[0].trigger));
        auto& config = g_regs.memory_fill_config[is_second_filler];

        if (config.address_start && config.trigger) {
            u8* start = Memory::GetPointer(Memory::PhysicalToVirtualAddress(config.GetStartAddress()));
            u8* end = Memory::GetPointer(Memory::PhysicalToVirtualAddress(config.GetEndAddress()));

            if (config.fill_24bit) {
                // fill with 24-bit values
                for (u8* ptr = start; ptr < end; ptr += 3) {
                    ptr[0] = config.value_24bit_b;
                    ptr[1] = config.value_24bit_g;
                    ptr[2] = config.value_24bit_r;
                }
            } else if (config.fill_32bit) {
                // fill with 32-bit values
                for (u32* ptr = (u32*)start; ptr < (u32*)end; ++ptr)
                    *ptr = config.value_32bit;
            } else {
                // fill with 16-bit values
                for (u16* ptr = (u16*)start; ptr < (u16*)end; ++ptr)
                    *ptr = config.value_16bit;
            }

            LOG_TRACE(HW_GPU, "MemoryFill from 0x%08x to 0x%08x", config.GetStartAddress(), config.GetEndAddress());

            config.trigger = 0;
            config.finished = 1;

            if (!is_second_filler) {
                GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PSC0);
            } else {
                GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PSC1);
            }
        }
        break;
    }

    case GPU_REG_INDEX(display_transfer_config.trigger):
    {
        const auto& config = g_regs.display_transfer_config;
        if (config.trigger & 1) {
            u8* source_pointer = Memory::GetPointer(Memory::PhysicalToVirtualAddress(config.GetPhysicalInputAddress()));
            u8* dest_pointer = Memory::GetPointer(Memory::PhysicalToVirtualAddress(config.GetPhysicalOutputAddress()));

            unsigned horizontal_scale = (config.scale_horizontally != 0) ? 2 : 1;
            unsigned vertical_scale = (config.scale_vertically != 0) ? 2 : 1;

            u32 output_width = config.output_width / horizontal_scale;
            u32 output_height = config.output_height / vertical_scale;

            if (config.raw_copy) {
                // Raw copies do not perform color conversion nor tiled->linear / linear->tiled conversions
                // TODO(Subv): Verify if raw copies perform scaling
                memcpy(dest_pointer, source_pointer, config.output_width * config.output_height * 
                        GPU::Regs::BytesPerPixel(config.output_format));
                
                LOG_TRACE(HW_GPU, "DisplayTriggerTransfer: 0x%08x bytes from 0x%08x(%ux%u)-> 0x%08x(%ux%u), flags 0x%08X, Raw copy",
                    config.output_height * output_width * GPU::Regs::BytesPerPixel(config.output_format),
                    config.GetPhysicalInputAddress(), config.input_width.Value(), config.input_height.Value(),
                    config.GetPhysicalOutputAddress(), config.output_width.Value(), config.output_height.Value(),
                    config.output_format.Value(), config.flags);

                GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PPF);
                break;
            }

            // TODO(Subv): Blend the pixels when horizontal / vertical scaling is enabled, 
            // right now we're just skipping the extra pixels.
            for (u32 y = 0; y < output_height; ++y) {
                for (u32 x = 0; x < output_width; ++x) {
                    struct {
                        int r, g, b, a;
                    } source_color = { 0, 0, 0, 0 };

                    u32 scaled_x = x * horizontal_scale;
                    u32 scaled_y = y * vertical_scale;

                    u32 dst_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.output_format);
                    u32 src_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.input_format);
                    u32 src_offset;
                    u32 dst_offset;

                    if (config.output_tiled) {
                        // Interpret the input as linear and the output as tiled
                        u32 coarse_y = y & ~7;
                        u32 stride = output_width * dst_bytes_per_pixel;

                        src_offset = (scaled_x + scaled_y * config.input_width) * src_bytes_per_pixel;
                        dst_offset = VideoCore::GetMortonOffset(x, y, dst_bytes_per_pixel) + coarse_y * stride;
                    } else {
                        // Interpret the input as tiled and the output as linear
                        u32 coarse_y = scaled_y & ~7;
                        u32 stride = config.input_width * src_bytes_per_pixel;

                        src_offset = VideoCore::GetMortonOffset(scaled_x, scaled_y, src_bytes_per_pixel) + coarse_y * stride;
                        dst_offset = (x + y * output_width) * dst_bytes_per_pixel;
                    }

                    switch (config.input_format) {
                    case Regs::PixelFormat::RGBA8:
                    {
                        u8* srcptr = source_pointer + src_offset;
                        source_color.r = srcptr[3]; // red
                        source_color.g = srcptr[2]; // green
                        source_color.b = srcptr[1]; // blue
                        source_color.a = srcptr[0]; // alpha
                        break;
                    }

                    case Regs::PixelFormat::RGB5A1:
                    {
                        u16 srcval = *(u16*)(source_pointer + src_offset);
                        source_color.r = Color::Convert5To8((srcval >> 11) & 0x1F); // red
                        source_color.g = Color::Convert5To8((srcval >>  6) & 0x1F); // green
                        source_color.b = Color::Convert5To8((srcval >>  1) & 0x1F); // blue
                        source_color.a = Color::Convert1To8(srcval & 0x1);          // alpha
                        break;
                    }

                    case Regs::PixelFormat::RGBA4:
                    {
                        u16 srcval = *(u16*)(source_pointer + src_offset);
                        source_color.r = Color::Convert4To8((srcval >> 12) & 0xF); // red
                        source_color.g = Color::Convert4To8((srcval >>  8) & 0xF); // green
                        source_color.b = Color::Convert4To8((srcval >>  4) & 0xF); // blue
                        source_color.a = Color::Convert4To8( srcval        & 0xF); // alpha
                        break;
                    }

                    default:
                        LOG_ERROR(HW_GPU, "Unknown source framebuffer format %x", config.input_format.Value());
                        break;
                    }

                    switch (config.output_format) {
                    case Regs::PixelFormat::RGBA8:
                    {
                        u8* dstptr = dest_pointer + dst_offset;
                        dstptr[3] = source_color.r;
                        dstptr[2] = source_color.g;
                        dstptr[1] = source_color.b;
                        dstptr[0] = source_color.a;
                        break;
                    }

                    case Regs::PixelFormat::RGB8:
                    {
                        u8* dstptr = dest_pointer + dst_offset;
                        dstptr[2] = source_color.r; // red
                        dstptr[1] = source_color.g; // green
                        dstptr[0] = source_color.b; // blue
                        break;
                    }

                    case Regs::PixelFormat::RGB5A1:
                    {
                        u16* dstptr = (u16*)(dest_pointer + dst_offset);
                        *dstptr = ((source_color.r >> 3) << 11) | ((source_color.g >> 3) << 6)
                                | ((source_color.b >> 3) <<  1) | ( source_color.a >> 7);
                        break;
                    }

                    case Regs::PixelFormat::RGBA4:
                    {
                        u16* dstptr = (u16*)(dest_pointer + dst_offset);
                        *dstptr = ((source_color.r >> 4) << 12) | ((source_color.g >> 4) << 8)
                                | ((source_color.b >> 4) <<  4) | ( source_color.a >> 4);
                        break;
                    }

                    default:
                        LOG_ERROR(HW_GPU, "Unknown destination framebuffer format %x", config.output_format.Value());
                        break;
                    }
                }
            }

            LOG_TRACE(HW_GPU, "DisplayTriggerTransfer: 0x%08x bytes from 0x%08x(%ux%u)-> 0x%08x(%ux%u), dst format %x, flags 0x%08X",
                      config.output_height * output_width * GPU::Regs::BytesPerPixel(config.output_format),
                      config.GetPhysicalInputAddress(), config.input_width.Value(), config.input_height.Value(),
                      config.GetPhysicalOutputAddress(), output_width, output_height,
                      config.output_format.Value(), config.flags);

            GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PPF);
        }
        break;
    }

    // Seems like writing to this register triggers processing
    case GPU_REG_INDEX(command_processor_config.trigger):
    {
        const auto& config = g_regs.command_processor_config;
        if (config.trigger & 1)
        {
            u32* buffer = (u32*)Memory::GetPointer(Memory::PhysicalToVirtualAddress(config.GetPhysicalAddress()));
            Pica::CommandProcessor::ProcessCommandList(buffer, config.size);
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
static void VBlankCallback(u64 userdata, int cycles_late) {
    frame_count++;
    last_skip_frame = g_skip_frame;
    g_skip_frame = (frame_count & Settings::values.frame_skip) != 0;

    // Swap buffers based on the frameskip mode, which is a little bit tricky. When
    // a frame is being skipped, nothing is being rendered to the internal framebuffer(s).
    // So, we should only swap frames if the last frame was rendered. The rules are:
    //  - If frameskip == 0 (disabled), always swap buffers
    //  - If frameskip == 1, swap buffers every other frame (starting from the first frame)
    //  - If frameskip > 1, swap buffers every frameskip^n frames (starting from the second frame)
    if ((((Settings::values.frame_skip != 1) ^ last_skip_frame) && last_skip_frame != g_skip_frame) || 
            Settings::values.frame_skip == 0) {
        VideoCore::g_renderer->SwapBuffers();
    }

    // Signal to GSP that GPU interrupt has occurred
    // TODO(yuriks): hwtest to determine if PDC0 is for the Top screen and PDC1 for the Sub
    // screen, or if both use the same interrupts and these two instead determine the
    // beginning and end of the VBlank period. If needed, split the interrupt firing into
    // two different intervals.
    GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PDC0);
    GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PDC1);

    // TODO(bunnei): Fake a DSP interrupt on each frame. This does not belong here, but
    // until we can emulate DSP interrupts, this is probably the only reasonable place to do
    // this. Certain games expect this to be periodically signaled.
    DSP_DSP::SignalInterrupt();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(frame_ticks - cycles_late, vblank_event);
}

/// Initialize hardware
void Init() {
    auto& framebuffer_top = g_regs.framebuffer_config[0];
    auto& framebuffer_sub = g_regs.framebuffer_config[1];

    // Setup default framebuffer addresses (located in VRAM)
    // .. or at least these are the ones used by system applets.
    // There's probably a smarter way to come up with addresses
    // like this which does not require hardcoding.
    framebuffer_top.address_left1  = 0x181E6000;
    framebuffer_top.address_left2  = 0x1822C800;
    framebuffer_top.address_right1 = 0x18273000;
    framebuffer_top.address_right2 = 0x182B9800;
    framebuffer_sub.address_left1  = 0x1848F000;
    framebuffer_sub.address_left2  = 0x184C7800;
    //framebuffer_sub.address_right1 = unknown;
    //framebuffer_sub.address_right2 = unknown;

    framebuffer_top.width = 240;
    framebuffer_top.height = 400;
    framebuffer_top.stride = 3 * 240;
    framebuffer_top.color_format = Regs::PixelFormat::RGB8;
    framebuffer_top.active_fb = 0;

    framebuffer_sub.width = 240;
    framebuffer_sub.height = 320;
    framebuffer_sub.stride = 3 * 240;
    framebuffer_sub.color_format = Regs::PixelFormat::RGB8;
    framebuffer_sub.active_fb = 0;

    frame_ticks = 268123480 / Settings::values.gpu_refresh_rate;
    last_skip_frame = false;
    g_skip_frame = false;

    vblank_event = CoreTiming::RegisterEvent("GPU::VBlankCallback", VBlankCallback);
    CoreTiming::ScheduleEvent(frame_ticks, vblank_event);

    LOG_DEBUG(HW_GPU, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    LOG_DEBUG(HW_GPU, "shutdown OK");
}

} // namespace
