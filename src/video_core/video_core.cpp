// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"

#include "video_core/pica.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow*                    g_emu_window = nullptr; ///< Frontend emulator window
std::unique_ptr<RendererBase> g_renderer;             ///< Renderer plugin

std::atomic<bool> g_hw_renderer_enabled;
std::atomic<bool> g_shader_jit_enabled;
std::atomic<bool> g_scaled_resolution_enabled;
std::atomic<bool> g_vsync_enabled;

/// Initialize the video core
bool Init(EmuWindow* emu_window) {
    Pica::Init();

    g_emu_window = emu_window;
    g_renderer = std::make_unique<RendererOpenGL>();
    g_renderer->SetWindow(g_emu_window);
    if (g_renderer->Init()) {
        LOG_DEBUG(Render, "initialized OK");
    } else {
        LOG_ERROR(Render, "initialization failed !");
        return false;
    }
    return true;
}

/// Shutdown the video core
void Shutdown() {
    Pica::Shutdown();

    g_renderer.reset();

    LOG_DEBUG(Render, "shutdown OK");
}

} // namespace
