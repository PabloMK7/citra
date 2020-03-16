// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {

class Anime4kUltrafast : public TextureFilterInterface {
public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "Anime4K Ultrafast";
        info.clamp_scale = {2, 2};
        info.constructor = std::make_unique<Anime4kUltrafast, u16>;
        return info;
    }

    Anime4kUltrafast(u16 scale_factor);
    void scale(CachedSurface& surface, const Common::Rectangle<u32>& rect,
               std::size_t buffer_offset) override;

private:
    OpenGLState state{};

    OGLVertexArray vao;
    OGLFramebuffer out_fbo;

    struct TempTex {
        OGLTexture tex;
        OGLFramebuffer fbo;
    };
    TempTex LUMAD;
    TempTex XY;

    std::array<OGLSampler, 3> samplers;

    OGLProgram gradient_x_program, gradient_y_program, refine_program;
};

} // namespace OpenGL
