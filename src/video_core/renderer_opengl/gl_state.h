// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "generated/gl_3_2_core.h"

class OpenGLState {
public:
    struct {
        bool enabled; // GL_CULL_FACE
        GLenum mode; // GL_CULL_FACE_MODE
    } cull;

    struct {
        bool test_enabled; // GL_DEPTH_TEST
        GLenum test_func; // GL_DEPTH_FUNC
        GLboolean write_mask; // GL_DEPTH_WRITEMASK
    } depth;

    struct {
        GLboolean red_enabled;
        GLboolean green_enabled;
        GLboolean blue_enabled;
        GLboolean alpha_enabled;
    } color_mask; // GL_COLOR_WRITEMASK

    struct {
        bool test_enabled; // GL_STENCIL_TEST
        GLenum test_func; // GL_STENCIL_FUNC
        GLint test_ref; // GL_STENCIL_REF
        GLuint test_mask; // GL_STENCIL_VALUE_MASK
        GLuint write_mask; // GL_STENCIL_WRITEMASK
    } stencil;

    struct {
        bool enabled; // GL_BLEND
        GLenum src_rgb_func; // GL_BLEND_SRC_RGB
        GLenum dst_rgb_func; // GL_BLEND_DST_RGB
        GLenum src_a_func; // GL_BLEND_SRC_ALPHA
        GLenum dst_a_func; // GL_BLEND_DST_ALPHA

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
        bool enabled_2d; // GL_TEXTURE_2D
        GLuint texture_2d; // GL_TEXTURE_BINDING_2D
    } texture_units[3];

    struct {
        GLuint framebuffer; // GL_DRAW_FRAMEBUFFER_BINDING
        GLuint vertex_array; // GL_VERTEX_ARRAY_BINDING
        GLuint vertex_buffer; // GL_ARRAY_BUFFER_BINDING
        GLuint shader_program; // GL_CURRENT_PROGRAM
    } draw;

    OpenGLState();

    /// Get the currently active OpenGL state
    static const OpenGLState& GetCurState() {
        return cur_state;
    }

    /// Apply this state as the current OpenGL state
    void Apply();

private:
    static OpenGLState cur_state;
};
