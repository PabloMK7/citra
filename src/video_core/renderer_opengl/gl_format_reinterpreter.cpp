// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/scope_exit.h"
#include "video_core/renderer_opengl/gl_format_reinterpreter.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"

namespace OpenGL {

using PixelFormat = SurfaceParams::PixelFormat;

class RGBA4toRGB5A1 final : public FormatReinterpreterBase {
public:
    RGBA4toRGB5A1() {
        constexpr std::string_view vs_source = R"(
out vec2 dst_coord;

uniform mediump ivec2 dst_size;

const vec2 vertices[4] =
    vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    dst_coord = (vertices[gl_VertexID] / 2.0 + 0.5) * vec2(dst_size);
}
)";

        constexpr std::string_view fs_source = R"(
in mediump vec2 dst_coord;

out lowp vec4 frag_color;

uniform lowp sampler2D source;
uniform mediump ivec2 dst_size;
uniform mediump ivec2 src_size;
uniform mediump ivec2 src_offset;

void main() {
    mediump ivec2 tex_coord;
    if (src_size == dst_size) {
        tex_coord = ivec2(dst_coord);
    } else {
        highp int tex_index = int(dst_coord.y) * dst_size.x + int(dst_coord.x);
        mediump int y = tex_index / src_size.x;
        tex_coord = ivec2(tex_index - y * src_size.x, y);
    }
    tex_coord -= src_offset;

    lowp ivec4 rgba4 = ivec4(texelFetch(source, tex_coord, 0) * (exp2(4.0) - 1.0));
    lowp ivec3 rgb5 =
        ((rgba4.rgb << ivec3(1, 2, 3)) | (rgba4.gba >> ivec3(3, 2, 1))) & 0x1F;
    frag_color = vec4(vec3(rgb5) / (exp2(5.0) - 1.0), rgba4.a & 0x01);
}
)";

        program.Create(vs_source.data(), fs_source.data());
        dst_size_loc = glGetUniformLocation(program.handle, "dst_size");
        src_size_loc = glGetUniformLocation(program.handle, "src_size");
        src_offset_loc = glGetUniformLocation(program.handle, "src_offset");
        vao.Create();
    }

    void Reinterpret(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint read_fb_handle,
                     GLuint dst_tex, const Common::Rectangle<u32>& dst_rect,
                     GLuint draw_fb_handle) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        SCOPE_EXIT({ prev_state.Apply(); });

        OpenGLState state;
        state.texture_units[0].texture_2d = src_tex;
        state.draw.draw_framebuffer = draw_fb_handle;
        state.draw.shader_program = program.handle;
        state.draw.vertex_array = vao.handle;
        state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                          static_cast<GLsizei>(dst_rect.GetWidth()),
                          static_cast<GLsizei>(dst_rect.GetHeight())};
        state.Apply();

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex,
                               0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        glUniform2i(dst_size_loc, dst_rect.GetWidth(), dst_rect.GetHeight());
        glUniform2i(src_size_loc, src_rect.GetWidth(), src_rect.GetHeight());
        glUniform2i(src_offset_loc, src_rect.left, src_rect.bottom);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

private:
    OGLProgram program;
    GLint dst_size_loc{-1}, src_size_loc{-1}, src_offset_loc{-1};
    OGLVertexArray vao;
};

class PixelBufferD24S8toABGR final : public FormatReinterpreterBase {
public:
    PixelBufferD24S8toABGR() {
        attributeless_vao.Create();
        d24s8_abgr_buffer.Create();
        d24s8_abgr_buffer_size = 0;

        constexpr std::string_view vs_source = R"(
const vec2 vertices[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0),
                                 vec2(-1.0,  1.0), vec2(1.0,  1.0));
void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
}
)";

        std::string fs_source = GLES ? fragment_shader_precision_OES : "";
        fs_source += R"(
uniform samplerBuffer tbo;
uniform vec2 tbo_size;
uniform vec4 viewport;

out vec4 color;

void main() {
    vec2 tbo_coord = (gl_FragCoord.xy - viewport.xy) * tbo_size / viewport.zw;
    int tbo_offset = int(tbo_coord.y) * int(tbo_size.x) + int(tbo_coord.x);
    color = texelFetch(tbo, tbo_offset).rabg;
}
)";
        d24s8_abgr_shader.Create(vs_source.data(), fs_source.c_str());

        OpenGLState state = OpenGLState::GetCurState();
        GLuint old_program = state.draw.shader_program;
        state.draw.shader_program = d24s8_abgr_shader.handle;
        state.Apply();

        GLint tbo_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "tbo");
        ASSERT(tbo_u_id != -1);
        glUniform1i(tbo_u_id, 0);

        state.draw.shader_program = old_program;
        state.Apply();

        d24s8_abgr_tbo_size_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "tbo_size");
        ASSERT(d24s8_abgr_tbo_size_u_id != -1);
        d24s8_abgr_viewport_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "viewport");
        ASSERT(d24s8_abgr_viewport_u_id != -1);
    }

    ~PixelBufferD24S8toABGR() {}

    void Reinterpret(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint read_fb_handle,
                     GLuint dst_tex, const Common::Rectangle<u32>& dst_rect,
                     GLuint draw_fb_handle) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        SCOPE_EXIT({ prev_state.Apply(); });

        OpenGLState state;
        state.draw.read_framebuffer = read_fb_handle;
        state.draw.draw_framebuffer = draw_fb_handle;
        state.Apply();

        glBindBuffer(GL_PIXEL_PACK_BUFFER, d24s8_abgr_buffer.handle);

        GLsizeiptr target_pbo_size =
            static_cast<GLsizeiptr>(src_rect.GetWidth()) * src_rect.GetHeight() * 4;
        if (target_pbo_size > d24s8_abgr_buffer_size) {
            d24s8_abgr_buffer_size = target_pbo_size * 2;
            glBufferData(GL_PIXEL_PACK_BUFFER, d24s8_abgr_buffer_size, nullptr, GL_STREAM_COPY);
        }

        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               src_tex, 0);
        glReadPixels(static_cast<GLint>(src_rect.left), static_cast<GLint>(src_rect.bottom),
                     static_cast<GLsizei>(src_rect.GetWidth()),
                     static_cast<GLsizei>(src_rect.GetHeight()), GL_DEPTH_STENCIL,
                     GL_UNSIGNED_INT_24_8, 0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // PBO now contains src_tex in RABG format
        state.draw.shader_program = d24s8_abgr_shader.handle;
        state.draw.vertex_array = attributeless_vao.handle;
        state.viewport.x = static_cast<GLint>(dst_rect.left);
        state.viewport.y = static_cast<GLint>(dst_rect.bottom);
        state.viewport.width = static_cast<GLsizei>(dst_rect.GetWidth());
        state.viewport.height = static_cast<GLsizei>(dst_rect.GetHeight());
        state.Apply();

        OGLTexture tbo;
        tbo.Create();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, tbo.handle);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, d24s8_abgr_buffer.handle);

        glUniform2f(d24s8_abgr_tbo_size_u_id, static_cast<GLfloat>(src_rect.GetWidth()),
                    static_cast<GLfloat>(src_rect.GetHeight()));
        glUniform4f(d24s8_abgr_viewport_u_id, static_cast<GLfloat>(state.viewport.x),
                    static_cast<GLfloat>(state.viewport.y),
                    static_cast<GLfloat>(state.viewport.width),
                    static_cast<GLfloat>(state.viewport.height));

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex,
                               0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_BUFFER, 0);
    }

private:
    OGLVertexArray attributeless_vao;
    OGLBuffer d24s8_abgr_buffer;
    GLsizeiptr d24s8_abgr_buffer_size;
    OGLProgram d24s8_abgr_shader;
    GLint d24s8_abgr_tbo_size_u_id;
    GLint d24s8_abgr_viewport_u_id;
};

FormatReinterpreterOpenGL::FormatReinterpreterOpenGL() {
    reinterpreters.emplace(PixelFormatPair{PixelFormat::RGBA8, PixelFormat::D24S8},
                           std::make_unique<PixelBufferD24S8toABGR>());
    reinterpreters.emplace(PixelFormatPair{PixelFormat::RGB5A1, PixelFormat::RGBA4},
                           std::make_unique<RGBA4toRGB5A1>());
}

FormatReinterpreterOpenGL::~FormatReinterpreterOpenGL() = default;

std::pair<FormatReinterpreterOpenGL::ReinterpreterMap::iterator,
          FormatReinterpreterOpenGL::ReinterpreterMap::iterator>
FormatReinterpreterOpenGL::GetPossibleReinterpretations(PixelFormat dst_format) {
    return reinterpreters.equal_range(dst_format);
}

} // namespace OpenGL
