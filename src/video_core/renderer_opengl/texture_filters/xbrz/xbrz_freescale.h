// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"

namespace OpenGL {

class XbrzFreescale : public TextureFilterBase {
public:
    static constexpr std::string_view NAME = "xBRZ freescale";

    explicit XbrzFreescale(u16 scale_factor);
    void Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                const Common::Rectangle<u32>& dst_rect, GLuint read_fb_handle,
                GLuint draw_fb_handle) override;

private:
    OpenGLState state{};
    OGLProgram program{};
    OGLVertexArray vao{};
    OGLSampler src_sampler{};
};
} // namespace OpenGL
