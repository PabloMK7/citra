// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/color.h"
#include "core/core.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "video_core/renderer_software/renderer_software.h"

namespace SwRenderer {

RendererSoftware::RendererSoftware(Core::System& system, Frontend::EmuWindow& window)
    : VideoCore::RendererBase{system, window, nullptr}, memory{system.Memory()},
      rasterizer{std::make_unique<RasterizerSoftware>(system.Memory())} {}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SwapBuffers() {
    PrepareRenderTarget();
    EndFrame();
}

void RendererSoftware::PrepareRenderTarget() {
    for (int i : {0, 1, 2}) {
        const int fb_id = i == 2 ? 1 : 0;
        const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];
        auto& info = screen_infos[i];

        u32 lcd_color_addr =
            (fb_id == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (!color_fill.is_enabled) {
            const u32 old_width = std::exchange(info.width, framebuffer.width);
            const u32 old_height = std::exchange(info.height, framebuffer.height);
            if (framebuffer.width != old_width || framebuffer.height != old_height) [[unlikely]] {
                info.pixels.resize(framebuffer.width * framebuffer.height * 4);
            }
            CopyPixels(i);
        }
    }
}

void RendererSoftware::CopyPixels(int i) {
    const u32 fb_id = i == 2 ? 1 : 0;
    const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;
    const s32 bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    const u8* framebuffer_data = memory.GetPhysicalPointer(framebuffer_addr);

    const s32 stride = framebuffer.stride;
    const s32 height = framebuffer.height;
    ASSERT(stride * height != 0);

    u32 output_offset = 0;
    for (u32 y = 0; y < framebuffer.height; y++) {
        for (u32 x = 0; x < framebuffer.width; x++) {
            const u8* pixel = framebuffer_data + (y * stride + x) * bpp;
            const Common::Vec4 color = [&] {
                switch (framebuffer.color_format) {
                case GPU::Regs::PixelFormat::RGBA8:
                    return Common::Color::DecodeRGBA8(pixel);
                case GPU::Regs::PixelFormat::RGB8:
                    return Common::Color::DecodeRGB8(pixel);
                case GPU::Regs::PixelFormat::RGB565:
                    return Common::Color::DecodeRGB565(pixel);
                case GPU::Regs::PixelFormat::RGB5A1:
                    return Common::Color::DecodeRGB5A1(pixel);
                case GPU::Regs::PixelFormat::RGBA4:
                    return Common::Color::DecodeRGBA4(pixel);
                }
                UNREACHABLE();
            }();
            u8* dest = screen_infos[i].pixels.data() + output_offset;
            std::memcpy(dest, color.AsArray(), sizeof(color));
            output_offset += sizeof(color);
        }
    }
}

} // namespace SwRenderer
