// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_interface.h"

namespace OpenGL {
class Bicubic : public TextureFilterInterface {
public:
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = "Bicubic";
        info.constructor = std::make_unique<Bicubic, u16>;
        return info;
    }

    Bicubic(u16 scale_factor);
    void scale(CachedSurface& surface, const Common::Rectangle<u32>& rect,
               std::size_t buffer_offset) override;

private:
    OpenGLState state{};
    OGLProgram program{};
    OGLVertexArray vao{};
    OGLFramebuffer draw_fbo{};
    OGLSampler src_sampler{};
};
} // namespace OpenGL
