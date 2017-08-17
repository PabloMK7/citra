// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <glad/glad.h>

namespace TextureUnits {

struct TextureUnit {
    GLint id;
    constexpr GLenum Enum() const {
        return static_cast<GLenum>(GL_TEXTURE0 + id);
    }
};

constexpr TextureUnit PicaTexture(int unit) {
    return TextureUnit{unit};
}

constexpr TextureUnit LightingLUT{3};
constexpr TextureUnit FogLUT{4};
constexpr TextureUnit ProcTexNoiseLUT{5};
constexpr TextureUnit ProcTexColorMap{6};
constexpr TextureUnit ProcTexAlphaMap{7};
constexpr TextureUnit ProcTexLUT{8};
constexpr TextureUnit ProcTexDiffLUT{9};

} // namespace TextureUnits

class OpenGLState {
public:
    struct {
        bool enabled;      // GL_CULL_FACE
        GLenum mode;       // GL_CULL_FACE_MODE
        GLenum front_face; // GL_FRONT_FACE
    } cull;

    struct {
        bool test_enabled;    // GL_DEPTH_TEST
        GLenum test_func;     // GL_DEPTH_FUNC
        GLboolean write_mask; // GL_DEPTH_WRITEMASK
    } depth;

    struct {
        GLboolean red_enabled;
        GLboolean green_enabled;
        GLboolean blue_enabled;
        GLboolean alpha_enabled;
    } color_mask; // GL_COLOR_WRITEMASK

    struct {
        bool test_enabled;          // GL_STENCIL_TEST
        GLenum test_func;           // GL_STENCIL_FUNC
        GLint test_ref;             // GL_STENCIL_REF
        GLuint test_mask;           // GL_STENCIL_VALUE_MASK
        GLuint write_mask;          // GL_STENCIL_WRITEMASK
        GLenum action_stencil_fail; // GL_STENCIL_FAIL
        GLenum action_depth_fail;   // GL_STENCIL_PASS_DEPTH_FAIL
        GLenum action_depth_pass;   // GL_STENCIL_PASS_DEPTH_PASS
    } stencil;

    struct {
        bool enabled;        // GL_BLEND
        GLenum rgb_equation; // GL_BLEND_EQUATION_RGB
        GLenum a_equation;   // GL_BLEND_EQUATION_ALPHA
        GLenum src_rgb_func; // GL_BLEND_SRC_RGB
        GLenum dst_rgb_func; // GL_BLEND_DST_RGB
        GLenum src_a_func;   // GL_BLEND_SRC_ALPHA
        GLenum dst_a_func;   // GL_BLEND_DST_ALPHA

        struct {
            GLclampf red;
            GLclampf green;
            GLclampf blue;
            GLclampf alpha;
        } color; // GL_BLEND_COLOR
    } blend;

    GLenum logic_op; // GL_LOGIC_OP_MODE

    // 3 texture units - one for each that is used in PICA fragment shader emulation
    struct {
        GLuint texture_2d; // GL_TEXTURE_BINDING_2D
        GLuint sampler;    // GL_SAMPLER_BINDING
    } texture_units[3];

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } lighting_lut;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } fog_lut;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } proctex_noise_lut;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } proctex_color_map;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } proctex_alpha_map;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } proctex_lut;

    struct {
        GLuint texture_buffer; // GL_TEXTURE_BINDING_BUFFER
    } proctex_diff_lut;

    struct {
        GLuint read_framebuffer; // GL_READ_FRAMEBUFFER_BINDING
        GLuint draw_framebuffer; // GL_DRAW_FRAMEBUFFER_BINDING
        GLuint vertex_array;     // GL_VERTEX_ARRAY_BINDING
        GLuint vertex_buffer;    // GL_ARRAY_BUFFER_BINDING
        GLuint uniform_buffer;   // GL_UNIFORM_BUFFER_BINDING
        GLuint shader_program;   // GL_CURRENT_PROGRAM
    } draw;

    std::array<bool, 2> clip_distance; // GL_CLIP_DISTANCE

    OpenGLState();

    /// Get the currently active OpenGL state
    static const OpenGLState& GetCurState() {
        return cur_state;
    }

    /// Apply this state as the current OpenGL state
    void Apply() const;

    /// Resets and unbinds any references to the given resource in the current OpenGL state
    static void ResetTexture(GLuint handle);
    static void ResetSampler(GLuint handle);
    static void ResetProgram(GLuint handle);
    static void ResetBuffer(GLuint handle);
    static void ResetVertexArray(GLuint handle);
    static void ResetFramebuffer(GLuint handle);

private:
    static OpenGLState cur_state;
};
