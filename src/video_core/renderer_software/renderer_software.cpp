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
      rasterizer{system.Memory()} {}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SwapBuffers() {
    PrepareRenderTarget();
    EndFrame();
}

void RendererSoftware::PrepareRenderTarget() {
    for (u32 i = 0; i < 3; i++) {
        const int fb_id = i == 2 ? 1 : 0;

        u32 lcd_color_addr =
            (fb_id == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (!color_fill.is_enabled) {
            LoadFBToScreenInfo(i);
        }
    }
}

void RendererSoftware::LoadFBToScreenInfo(int i) {
    const u32 fb_id = i == 2 ? 1 : 0;
    const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];
    auto& info = screen_infos[i];

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;
    const s32 bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    const u8* framebuffer_data = memory.GetPhysicalPointer(framebuffer_addr);

    const s32 pixel_stride = framebuffer.stride / bpp;
    info.height = framebuffer.height;
    info.width = pixel_stride;
    info.pixels.resize(info.width * info.height * 4);

    for (u32 y = 0; y < info.height; y++) {
        for (u32 x = 0; x < info.width; x++) {
            const u8* pixel = framebuffer_data + (y * pixel_stride + pixel_stride - x) * bpp;
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
            const u32 output_offset = (x * info.height + y) * 4;
            u8* dest = info.pixels.data() + output_offset;
            std::memcpy(dest, color.AsArray(), sizeof(color));
        }
    }
}

} // namespace SwRenderer
