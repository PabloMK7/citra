// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common.h"
#include "emu_window.h"
#include "log.h"

#include "core.h"

#include "video_core.h"
#include "renderer_base.h"
#include "renderer_opengl/renderer_opengl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow*      g_emu_window    = NULL;     ///< Frontend emulator window
RendererBase*   g_renderer      = NULL;     ///< Renderer plugin
int             g_current_frame = 0;

/// Start the video core
void Start() {
    if (g_emu_window == NULL) {
        ERROR_LOG(VIDEO, "VideoCore::Start called without calling Init()!");
    }
}

/// Initialize the video core
void Init(EmuWindow* emu_window) {
    g_emu_window = emu_window;
    g_emu_window->MakeCurrent();
    g_renderer = new RendererOpenGL();
    g_renderer->SetWindow(g_emu_window);
    g_renderer->Init();

    g_current_frame = 0;

    NOTICE_LOG(VIDEO, "initialized ok");
}

/// Shutdown the video core
void Shutdown() {
    delete g_renderer;
}

} // namespace
