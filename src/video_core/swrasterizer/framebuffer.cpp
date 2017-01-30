// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/color.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "core/hw/gpu.h"
#include "core/memory.h"
#include "video_core/pica_state.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/swrasterizer/framebuffer.h"
#include "video_core/utils.h"

namespace Pica {
namespace Rasterizer {

void DrawPixel(int x, int y, const Math::Vec4<u8>& color) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetColorBufferPhysicalAddress();

    // Similarly to textures, the render framebuffer is laid out from bottom to top, too.
    // NOTE: The framebuffer height register contains the actual FB height minus one.
    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel =
        GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(framebuffer.color_format.Value()));
    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) +
                     coarse_y * framebuffer.width * bytes_per_pixel;
    u8* dst_pixel = Memory::GetPhysicalPointer(addr) + dst_offset;

    switch (framebuffer.color_format) {
    case FramebufferRegs::ColorFormat::RGBA8:
        Color::EncodeRGBA8(color, dst_pixel);
        break;

    case FramebufferRegs::ColorFormat::RGB8:
        Color::EncodeRGB8(color, dst_pixel);
        break;

    case FramebufferRegs::ColorFormat::RGB5A1:
        Color::EncodeRGB5A1(color, dst_pixel);
        break;

    case FramebufferRegs::ColorFormat::RGB565:
        Color::EncodeRGB565(color, dst_pixel);
        break;

    case FramebufferRegs::ColorFormat::RGBA4:
        Color::EncodeRGBA4(color, dst_pixel);
        break;

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x",
                     framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }
}

const Math::Vec4<u8> GetPixel(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetColorBufferPhysicalAddress();

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel =
        GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(framebuffer.color_format.Value()));
    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) +
                     coarse_y * framebuffer.width * bytes_per_pixel;
    u8* src_pixel = Memory::GetPhysicalPointer(addr) + src_offset;

    switch (framebuffer.color_format) {
    case FramebufferRegs::ColorFormat::RGBA8:
        return Color::DecodeRGBA8(src_pixel);

    case FramebufferRegs::ColorFormat::RGB8:
        return Color::DecodeRGB8(src_pixel);

    case FramebufferRegs::ColorFormat::RGB5A1:
        return Color::DecodeRGB5A1(src_pixel);

    case FramebufferRegs::ColorFormat::RGB565:
        return Color::DecodeRGB565(src_pixel);

    case FramebufferRegs::ColorFormat::RGBA4:
        return Color::DecodeRGBA4(src_pixel);

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x",
                     framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }

    return {0, 0, 0, 0};
}

u32 GetDepth(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = FramebufferRegs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* src_pixel = depth_buffer + src_offset;

    switch (framebuffer.depth_format) {
    case FramebufferRegs::DepthFormat::D16:
        return Color::DecodeD16(src_pixel);
    case FramebufferRegs::DepthFormat::D24:
        return Color::DecodeD24(src_pixel);
    case FramebufferRegs::DepthFormat::D24S8:
        return Color::DecodeD24S8(src_pixel).x;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
        UNIMPLEMENTED();
        return 0;
    }
}

u8 GetStencil(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Pica::FramebufferRegs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* src_pixel = depth_buffer + src_offset;

    switch (framebuffer.depth_format) {
    case FramebufferRegs::DepthFormat::D24S8:
        return Color::DecodeD24S8(src_pixel).y;

    default:
        LOG_WARNING(
            HW_GPU,
            "GetStencil called for function which doesn't have a stencil component (format %u)",
            framebuffer.depth_format);
        return 0;
    }
}

void SetDepth(int x, int y, u32 value) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = FramebufferRegs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* dst_pixel = depth_buffer + dst_offset;

    switch (framebuffer.depth_format) {
    case FramebufferRegs::DepthFormat::D16:
        Color::EncodeD16(value, dst_pixel);
        break;

    case FramebufferRegs::DepthFormat::D24:
        Color::EncodeD24(value, dst_pixel);
        break;

    case FramebufferRegs::DepthFormat::D24S8:
        Color::EncodeD24X8(value, dst_pixel);
        break;

    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
        UNIMPLEMENTED();
        break;
    }
}

void SetStencil(int x, int y, u8 value) {
    const auto& framebuffer = g_state.regs.framebuffer.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Pica::FramebufferRegs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* dst_pixel = depth_buffer + dst_offset;

    switch (framebuffer.depth_format) {
    case Pica::FramebufferRegs::DepthFormat::D16:
    case Pica::FramebufferRegs::DepthFormat::D24:
        // Nothing to do
        break;

    case Pica::FramebufferRegs::DepthFormat::D24S8:
        Color::EncodeX24S8(value, dst_pixel);
        break;

    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
        UNIMPLEMENTED();
        break;
    }
}

u8 PerformStencilAction(FramebufferRegs::StencilAction action, u8 old_stencil, u8 ref) {
    switch (action) {
    case FramebufferRegs::StencilAction::Keep:
        return old_stencil;

    case FramebufferRegs::StencilAction::Zero:
        return 0;

    case FramebufferRegs::StencilAction::Replace:
        return ref;

    case FramebufferRegs::StencilAction::Increment:
        // Saturated increment
        return std::min<u8>(old_stencil, 254) + 1;

    case FramebufferRegs::StencilAction::Decrement:
        // Saturated decrement
        return std::max<u8>(old_stencil, 1) - 1;

    case FramebufferRegs::StencilAction::Invert:
        return ~old_stencil;

    case FramebufferRegs::StencilAction::IncrementWrap:
        return old_stencil + 1;

    case FramebufferRegs::StencilAction::DecrementWrap:
        return old_stencil - 1;

    default:
        LOG_CRITICAL(HW_GPU, "Unknown stencil action %x", (int)action);
        UNIMPLEMENTED();
        return 0;
    }
}

} // namespace Rasterizer
} // namespace Pica
