// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/emu_window.h"
#include "common/log.h"

#include "core/core.h"

#include "video_core/video_core.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow*      g_emu_window    = nullptr;     ///< Frontend emulator window
RendererBase*   g_renderer      = nullptr;     ///< Renderer plugin
int             g_current_frame = 0;

/// Initialize the video core
void Init(EmuWindow* emu_window) {
    g_emu_window = emu_window;
    g_renderer = new RendererOpenGL();
    g_renderer->SetWindow(g_emu_window);
    g_renderer->Init();

    g_current_frame = 0;

    NOTICE_LOG(VIDEO, "initialized OK");
}

/// Shutdown the video core
void Shutdown() {
    delete g_renderer;
    NOTICE_LOG(VIDEO, "shutdown OK");
}

} // namespace
