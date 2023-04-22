// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scope_exit.h"
#include "video_core/renderer_opengl/gl_format_reinterpreter.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"

#include "video_core/host_shaders/format_reinterpreter/d24s8_to_rgba8_frag.h"
#include "video_core/host_shaders/format_reinterpreter/fullscreen_quad_vert.h"
#include "video_core/host_shaders/format_reinterpreter/rgba4_to_rgb5a1_frag.h"

namespace OpenGL {

RGBA4toRGB5A1::RGBA4toRGB5A1() {
    program.Create(HostShaders::FULLSCREEN_QUAD_VERT, HostShaders::RGBA4_TO_RGB5A1_FRAG);
    dst_size_loc = glGetUniformLocation(program.handle, "dst_size");
    src_size_loc = glGetUniformLocation(program.handle, "src_size");
    src_offset_loc = glGetUniformLocation(program.handle, "src_offset");
    vao.Create();
}

void RGBA4toRGB5A1::Reinterpret(Surface& source, Common::Rectangle<u32> src_rect, Surface& dest,
                                Common::Rectangle<u32> dst_rect) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.texture_units[0].texture_2d = source.Handle();
    state.draw.draw_framebuffer = draw_fbo.handle;
    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                      static_cast<GLsizei>(dst_rect.GetWidth()),
                      static_cast<GLsizei>(dst_rect.GetHeight())};
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dest.Handle(),
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    glUniform2i(dst_size_loc, dst_rect.GetWidth(), dst_rect.GetHeight());
    glUniform2i(src_size_loc, src_rect.GetWidth(), src_rect.GetHeight());
    glUniform2i(src_offset_loc, src_rect.left, src_rect.bottom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

ShaderD24S8toRGBA8::ShaderD24S8toRGBA8() {
    program.Create(HostShaders::FULLSCREEN_QUAD_VERT, HostShaders::D24S8_TO_RGBA8_FRAG);
    dst_size_loc = glGetUniformLocation(program.handle, "dst_size");
    src_size_loc = glGetUniformLocation(program.handle, "src_size");
    src_offset_loc = glGetUniformLocation(program.handle, "src_offset");
    vao.Create();

    auto state = OpenGLState::GetCurState();
    auto cur_program = state.draw.shader_program;
    state.draw.shader_program = program.handle;
    state.Apply();
    glUniform1i(glGetUniformLocation(program.handle, "stencil"), 1);
    state.draw.shader_program = cur_program;
    state.Apply();

    // Nvidia seem to be the only one to support D24S8 views, at least on windows
    // so for everyone else it will do an intermediate copy before running through the shader
    std::string_view vendor{reinterpret_cast<const char*>(glGetString(GL_VENDOR))};
    if (vendor.find("NVIDIA") != vendor.npos) {
        use_texture_view = true;
    } else {
        LOG_INFO(Render_OpenGL,
                 "Texture views are unsupported, reinterpretation will do intermediate copy");
        temp_tex.Create();
    }
}

void ShaderD24S8toRGBA8::Reinterpret(Surface& source, Common::Rectangle<u32> src_rect,
                                     Surface& dest, Common::Rectangle<u32> dst_rect) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.texture_units[0].texture_2d = source.Handle();

    if (use_texture_view) {
        temp_tex.Create();
        glActiveTexture(GL_TEXTURE1);
        glTextureView(temp_tex.handle, GL_TEXTURE_2D, source.Handle(), GL_DEPTH24_STENCIL8, 0, 1, 0,
                      1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else if (src_rect.top > temp_rect.top || src_rect.right > temp_rect.right) {
        temp_tex.Release();
        temp_tex.Create();
        state.texture_units[1].texture_2d = temp_tex.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE1);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, src_rect.right, src_rect.top);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        temp_rect = src_rect;
    }

    state.texture_units[1].texture_2d = temp_tex.handle;
    state.draw.draw_framebuffer = draw_fbo.handle;
    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                      static_cast<GLsizei>(dst_rect.GetWidth()),
                      static_cast<GLsizei>(dst_rect.GetHeight())};
    state.Apply();

    glActiveTexture(GL_TEXTURE1);
    if (!use_texture_view) {
        glCopyImageSubData(source.Handle(), GL_TEXTURE_2D, 0, src_rect.left, src_rect.bottom, 0,
                           temp_tex.handle, GL_TEXTURE_2D, 0, src_rect.left, src_rect.bottom, 0,
                           src_rect.GetWidth(), src_rect.GetHeight(), 1);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dest.Handle(),
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    glUniform2i(dst_size_loc, dst_rect.GetWidth(), dst_rect.GetHeight());
    glUniform2i(src_size_loc, src_rect.GetWidth(), src_rect.GetHeight());
    glUniform2i(src_offset_loc, src_rect.left, src_rect.bottom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (use_texture_view) {
        temp_tex.Release();
    }
}

} // namespace OpenGL
