// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <type_traits>

#include "common/color.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"

#include "core/settings.h"
#include "core/memory.h"
#include "core/core_timing.h"

#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/hid/hid.h"

#include "core/hw/hw.h"
#include "core/hw/gpu.h"

#include "core/tracer/recorder.h"

#include "video_core/command_processor.h"
#include "video_core/hwrasterizer_base.h"
#include "video_core/renderer_base.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

#include "video_core/debug_utils/debug_utils.h"


namespace GPU {

Regs g_regs;

/// True if the current frame was skipped
bool g_skip_frame;
/// 268MHz CPU clocks / 60Hz frames per second
const u64 frame_ticks = 268123480ull / 60;
/// Event id for CoreTiming
static int vblank_event;
/// Total number of frames drawn
static u64 frame_count;
/// True if the last frame was skipped
static bool last_skip_frame;

template <typename T>
inline void Read(T &var, const u32 raw_addr) {
    u32 addr = raw_addr - HW::VADDR_GPU;
    u32 index = addr / 4;

    // Reads other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds() || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_GPU, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, addr);
        return;
    }

    var = g_regs[addr / 4];
}

static Math::Vec4<u8> DecodePixel(Regs::PixelFormat input_format, const u8* src_pixel) {
    switch (input_format) {
    case Regs::PixelFormat::RGBA8:
        return Color::DecodeRGBA8(src_pixel);

    case Regs::PixelFormat::RGB8:
        return Color::DecodeRGB8(src_pixel);

    case Regs::PixelFormat::RGB565:
        return Color::DecodeRGB565(src_pixel);

    case Regs::PixelFormat::RGB5A1:
        return Color::DecodeRGB5A1(src_pixel);

    case Regs::PixelFormat::RGBA4:
        return Color::DecodeRGBA4(src_pixel);

    default:
        LOG_ERROR(HW_GPU, "Unknown source framebuffer format %x", input_format);
        return {0, 0, 0, 0};
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    addr -= HW::VADDR_GPU;
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

        if (config.trigger) {
            if (config.address_start) { // Some games pass invalid values here
                u8* start = Memory::GetPhysicalPointer(config.GetStartAddress());
                u8* end = Memory::GetPhysicalPointer(config.GetEndAddress());

                if (config.fill_24bit) {
                    // fill with 24-bit values
                    for (u8* ptr = start; ptr < end; ptr += 3) {
                        ptr[0] = config.value_24bit_r;
                        ptr[1] = config.value_24bit_g;
                        ptr[2] = config.value_24bit_b;
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

                if (!is_second_filler) {
                    GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PSC0);
                } else {
                    GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PSC1);
                }

                VideoCore::g_renderer->hw_rasterizer->NotifyFlush(config.GetStartAddress(), config.GetEndAddress() - config.GetStartAddress());
            }

            // Reset "trigger" flag and set the "finish" flag
            // NOTE: This was confirmed to happen on hardware even if "address_start" is zero.
            config.trigger = 0;
            config.finished = 1;
        }
        break;
    }

    case GPU_REG_INDEX(display_transfer_config.trigger):
    {
        const auto& config = g_regs.display_transfer_config;
        if (config.trigger & 1) {

            if (Pica::g_debug_context)
                Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::IncomingDisplayTransfer, nullptr);

            u8* src_pointer = Memory::GetPhysicalPointer(config.GetPhysicalInputAddress());
            u8* dst_pointer = Memory::GetPhysicalPointer(config.GetPhysicalOutputAddress());

            if (config.scaling > config.ScaleXY) {
                LOG_CRITICAL(HW_GPU, "Unimplemented display transfer scaling mode %u", config.scaling.Value());
                UNIMPLEMENTED();
                break;
            }

            if (config.output_tiled &&
                    (config.scaling == config.ScaleXY || config.scaling == config.ScaleX)) {
                LOG_CRITICAL(HW_GPU, "Scaling is only implemented on tiled input");
                UNIMPLEMENTED();
                break;
            }

            bool horizontal_scale = config.scaling != config.NoScale;
            bool vertical_scale = config.scaling == config.ScaleXY;

            u32 output_width = config.output_width >> horizontal_scale;
            u32 output_height = config.output_height >> vertical_scale;

            u32 input_size = config.input_width * config.input_height * GPU::Regs::BytesPerPixel(config.input_format);
            u32 output_size = output_width * output_height * GPU::Regs::BytesPerPixel(config.output_format);

            VideoCore::g_renderer->hw_rasterizer->NotifyPreRead(config.GetPhysicalInputAddress(), input_size);

            if (config.raw_copy) {
                // Raw copies do not perform color conversion nor tiled->linear / linear->tiled conversions
                // TODO(Subv): Verify if raw copies perform scaling
                memcpy(dst_pointer, src_pointer, output_size);

                LOG_TRACE(HW_GPU, "DisplayTriggerTransfer: 0x%08x bytes from 0x%08x(%ux%u)-> 0x%08x(%ux%u), output format: %x, flags 0x%08X, Raw copy",
                    output_size,
                    config.GetPhysicalInputAddress(), config.input_width.Value(), config.input_height.Value(),
                    config.GetPhysicalOutputAddress(), config.output_width.Value(), config.output_height.Value(),
                    config.output_format.Value(), config.flags);

                GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PPF);

                VideoCore::g_renderer->hw_rasterizer->NotifyFlush(config.GetPhysicalOutputAddress(), output_size);
                break;
            }

            for (u32 y = 0; y < output_height; ++y) {
                for (u32 x = 0; x < output_width; ++x) {
                    Math::Vec4<u8> src_color;

                    // Calculate the [x,y] position of the input image
                    // based on the current output position and the scale
                    u32 input_x = x << horizontal_scale;
                    u32 input_y = y << vertical_scale;

                    if (config.flip_vertically) {
                        // Flip the y value of the output data,
                        // we do this after calculating the [x,y] position of the input image
                        // to account for the scaling options.
                        y = output_height - y - 1;
                    }

                    u32 dst_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.output_format);
                    u32 src_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.input_format);
                    u32 src_offset;
                    u32 dst_offset;

                    if (config.output_tiled) {
                        if (!config.dont_swizzle) {
                            // Interpret the input as linear and the output as tiled
                            u32 coarse_y = y & ~7;
                            u32 stride = output_width * dst_bytes_per_pixel;

                            src_offset = (input_x + input_y * config.input_width) * src_bytes_per_pixel;
                            dst_offset = VideoCore::GetMortonOffset(x, y, dst_bytes_per_pixel) + coarse_y * stride;
                        } else {
                           // Both input and output are linear
                            src_offset = (input_x + input_y * config.input_width) * src_bytes_per_pixel;
                            dst_offset = (x + y * output_width) * dst_bytes_per_pixel;
                        }
                    } else {
                        if (!config.dont_swizzle) {
                            // Interpret the input as tiled and the output as linear
                            u32 coarse_y = input_y & ~7;
                            u32 stride = config.input_width * src_bytes_per_pixel;

                            src_offset = VideoCore::GetMortonOffset(input_x, input_y, src_bytes_per_pixel) + coarse_y * stride;
                            dst_offset = (x + y * output_width) * dst_bytes_per_pixel;
                        } else {
                            // Both input and output are tiled
                            u32 out_coarse_y = y & ~7;
                            u32 out_stride = output_width * dst_bytes_per_pixel;

                            u32 in_coarse_y = input_y & ~7;
                            u32 in_stride = config.input_width * src_bytes_per_pixel;

                            src_offset = VideoCore::GetMortonOffset(input_x, input_y, src_bytes_per_pixel) + in_coarse_y * in_stride;
                            dst_offset = VideoCore::GetMortonOffset(x, y, dst_bytes_per_pixel) + out_coarse_y * out_stride;
                        }
                    }

                    const u8* src_pixel = src_pointer + src_offset;
                    src_color = DecodePixel(config.input_format, src_pixel);
                    if (config.scaling == config.ScaleX) {
                        Math::Vec4<u8> pixel = DecodePixel(config.input_format, src_pixel + src_bytes_per_pixel);
                        src_color = ((src_color + pixel) / 2).Cast<u8>();
                    } else if (config.scaling == config.ScaleXY) {
                        Math::Vec4<u8> pixel1 = DecodePixel(config.input_format, src_pixel + 1 * src_bytes_per_pixel);
                        Math::Vec4<u8> pixel2 = DecodePixel(config.input_format, src_pixel + 2 * src_bytes_per_pixel);
                        Math::Vec4<u8> pixel3 = DecodePixel(config.input_format, src_pixel + 3 * src_bytes_per_pixel);
                        src_color = (((src_color + pixel1) + (pixel2 + pixel3)) / 4).Cast<u8>();
                    }

                    u8* dst_pixel = dst_pointer + dst_offset;
                    switch (config.output_format) {
                    case Regs::PixelFormat::RGBA8:
                        Color::EncodeRGBA8(src_color, dst_pixel);
                        break;

                    case Regs::PixelFormat::RGB8:
                        Color::EncodeRGB8(src_color, dst_pixel);
                        break;

                    case Regs::PixelFormat::RGB565:
                        Color::EncodeRGB565(src_color, dst_pixel);
                        break;

                    case Regs::PixelFormat::RGB5A1:
                        Color::EncodeRGB5A1(src_color, dst_pixel);
                        break;

                    case Regs::PixelFormat::RGBA4:
                        Color::EncodeRGBA4(src_color, dst_pixel);
                        break;

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

            g_regs.display_transfer_config.trigger = 0;
            GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::PPF);

            VideoCore::g_renderer->hw_rasterizer->NotifyFlush(config.GetPhysicalOutputAddress(), output_size);
        }
        break;
    }

    // Seems like writing to this register triggers processing
    case GPU_REG_INDEX(command_processor_config.trigger):
    {
        const auto& config = g_regs.command_processor_config;
        if (config.trigger & 1)
        {
            u32* buffer = (u32*)Memory::GetPhysicalPointer(config.GetPhysicalAddress());

            if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
                Pica::g_debug_context->recorder->MemoryAccessed((u8*)buffer, config.size * sizeof(u32), config.GetPhysicalAddress());
            }

            Pica::CommandProcessor::ProcessCommandList(buffer, config.size);

            g_regs.command_processor_config.trigger = 0;
        }
        break;
    }

    default:
        break;
    }

    // Notify tracer about the register write
    // This is happening *after* handling the write to make sure we properly catch all memory reads.
    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        // addr + GPU VBase - IO VBase + IO PBase
        Pica::g_debug_context->recorder->RegisterWritten<T>(addr + 0x1EF00000 - 0x1EC00000 + 0x10100000, data);
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

    // Check for user input updates
    Service::HID::Update();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(frame_ticks - cycles_late, vblank_event);
}

/// Initialize hardware
void Init() {
    memset(&g_regs, 0, sizeof(g_regs));

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

    last_skip_frame = false;
    g_skip_frame = false;
    frame_count = 0;

    vblank_event = CoreTiming::RegisterEvent("GPU::VBlankCallback", VBlankCallback);
    CoreTiming::ScheduleEvent(frame_ticks, vblank_event);

    LOG_DEBUG(HW_GPU, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    LOG_DEBUG(HW_GPU, "shutdown OK");
}

} // namespace
