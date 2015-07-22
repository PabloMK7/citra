// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/pica.h"

OpenGLState OpenGLState::cur_state;

OpenGLState::OpenGLState() {
    // These all match default OpenGL values
    cull.enabled = false;
    cull.mode = GL_BACK;

    depth.test_enabled = false;
    depth.test_func = GL_LESS;
    depth.write_mask = GL_TRUE;

    color_mask.red_enabled = GL_TRUE;
    color_mask.green_enabled = GL_TRUE;
    color_mask.blue_enabled = GL_TRUE;
    color_mask.alpha_enabled = GL_TRUE;

    stencil.test_enabled = false;
    stencil.test_func = GL_ALWAYS;
    stencil.test_ref = 0;
    stencil.test_mask = -1;
    stencil.write_mask = -1;

    blend.enabled = false;
    blend.src_rgb_func = GL_ONE;
    blend.dst_rgb_func = GL_ZERO;
    blend.src_a_func = GL_ONE;
    blend.dst_a_func = GL_ZERO;
    blend.color.red = 0.0f;
    blend.color.green = 0.0f;
    blend.color.blue = 0.0f;
    blend.color.alpha = 0.0f;

    logic_op = GL_COPY;

    for (auto& texture_unit : texture_units) {
        texture_unit.enabled_2d = false;
        texture_unit.texture_2d = 0;
    }

    draw.framebuffer = 0;
    draw.vertex_array = 0;
    draw.vertex_buffer = 0;
    draw.shader_program = 0;
}

void OpenGLState::Apply() {
    // Culling
    if (cull.enabled != cur_state.cull.enabled) {
        if (cull.enabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    if (cull.mode != cur_state.cull.mode) {
        glCullFace(cull.mode);
    }

    // Depth test
    if (depth.test_enabled != cur_state.depth.test_enabled) {
        if (depth.test_enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (depth.test_func != cur_state.depth.test_func) {
        glDepthFunc(depth.test_func);
    }

    // Depth mask
    if (depth.write_mask != cur_state.depth.write_mask) {
        glDepthMask(depth.write_mask);
    }

    // Color mask
    if (color_mask.red_enabled != cur_state.color_mask.red_enabled ||
            color_mask.green_enabled != cur_state.color_mask.green_enabled ||
            color_mask.blue_enabled != cur_state.color_mask.blue_enabled ||
            color_mask.alpha_enabled != cur_state.color_mask.alpha_enabled) {
        glColorMask(color_mask.red_enabled, color_mask.green_enabled,
                    color_mask.blue_enabled, color_mask.alpha_enabled);
    }

    // Stencil test
    if (stencil.test_enabled != cur_state.stencil.test_enabled) {
        if (stencil.test_enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }

    if (stencil.test_func != cur_state.stencil.test_func ||
            stencil.test_ref != cur_state.stencil.test_ref ||
            stencil.test_mask != cur_state.stencil.test_mask) {
        glStencilFunc(stencil.test_func, stencil.test_ref, stencil.test_mask);
    }

    // Stencil mask
    if (stencil.write_mask != cur_state.stencil.write_mask) {
        glStencilMask(stencil.write_mask);
    }

    // Blending
    if (blend.enabled != cur_state.blend.enabled) {
        if (blend.enabled) {
            glEnable(GL_BLEND);

            cur_state.logic_op = GL_COPY;
            glLogicOp(cur_state.logic_op);
            glDisable(GL_COLOR_LOGIC_OP);
        } else {
            glDisable(GL_BLEND);
            glEnable(GL_COLOR_LOGIC_OP);
        }
    }

    if (blend.color.red != cur_state.blend.color.red ||
            blend.color.green != cur_state.blend.color.green ||
            blend.color.blue != cur_state.blend.color.blue ||
            blend.color.alpha != cur_state.blend.color.alpha) {
        glBlendColor(blend.color.red, blend.color.green,
                     blend.color.blue, blend.color.alpha);
    }

    if (blend.src_rgb_func != cur_state.blend.src_rgb_func ||
            blend.dst_rgb_func != cur_state.blend.dst_rgb_func ||
            blend.src_a_func != cur_state.blend.src_a_func ||
            blend.dst_a_func != cur_state.blend.dst_a_func) {
        glBlendFuncSeparate(blend.src_rgb_func, blend.dst_rgb_func,
                            blend.src_a_func, blend.dst_a_func);
    }

    if (logic_op != cur_state.logic_op) {
        glLogicOp(logic_op);
    }

    // Textures
    for (unsigned texture_index = 0; texture_index < ARRAY_SIZE(texture_units); ++texture_index) {
        if (texture_units[texture_index].enabled_2d != cur_state.texture_units[texture_index].enabled_2d ||
            texture_units[texture_index].texture_2d != cur_state.texture_units[texture_index].texture_2d) {

            glActiveTexture(GL_TEXTURE0 + texture_index);

            if (texture_units[texture_index].enabled_2d) {
                glBindTexture(GL_TEXTURE_2D, texture_units[texture_index].texture_2d);
            } else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    // Framebuffer
    if (draw.framebuffer != cur_state.draw.framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, draw.framebuffer);
    }

    // Vertex array
    if (draw.vertex_array != cur_state.draw.vertex_array) {
        glBindVertexArray(draw.vertex_array);
    }

    // Vertex buffer
    if (draw.vertex_buffer != cur_state.draw.vertex_buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, draw.vertex_buffer);
    }

    // Shader program
    if (draw.shader_program != cur_state.draw.shader_program) {
        glUseProgram(draw.shader_program);
    }

    cur_state = *this;
}
