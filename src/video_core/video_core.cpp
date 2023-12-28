// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/gpu.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/renderer_software/renderer_software.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<RendererBase> CreateRenderer(Frontend::EmuWindow& emu_window,
                                             Frontend::EmuWindow* secondary_window,
                                             Pica::PicaCore& pica, Core::System& system) {
    const Settings::GraphicsAPI graphics_api = Settings::values.graphics_api.GetValue();
    switch (graphics_api) {
    case Settings::GraphicsAPI::Software:
        return std::make_unique<SwRenderer::RendererSoftware>(system, pica, emu_window);
    case Settings::GraphicsAPI::Vulkan:
        return std::make_unique<Vulkan::RendererVulkan>(system, pica, emu_window, secondary_window);
    case Settings::GraphicsAPI::OpenGL:
        return std::make_unique<OpenGL::RendererOpenGL>(system, pica, emu_window, secondary_window);
    default:
        LOG_CRITICAL(Render, "Unknown graphics API {}, using OpenGL", graphics_api);
        return std::make_unique<OpenGL::RendererOpenGL>(system, pica, emu_window, secondary_window);
    }
}

} // namespace VideoCore
