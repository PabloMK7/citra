// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/scope_exit.h"
#include "video_core/renderer_opengl/gl_format_reinterpreter.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

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

    PixelFormat GetSourceFormat() const override {
        return PixelFormat::RGBA4;
    }

    void Reinterpret(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                     const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        SCOPE_EXIT({ prev_state.Apply(); });

        OpenGLState state;
        state.texture_units[0].texture_2d = src_tex.handle;
        state.draw.draw_framebuffer = draw_fbo.handle;
        state.draw.shader_program = program.handle;
        state.draw.vertex_array = vao.handle;
        state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                          static_cast<GLsizei>(dst_rect.GetWidth()),
                          static_cast<GLsizei>(dst_rect.GetHeight())};
        state.Apply();

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               dst_tex.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               0, 0);

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

    PixelFormat GetSourceFormat() const override {
        return PixelFormat::D24S8;
    }

    void Reinterpret(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                     const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        SCOPE_EXIT({ prev_state.Apply(); });

        OpenGLState state;
        state.draw.read_framebuffer = read_fbo.handle;
        state.draw.draw_framebuffer = draw_fbo.handle;
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
                               src_tex.handle, 0);

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

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               dst_tex.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               0, 0);
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

class ShaderD24S8toRGBA8 final : public FormatReinterpreterBase {
public:
    ShaderD24S8toRGBA8() {
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

uniform highp sampler2D depth;
uniform lowp usampler2D stencil;
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

    highp uint depth_val =
        uint(texelFetch(depth, tex_coord, 0).x * (exp2(32.0) - 1.0));
    lowp uint stencil_val = texelFetch(stencil, tex_coord, 0).x;
    highp uvec4 components =
        uvec4(stencil_val, (uvec3(depth_val) >> uvec3(24u, 16u, 8u)) & 0x000000FFu);
    frag_color = vec4(components) / (exp2(8.0) - 1.0);
}
)";

        program.Create(vs_source.data(), fs_source.data());
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

    PixelFormat GetSourceFormat() const override {
        return PixelFormat::D24S8;
    }

    void Reinterpret(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                     const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) override {
        OpenGLState prev_state = OpenGLState::GetCurState();
        SCOPE_EXIT({ prev_state.Apply(); });

        OpenGLState state;
        state.texture_units[0].texture_2d = src_tex.handle;

        if (use_texture_view) {
            temp_tex.Create();
            glActiveTexture(GL_TEXTURE1);
            glTextureView(temp_tex.handle, GL_TEXTURE_2D, src_tex.handle, GL_DEPTH24_STENCIL8, 0, 1, 0, 1);
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
            glCopyImageSubData(src_tex.handle, GL_TEXTURE_2D, 0, src_rect.left, src_rect.bottom, 0,
                               temp_tex.handle, GL_TEXTURE_2D, 0, src_rect.left, src_rect.bottom, 0,
                               src_rect.GetWidth(), src_rect.GetHeight(), 1);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               dst_tex.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               0, 0);

        glUniform2i(dst_size_loc, dst_rect.GetWidth(), dst_rect.GetHeight());
        glUniform2i(src_size_loc, src_rect.GetWidth(), src_rect.GetHeight());
        glUniform2i(src_offset_loc, src_rect.left, src_rect.bottom);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (use_texture_view) {
            temp_tex.Release();
        }
    }

private:
    bool use_texture_view{};
    OGLProgram program{};
    GLint dst_size_loc{-1}, src_size_loc{-1}, src_offset_loc{-1};
    OGLVertexArray vao{};
    OGLTexture temp_tex{};
    Common::Rectangle<u32> temp_rect{0, 0, 0, 0};
};

FormatReinterpreterOpenGL::FormatReinterpreterOpenGL() {
    const std::string_view vendor{reinterpret_cast<const char*>(glGetString(GL_VENDOR))};
    const std::string_view version{reinterpret_cast<const char*>(glGetString(GL_VERSION))};

    // Fallback to PBO path on obsolete intel drivers
    // intel`s GL_VERSION string - `3.3.0 - Build 25.20.100.6373`
    const bool intel_broken_drivers =
        vendor.find("Intel") != vendor.npos && (std::atoi(version.substr(14, 2).data()) < 30);

    auto Register = [this](PixelFormat dest, std::unique_ptr<FormatReinterpreterBase>&& obj) {
        const u32 dst_index = static_cast<u32>(dest);
        return reinterpreters[dst_index].push_back(std::move(obj));
    };

    if ((!intel_broken_drivers && GLAD_GL_ARB_stencil_texturing &&
         GLAD_GL_ARB_texture_storage && GLAD_GL_ARB_copy_image) || GLES) {
        Register(PixelFormat::RGBA8, std::make_unique<ShaderD24S8toRGBA8>());
        LOG_INFO(Render_OpenGL, "Using shader for D24S8 to RGBA8 reinterpretation");
    } else {
        Register(PixelFormat::RGBA8, std::make_unique<PixelBufferD24S8toABGR>());
        LOG_INFO(Render_OpenGL, "Using PBO for D24S8 to RGBA8 reinterpretation");
    }

    Register(PixelFormat::RGB5A1, std::make_unique<RGBA4toRGB5A1>());
}

auto FormatReinterpreterOpenGL::GetPossibleReinterpretations(PixelFormat dst_format)
    -> const ReinterpreterList& {
    return reinterpreters[static_cast<u32>(dst_format)];
}

} // namespace OpenGL
