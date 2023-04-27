// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"

namespace VideoCore {
struct Extent;
struct TextureBlit;
} // namespace VideoCore

namespace OpenGL {

class TextureRuntime;
class Surface;

class BlitHelper {
public:
    BlitHelper(TextureRuntime& runtime);
    ~BlitHelper();

    bool Filter(Surface& surface, const VideoCore::TextureBlit& blit);

private:
    void FilterAnime4K(Surface& surface, const VideoCore::TextureBlit& blit);

    void FilterBicubic(Surface& surface, const VideoCore::TextureBlit& blit);

    void FilterNearest(Surface& surface, const VideoCore::TextureBlit& blit);

    void FilterScaleForce(Surface& surface, const VideoCore::TextureBlit& blit);

    void FilterXbrz(Surface& surface, const VideoCore::TextureBlit& blit);

    void SetParams(OGLProgram& program, const VideoCore::Extent& src_extent,
                   Common::Rectangle<u32> src_rect);

    void Draw(OGLProgram& program, GLuint dst_tex, GLuint dst_fbo, u32 dst_level,
              Common::Rectangle<u32> dst_rect);

private:
    TextureRuntime& runtime;
    OGLVertexArray vao;
    OpenGLState state;
    OGLFramebuffer filter_fbo;
    OGLSampler linear_sampler;
    OGLSampler nearest_sampler;

    OGLProgram bicubic_program;
    OGLProgram nearest_program;
    OGLProgram scale_force_program;
    OGLProgram xbrz_program;
    OGLProgram gradient_x_program;
    OGLProgram gradient_y_program;
    OGLProgram refine_program;
};

} // namespace OpenGL
