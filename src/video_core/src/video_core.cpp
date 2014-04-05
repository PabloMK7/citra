/**
 * Copyright (C) 2014 Citra Emulator
 *
 * @file    video_core.cpp
 * @author  bunnei
 * @date    2014-04-05
 * @brief   Main module for new video core
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"
#include "emu_window.h"
#include "log.h"

#include "core.h"
#include "video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow*      g_emu_window    = NULL;     ///< Frontend emulator window
RendererBase*   g_renderer      = NULL;     ///< Renderer plugin
int             g_current_frame = 0;

int VideoEntry(void*) {
    if (g_emu_window == NULL) {
        ERROR_LOG(VIDEO, "VideoCore::VideoEntry called without calling Init()!");
    }
    g_emu_window->MakeCurrent();
    //for(;;) {
    //    gp::Fifo_DecodeCommand();
    //}
    return 0;
}

/// Start the video core
void Start() {
    if (g_emu_window == NULL) {
        ERROR_LOG(VIDEO, "VideoCore::Start called without calling Init()!");
    }
    //if (common::g_config->enable_multicore()) {
    //    g_emu_window->DoneCurrent();
    //    g_video_thread = SDL_CreateThread(VideoEntry, NULL, NULL);
    //    if (g_video_thread == NULL) {
    //        LOG_ERROR(TVIDEO, "Unable to create thread: %s... Exiting\n", SDL_GetError());
    //        exit(1);
    //    }
    //}
}

/// Initialize the video core
void Init(EmuWindow* emu_window) {
    g_emu_window = emu_window;
    //g_renderer = new RendererGL3();
    //g_renderer->SetWindow(g_emu_window);
    //g_renderer->Init();

    //gp::Fifo_Init();
    //gp::VertexManager_Init();
    //gp::VertexLoader_Init();
    //gp::BP_Init();
    //gp::CP_Init();
    //gp::XF_Init();

    g_current_frame = 0;

    NOTICE_LOG(VIDEO, "initialized ok");
}

/// Shutdown the video core
void Shutdown() {
    //delete g_renderer;
}

} // namespace
