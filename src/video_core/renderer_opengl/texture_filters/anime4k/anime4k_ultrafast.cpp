// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// modified from
// https://github.com/bloc97/Anime4K/blob/533cee5f7018d0e57ad2a26d76d43f13b9d8782a/glsl/Anime4K_Adaptive_v1.0RC2_UltraFast.glsl

// MIT License
//
// Copyright(c) 2019 bloc97
//
// Permission is hereby granted,
// free of charge,
// to any person obtaining a copy of this software and associated documentation
// files(the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies
// or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/anime4k/anime4k_ultrafast.h"

#include "shaders/refine.frag"
#include "shaders/refine.vert"
#include "shaders/tex_coord.vert"
#include "shaders/x_gradient.frag"
#include "shaders/y_gradient.frag"
#include "shaders/y_gradient.vert"

namespace OpenGL {

Anime4kUltrafast::Anime4kUltrafast(u16 scale_factor) : TextureFilterBase(scale_factor) {
    const OpenGLState cur_state = OpenGLState::GetCurState();
    const auto setup_temp_tex = [this](TempTex& texture, GLint internal_format, GLint format) {
        texture.fbo.Create();
        texture.tex.Create();
        state.draw.draw_framebuffer = texture.fbo.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture.tex.handle);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, internal_format, 1024 * internal_scale_factor,
                     1024 * internal_scale_factor, 0, format, GL_HALF_FLOAT, nullptr);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
                               texture.tex.handle, 0);
    };
    setup_temp_tex(LUMAD, GL_R16F, GL_RED);
    setup_temp_tex(XY, GL_RG16F, GL_RG);

    vao.Create();

    for (std::size_t idx = 0; idx < samplers.size(); ++idx) {
        samplers[idx].Create();
        state.texture_units[idx].sampler = samplers[idx].handle;
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_MIN_FILTER,
                            idx == 0 ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_MAG_FILTER,
                            idx == 0 ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    state.draw.vertex_array = vao.handle;

    gradient_x_program.Create(tex_coord_vert.data(), x_gradient_frag.data());
    gradient_y_program.Create(y_gradient_vert.data(), y_gradient_frag.data());
    refine_program.Create(refine_vert.data(), refine_frag.data());

    state.draw.shader_program = gradient_y_program.handle;
    state.Apply();
    glUniform1i(glGetUniformLocation(gradient_y_program.handle, "tex_input"), 2);

    state.draw.shader_program = refine_program.handle;
    state.Apply();
    glUniform1i(glGetUniformLocation(refine_program.handle, "LUMAD"), 1);
    glUniform1f(glGetUniformLocation(refine_program.handle, "final_scale"),
                static_cast<GLfloat>(internal_scale_factor) / scale_factor);

    cur_state.Apply();
}

void Anime4kUltrafast::Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect,
                              GLuint dst_tex, const Common::Rectangle<u32>& dst_rect,
                              GLuint read_fb_handle, GLuint draw_fb_handle) {
    const OpenGLState cur_state = OpenGLState::GetCurState();

    state.viewport = {static_cast<GLint>(src_rect.left * internal_scale_factor),
                      static_cast<GLint>(src_rect.bottom * internal_scale_factor),
                      static_cast<GLsizei>(src_rect.GetWidth() * internal_scale_factor),
                      static_cast<GLsizei>(src_rect.GetHeight() * internal_scale_factor)};
    state.texture_units[0].texture_2d = src_tex;
    state.draw.draw_framebuffer = XY.fbo.handle;
    state.draw.shader_program = gradient_x_program.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, LUMAD.tex.handle);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, XY.tex.handle);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // gradient y pass
    state.draw.draw_framebuffer = LUMAD.fbo.handle;
    state.draw.shader_program = gradient_y_program.handle;
    state.Apply();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // refine pass
    state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                      static_cast<GLsizei>(dst_rect.GetWidth()),
                      static_cast<GLsizei>(dst_rect.GetHeight())};
    state.draw.draw_framebuffer = draw_fb_handle;
    state.draw.shader_program = refine_program.handle;
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    cur_state.Apply();
}

} // namespace OpenGL
