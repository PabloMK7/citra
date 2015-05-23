// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/emu_window.h"

#include "core/core.h"
#include "core/settings.h"

#include "video_core.h"
#include "renderer_base.h"
#include "renderer_opengl/renderer_opengl.h"

#include "pica.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow*      g_emu_window    = nullptr;     ///< Frontend emulator window
RendererBase*   g_renderer      = nullptr;     ///< Renderer plugin

std::atomic<bool> g_hw_renderer_enabled;

/// Initialize the video core
void Init(EmuWindow* emu_window) {
    Pica::Init();

    g_emu_window = emu_window;
    g_renderer = new RendererOpenGL();
    g_renderer->SetWindow(g_emu_window);
    g_renderer->Init();

    LOG_DEBUG(Render, "initialized OK");
}

/// Shutdown the video core
void Shutdown() {
    Pica::Shutdown();

    delete g_renderer;

    LOG_DEBUG(Render, "shutdown OK");
}

} // namespace
