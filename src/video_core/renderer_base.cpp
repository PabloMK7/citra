// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <memory>
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/swrasterizer.h"
#include "video_core/video_core.h"

void RendererBase::RefreshRasterizerSetting() {
    bool hw_renderer_enabled = VideoCore::g_hw_renderer_enabled;
    if (rasterizer == nullptr || opengl_rasterizer_active != hw_renderer_enabled) {
        opengl_rasterizer_active = hw_renderer_enabled;

        if (hw_renderer_enabled) {
            rasterizer = std::make_unique<RasterizerOpenGL>();
        } else {
            rasterizer = std::make_unique<VideoCore::SWRasterizer>();
        }
    }
}
