// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {
class OpenGLState;

class TextureDownloaderES {
    static constexpr u16 max_size = 1024;

    OGLVertexArray vao;
    OGLFramebuffer read_fbo_generic;
    OGLFramebuffer depth32_fbo, depth16_fbo;
    OGLRenderbuffer r32ui_renderbuffer, r16_renderbuffer;
    struct ConversionShader {
        OGLProgram program;
        GLint lod_location{-1};
    } d24_r32ui_conversion_shader, d16_r16_conversion_shader, d24s8_r32ui_conversion_shader;
    OGLSampler sampler;

    void Test();
    GLuint ConvertDepthToColor(GLuint level, GLenum& format, GLenum& type, GLint height,
                               GLint width);

public:
    TextureDownloaderES(bool enable_depth_stencil);

    void GetTexImage(GLenum target, GLuint level, GLenum format, const GLenum type, GLint height,
                     GLint width, void* pixels);
};
} // namespace OpenGL
