// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_rect.h>
#include "citra/emu_window/emu_window_sdl2_sw.h"
#include "common/color.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "core/hw/gpu.h"
#include "core/memory.h"
#include "video_core/video_core.h"

class DummyContext : public Frontend::GraphicsContext {};

EmuWindow_SDL2_SW::EmuWindow_SDL2_SW(bool fullscreen, bool is_secondary)
    : EmuWindow_SDL2{is_secondary} {
    std::string window_title = fmt::format("Citra {} | {}-{}", Common::g_build_fullname,
                                           Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight,
                         SDL_WINDOW_SHOWN);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window: {}", SDL_GetError());
        exit(1);
    }

    window_surface = SDL_GetWindowSurface(render_window);
    renderer = SDL_CreateSoftwareRenderer(window_surface);

    if (renderer == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 software renderer: {}", SDL_GetError());
        exit(1);
    }

    if (fullscreen) {
        Fullscreen();
    }

    render_window_id = SDL_GetWindowID(render_window);

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
}

EmuWindow_SDL2_SW::~EmuWindow_SDL2_SW() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(render_window);
}

std::unique_ptr<Frontend::GraphicsContext> EmuWindow_SDL2_SW::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}

void EmuWindow_SDL2_SW::Present() {
    const auto layout{Layout::DefaultFrameLayout(
        Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight, false, false)};

    while (IsOpen()) {
        SDL_SetRenderDrawColor(renderer,
                               static_cast<Uint8>(Settings::values.bg_red.GetValue() * 255),
                               static_cast<Uint8>(Settings::values.bg_green.GetValue() * 255),
                               static_cast<Uint8>(Settings::values.bg_blue.GetValue() * 255), 0xFF);
        SDL_RenderClear(renderer);

        const auto draw_screen = [&](int fb_id) {
            const auto dst_rect = fb_id == 0 ? layout.top_screen : layout.bottom_screen;
            SDL_Rect sdl_rect{static_cast<int>(dst_rect.left), static_cast<int>(dst_rect.top),
                              static_cast<int>(dst_rect.GetWidth()),
                              static_cast<int>(dst_rect.GetHeight())};
            SDL_Surface* screen = LoadFramebuffer(fb_id);
            SDL_BlitSurface(screen, nullptr, window_surface, &sdl_rect);
            SDL_FreeSurface(screen);
        };

        draw_screen(0);
        draw_screen(1);

        SDL_RenderPresent(renderer);
        SDL_UpdateWindowSurface(render_window);
    }
}

SDL_Surface* EmuWindow_SDL2_SW::LoadFramebuffer(int fb_id) {
    const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];
    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;

    Memory::RasterizerFlushRegion(framebuffer_addr, framebuffer.stride * framebuffer.height);
    const u8* framebuffer_data = VideoCore::g_memory->GetPhysicalPointer(framebuffer_addr);

    const int width = framebuffer.height;
    const int height = framebuffer.width;
    const int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);

    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, width, height, 0, SDL_PIXELFORMAT_ABGR8888);
    SDL_LockSurface(surface);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const u8* pixel = framebuffer_data + (x * height + height - y) * bpp;
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

            u8* dst_pixel = reinterpret_cast<u8*>(surface->pixels) + (y * width + x) * 4;
            std::memcpy(dst_pixel, color.AsArray(), sizeof(color));
        }
    }
    SDL_UnlockSurface(surface);
    return surface;
}
