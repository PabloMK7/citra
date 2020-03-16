// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/bicubic/bicubic.h"

#include "shaders/bicubic.frag"
#include "shaders/tex_coord.vert"

namespace OpenGL {

Bicubic::Bicubic(u16 scale_factor) : TextureFilterInterface(scale_factor) {
    program.Create(tex_coord_vert.data(), bicubic_frag.data());
    vao.Create();
    draw_fbo.Create();
    src_sampler.Create();

    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.draw.shader_program = program.handle;
    state.draw.draw_framebuffer = draw_fbo.handle;
    state.texture_units[0].sampler = src_sampler.handle;

    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Bicubic::scale(CachedSurface& surface, const Common::Rectangle<u32>& rect,
                    std::size_t buffer_offset) {
    const OpenGLState cur_state = OpenGLState::GetCurState();

    OGLTexture src_tex;
    src_tex.Create();
    state.texture_units[0].texture_2d = src_tex.handle;

    state.viewport = RectToViewport(rect);
    state.Apply();

    const FormatTuple tuple = GetFormatTuple(surface.pixel_format);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(surface.stride));
    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, rect.GetWidth(), rect.GetHeight(), 0,
                 tuple.format, tuple.type, &surface.gl_buffer[buffer_offset]);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           cur_state.texture_units[0].texture_2d, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, NULL, 0);

    cur_state.Apply();
}

} // namespace OpenGL
