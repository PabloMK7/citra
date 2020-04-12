// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"

namespace OpenGL {

class Anime4kUltrafast : public TextureFilterBase {
public:
    static constexpr std::string_view NAME = "Anime4K Ultrafast";

    explicit Anime4kUltrafast(u16 scale_factor);
    void Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                const Common::Rectangle<u32>& dst_rect, GLuint read_fb_handle,
                GLuint draw_fb_handle) override;

private:
    static constexpr u8 internal_scale_factor = 2;

    OpenGLState state{};

    OGLVertexArray vao;

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
