// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_state.h"

OpenGLState OpenGLState::cur_state;

OpenGLState::OpenGLState() {
    // These all match default OpenGL values
    cull.enabled = false;
    cull.mode = GL_BACK;
    cull.front_face = GL_CCW;

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
    stencil.action_depth_fail = GL_KEEP;
    stencil.action_depth_pass = GL_KEEP;
    stencil.action_stencil_fail = GL_KEEP;

    blend.enabled = false;
    blend.rgb_equation = GL_FUNC_ADD;
    blend.a_equation = GL_FUNC_ADD;
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
        texture_unit.texture_2d = 0;
        texture_unit.sampler = 0;
    }

    for (auto& lut : lighting_luts) {
        lut.texture_1d = 0;
    }

    fog_lut.texture_1d = 0;

    draw.read_framebuffer = 0;
    draw.draw_framebuffer = 0;
    draw.vertex_array = 0;
    draw.vertex_buffer = 0;
    draw.uniform_buffer = 0;
    draw.shader_program = 0;
}

void OpenGLState::Apply() const {
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

    if (cull.front_face != cur_state.cull.front_face) {
        glFrontFace(cull.front_face);
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
        glColorMask(color_mask.red_enabled, color_mask.green_enabled, color_mask.blue_enabled,
                    color_mask.alpha_enabled);
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

    if (stencil.action_depth_fail != cur_state.stencil.action_depth_fail ||
        stencil.action_depth_pass != cur_state.stencil.action_depth_pass ||
        stencil.action_stencil_fail != cur_state.stencil.action_stencil_fail) {
        glStencilOp(stencil.action_stencil_fail, stencil.action_depth_fail,
                    stencil.action_depth_pass);
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
        glBlendColor(blend.color.red, blend.color.green, blend.color.blue, blend.color.alpha);
    }

    if (blend.src_rgb_func != cur_state.blend.src_rgb_func ||
        blend.dst_rgb_func != cur_state.blend.dst_rgb_func ||
        blend.src_a_func != cur_state.blend.src_a_func ||
        blend.dst_a_func != cur_state.blend.dst_a_func) {
        glBlendFuncSeparate(blend.src_rgb_func, blend.dst_rgb_func, blend.src_a_func,
                            blend.dst_a_func);
    }

    if (blend.rgb_equation != cur_state.blend.rgb_equation ||
        blend.a_equation != cur_state.blend.a_equation) {
        glBlendEquationSeparate(blend.rgb_equation, blend.a_equation);
    }

    if (logic_op != cur_state.logic_op) {
        glLogicOp(logic_op);
    }

    // Textures
    for (unsigned i = 0; i < ARRAY_SIZE(texture_units); ++i) {
        if (texture_units[i].texture_2d != cur_state.texture_units[i].texture_2d) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, texture_units[i].texture_2d);
        }
        if (texture_units[i].sampler != cur_state.texture_units[i].sampler) {
            glBindSampler(i, texture_units[i].sampler);
        }
    }

    // Lighting LUTs
    for (unsigned i = 0; i < ARRAY_SIZE(lighting_luts); ++i) {
        if (lighting_luts[i].texture_1d != cur_state.lighting_luts[i].texture_1d) {
            glActiveTexture(GL_TEXTURE3 + i);
            glBindTexture(GL_TEXTURE_1D, lighting_luts[i].texture_1d);
        }
    }

    // Fog LUT
    if (fog_lut.texture_1d != cur_state.fog_lut.texture_1d) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_1D, fog_lut.texture_1d);
    }

    // Framebuffer
    if (draw.read_framebuffer != cur_state.draw.read_framebuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, draw.read_framebuffer);
    }
    if (draw.draw_framebuffer != cur_state.draw.draw_framebuffer) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw.draw_framebuffer);
    }

    // Vertex array
    if (draw.vertex_array != cur_state.draw.vertex_array) {
        glBindVertexArray(draw.vertex_array);
    }

    // Vertex buffer
    if (draw.vertex_buffer != cur_state.draw.vertex_buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, draw.vertex_buffer);
    }

    // Uniform buffer
    if (draw.uniform_buffer != cur_state.draw.uniform_buffer) {
        glBindBuffer(GL_UNIFORM_BUFFER, draw.uniform_buffer);
    }

    // Shader program
    if (draw.shader_program != cur_state.draw.shader_program) {
        glUseProgram(draw.shader_program);
    }

    cur_state = *this;
}

GLenum OpenGLState::CheckFBStatus(GLenum target) {
    GLenum fb_status = glCheckFramebufferStatus(target);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        const char* fb_description =
            (target == GL_READ_FRAMEBUFFER ? "READ"
                                           : (target == GL_DRAW_FRAMEBUFFER ? "DRAW" : "UNK"));
        LOG_CRITICAL(Render_OpenGL, "OpenGL %s framebuffer check failed, status %X", fb_description,
                     fb_status);
    }

    return fb_status;
}

void OpenGLState::ResetTexture(GLuint handle) {
    for (auto& unit : cur_state.texture_units) {
        if (unit.texture_2d == handle) {
            unit.texture_2d = 0;
        }
    }
}

void OpenGLState::ResetSampler(GLuint handle) {
    for (auto& unit : cur_state.texture_units) {
        if (unit.sampler == handle) {
            unit.sampler = 0;
        }
    }
}

void OpenGLState::ResetProgram(GLuint handle) {
    if (cur_state.draw.shader_program == handle) {
        cur_state.draw.shader_program = 0;
    }
}

void OpenGLState::ResetBuffer(GLuint handle) {
    if (cur_state.draw.vertex_buffer == handle) {
        cur_state.draw.vertex_buffer = 0;
    }
    if (cur_state.draw.uniform_buffer == handle) {
        cur_state.draw.uniform_buffer = 0;
    }
}

void OpenGLState::ResetVertexArray(GLuint handle) {
    if (cur_state.draw.vertex_array == handle) {
        cur_state.draw.vertex_array = 0;
    }
}

void OpenGLState::ResetFramebuffer(GLuint handle) {
    if (cur_state.draw.read_framebuffer == handle) {
        cur_state.draw.read_framebuffer = 0;
    }
    if (cur_state.draw.draw_framebuffer == handle) {
        cur_state.draw.draw_framebuffer = 0;
    }
}
