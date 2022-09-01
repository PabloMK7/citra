// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"

namespace OpenGL {

class ScaleForce : public TextureFilterBase {
public:
    static constexpr std::string_view NAME = "ScaleForce";

    explicit ScaleForce(u16 scale_factor);
    void Filter(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) override;

private:
    OpenGLState state{};
    OGLProgram program{};
    OGLVertexArray vao{};
    OGLSampler src_sampler{};
};

} // namespace OpenGL
