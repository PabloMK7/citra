// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>

#include <glad/glad.h>

#include "common/color.h"
#include "common/file_util.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/profiler.h"

#include "core/memory.h"
#include "core/settings.h"
#include "core/hw/gpu.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/utils.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/pica_to_gl.h"

static bool IsPassThroughTevStage(const Pica::Regs::TevStageConfig& stage) {
    return (stage.color_op == Pica::Regs::TevStageConfig::Operation::Replace &&
            stage.alpha_op == Pica::Regs::TevStageConfig::Operation::Replace &&
            stage.color_source1 == Pica::Regs::TevStageConfig::Source::Previous &&
            stage.alpha_source1 == Pica::Regs::TevStageConfig::Source::Previous &&
            stage.color_modifier1 == Pica::Regs::TevStageConfig::ColorModifier::SourceColor &&
            stage.alpha_modifier1 == Pica::Regs::TevStageConfig::AlphaModifier::SourceAlpha &&
            stage.GetColorMultiplier() == 1 &&
            stage.GetAlphaMultiplier() == 1);
}

RasterizerOpenGL::RasterizerOpenGL() : cached_fb_color_addr(0), cached_fb_depth_addr(0) { }
RasterizerOpenGL::~RasterizerOpenGL() { }

void RasterizerOpenGL::InitObjects() {
    // Create sampler objects
    for (size_t i = 0; i < texture_samplers.size(); ++i) {
        texture_samplers[i].Create();
        state.texture_units[i].sampler = texture_samplers[i].sampler.handle;
    }

    // Generate VBO, VAO and UBO
    vertex_buffer.Create();
    vertex_array.Create();
    uniform_buffer.Create();

    state.draw.vertex_array = vertex_array.handle;
    state.draw.vertex_buffer = vertex_buffer.handle;
    state.draw.uniform_buffer = uniform_buffer.handle;
    state.Apply();

    // Bind the UBO to binding point 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buffer.handle);

    uniform_block_data.dirty = true;

    // Set vertex attributes
    glVertexAttribPointer(GLShader::ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, position));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_POSITION);

    glVertexAttribPointer(GLShader::ATTRIBUTE_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, color));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_COLOR);

    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord0));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord1));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord2));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD0);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD1);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD2);

    glVertexAttribPointer(GLShader::ATTRIBUTE_NORMQUAT, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, normquat));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_NORMQUAT);

    glVertexAttribPointer(GLShader::ATTRIBUTE_VIEW, 3, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, view));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_VIEW);

    SetShader();

    // Create textures for OGL framebuffer that will be rendered to, initially 1x1 to succeed in framebuffer creation
    fb_color_texture.texture.Create();
    ReconfigureColorTexture(fb_color_texture, Pica::Regs::ColorFormat::RGBA8, 1, 1);

    state.texture_units[0].texture_2d = fb_color_texture.texture.handle;
    state.Apply();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    state.texture_units[0].texture_2d = 0;
    state.Apply();

    fb_depth_texture.texture.Create();
    ReconfigureDepthTexture(fb_depth_texture, Pica::Regs::DepthFormat::D16, 1, 1);

    state.texture_units[0].texture_2d = fb_depth_texture.texture.handle;
    state.Apply();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    state.texture_units[0].texture_2d = 0;
    state.Apply();

    // Configure OpenGL framebuffer
    framebuffer.Create();

    state.draw.framebuffer = framebuffer.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_color_texture.texture.handle, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb_depth_texture.texture.handle, 0);

    for (size_t i = 0; i < lighting_lut.size(); ++i) {
        lighting_lut[i].Create();
        state.lighting_lut[i].texture_1d = lighting_lut[i].handle;

        glActiveTexture(GL_TEXTURE3 + i);
        glBindTexture(GL_TEXTURE_1D, state.lighting_lut[i].texture_1d);

        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    state.Apply();

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_MSG(status == GL_FRAMEBUFFER_COMPLETE,
               "OpenGL rasterizer framebuffer setup failed, status %X", status);
}

void RasterizerOpenGL::Reset() {
    SyncCullMode();
    SyncDepthModifiers();
    SyncBlendEnabled();
    SyncBlendFuncs();
    SyncBlendColor();
    SyncLogicOp();
    SyncStencilTest();
    SyncDepthTest();

    SetShader();

    res_cache.InvalidateAll();
}

/**
 * This is a helper function to resolve an issue with opposite quaternions being interpolated by
 * OpenGL. See below for a detailed description of this issue (yuriks):
 *
 * For any rotation, there are two quaternions Q, and -Q, that represent the same rotation. If you
 * interpolate two quaternions that are opposite, instead of going from one rotation to another
 * using the shortest path, you'll go around the longest path. You can test if two quaternions are
 * opposite by checking if Dot(Q1, W2) < 0. In that case, you can flip either of them, therefore
 * making Dot(-Q1, W2) positive.
 *
 * NOTE: This solution corrects this issue per-vertex before passing the quaternions to OpenGL. This
 * should be correct for nearly all cases, however a more correct implementation (but less trivial
 * and perhaps unnecessary) would be to handle this per-fragment, by interpolating the quaternions
 * manually using two Lerps, and doing this correction before each Lerp.
 */
static bool AreQuaternionsOpposite(Math::Vec4<Pica::float24> qa, Math::Vec4<Pica::float24> qb) {
    Math::Vec4f a{ qa.x.ToFloat32(), qa.y.ToFloat32(), qa.z.ToFloat32(), qa.w.ToFloat32() };
    Math::Vec4f b{ qb.x.ToFloat32(), qb.y.ToFloat32(), qb.z.ToFloat32(), qb.w.ToFloat32() };

    return (Math::Dot(a, b) < 0.f);
}

void RasterizerOpenGL::AddTriangle(const Pica::Shader::OutputVertex& v0,
                                   const Pica::Shader::OutputVertex& v1,
                                   const Pica::Shader::OutputVertex& v2) {
    vertex_batch.emplace_back(v0, false);
    vertex_batch.emplace_back(v1, AreQuaternionsOpposite(v0.quat, v1.quat));
    vertex_batch.emplace_back(v2, AreQuaternionsOpposite(v0.quat, v2.quat));
}

void RasterizerOpenGL::DrawTriangles() {
    if (vertex_batch.empty())
        return;

    SyncFramebuffer();
    SyncDrawState();

    if (state.draw.shader_dirty) {
        SetShader();
        state.draw.shader_dirty = false;
    }

    for (unsigned index = 0; index < lighting_lut.size(); index++) {
        if (uniform_block_data.lut_dirty[index]) {
            SyncLightingLUT(index);
            uniform_block_data.lut_dirty[index] = false;
        }
    }

    if (uniform_block_data.dirty) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformData), &uniform_block_data.data, GL_STATIC_DRAW);
        uniform_block_data.dirty = false;
    }

    glBufferData(GL_ARRAY_BUFFER, vertex_batch.size() * sizeof(HardwareVertex), vertex_batch.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_batch.size());

    vertex_batch.clear();

    // Flush the resource cache at the current depth and color framebuffer addresses for render-to-texture
    const auto& regs = Pica::g_state.regs;

    u32 cached_fb_color_size = Pica::Regs::BytesPerColorPixel(fb_color_texture.format)
                               * fb_color_texture.width * fb_color_texture.height;

    u32 cached_fb_depth_size = Pica::Regs::BytesPerDepthPixel(fb_depth_texture.format)
                               * fb_depth_texture.width * fb_depth_texture.height;

    res_cache.InvalidateInRange(cached_fb_color_addr, cached_fb_color_size, true);
    res_cache.InvalidateInRange(cached_fb_depth_addr, cached_fb_depth_size, true);
}

void RasterizerOpenGL::FlushFramebuffer() {
    CommitColorBuffer();
    CommitDepthBuffer();
}

void RasterizerOpenGL::NotifyPicaRegisterChanged(u32 id) {
    const auto& regs = Pica::g_state.regs;

    switch(id) {
    // Culling
    case PICA_REG_INDEX(cull_mode):
        SyncCullMode();
        break;

    // Depth modifiers
    case PICA_REG_INDEX(viewport_depth_range):
    case PICA_REG_INDEX(viewport_depth_far_plane):
        SyncDepthModifiers();
        break;

    // Blending
    case PICA_REG_INDEX(output_merger.alphablend_enable):
        SyncBlendEnabled();
        break;
    case PICA_REG_INDEX(output_merger.alpha_blending):
        SyncBlendFuncs();
        break;
    case PICA_REG_INDEX(output_merger.blend_const):
        SyncBlendColor();
        break;

    // Alpha test
    case PICA_REG_INDEX(output_merger.alpha_test):
        SyncAlphaTest();
        state.draw.shader_dirty = true;
        break;

    // Stencil test
    case PICA_REG_INDEX(output_merger.stencil_test.raw_func):
    case PICA_REG_INDEX(output_merger.stencil_test.raw_op):
        SyncStencilTest();
        break;

    // Depth test
    case PICA_REG_INDEX(output_merger.depth_test_enable):
        SyncDepthTest();
        break;

    // Logic op
    case PICA_REG_INDEX(output_merger.logic_op):
        SyncLogicOp();
        break;

    // TEV stages
    case PICA_REG_INDEX(tev_stage0.color_source1):
    case PICA_REG_INDEX(tev_stage0.color_modifier1):
    case PICA_REG_INDEX(tev_stage0.color_op):
    case PICA_REG_INDEX(tev_stage0.color_scale):
    case PICA_REG_INDEX(tev_stage1.color_source1):
    case PICA_REG_INDEX(tev_stage1.color_modifier1):
    case PICA_REG_INDEX(tev_stage1.color_op):
    case PICA_REG_INDEX(tev_stage1.color_scale):
    case PICA_REG_INDEX(tev_stage2.color_source1):
    case PICA_REG_INDEX(tev_stage2.color_modifier1):
    case PICA_REG_INDEX(tev_stage2.color_op):
    case PICA_REG_INDEX(tev_stage2.color_scale):
    case PICA_REG_INDEX(tev_stage3.color_source1):
    case PICA_REG_INDEX(tev_stage3.color_modifier1):
    case PICA_REG_INDEX(tev_stage3.color_op):
    case PICA_REG_INDEX(tev_stage3.color_scale):
    case PICA_REG_INDEX(tev_stage4.color_source1):
    case PICA_REG_INDEX(tev_stage4.color_modifier1):
    case PICA_REG_INDEX(tev_stage4.color_op):
    case PICA_REG_INDEX(tev_stage4.color_scale):
    case PICA_REG_INDEX(tev_stage5.color_source1):
    case PICA_REG_INDEX(tev_stage5.color_modifier1):
    case PICA_REG_INDEX(tev_stage5.color_op):
    case PICA_REG_INDEX(tev_stage5.color_scale):
    case PICA_REG_INDEX(tev_combiner_buffer_input):
        state.draw.shader_dirty = true;
        break;
    case PICA_REG_INDEX(tev_stage0.const_r):
        SyncTevConstColor(0, regs.tev_stage0);
        break;
    case PICA_REG_INDEX(tev_stage1.const_r):
        SyncTevConstColor(1, regs.tev_stage1);
        break;
    case PICA_REG_INDEX(tev_stage2.const_r):
        SyncTevConstColor(2, regs.tev_stage2);
        break;
    case PICA_REG_INDEX(tev_stage3.const_r):
        SyncTevConstColor(3, regs.tev_stage3);
        break;
    case PICA_REG_INDEX(tev_stage4.const_r):
        SyncTevConstColor(4, regs.tev_stage4);
        break;
    case PICA_REG_INDEX(tev_stage5.const_r):
        SyncTevConstColor(5, regs.tev_stage5);
        break;

    // TEV combiner buffer color
    case PICA_REG_INDEX(tev_combiner_buffer_color):
        SyncCombinerColor();
        break;

    // Fragment lighting specular 0 color
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].specular_0, 0x140 + 0 * 0x10):
        SyncLightSpecular0(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].specular_0, 0x140 + 1 * 0x10):
        SyncLightSpecular0(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].specular_0, 0x140 + 2 * 0x10):
        SyncLightSpecular0(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].specular_0, 0x140 + 3 * 0x10):
        SyncLightSpecular0(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].specular_0, 0x140 + 4 * 0x10):
        SyncLightSpecular0(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].specular_0, 0x140 + 5 * 0x10):
        SyncLightSpecular0(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].specular_0, 0x140 + 6 * 0x10):
        SyncLightSpecular0(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].specular_0, 0x140 + 7 * 0x10):
        SyncLightSpecular0(7);
        break;

    // Fragment lighting specular 1 color
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].specular_1, 0x141 + 0 * 0x10):
        SyncLightSpecular1(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].specular_1, 0x141 + 1 * 0x10):
        SyncLightSpecular1(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].specular_1, 0x141 + 2 * 0x10):
        SyncLightSpecular1(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].specular_1, 0x141 + 3 * 0x10):
        SyncLightSpecular1(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].specular_1, 0x141 + 4 * 0x10):
        SyncLightSpecular1(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].specular_1, 0x141 + 5 * 0x10):
        SyncLightSpecular1(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].specular_1, 0x141 + 6 * 0x10):
        SyncLightSpecular1(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].specular_1, 0x141 + 7 * 0x10):
        SyncLightSpecular1(7);
        break;

    // Fragment lighting diffuse color
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].diffuse, 0x142 + 0 * 0x10):
        SyncLightDiffuse(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].diffuse, 0x142 + 1 * 0x10):
        SyncLightDiffuse(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].diffuse, 0x142 + 2 * 0x10):
        SyncLightDiffuse(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].diffuse, 0x142 + 3 * 0x10):
        SyncLightDiffuse(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].diffuse, 0x142 + 4 * 0x10):
        SyncLightDiffuse(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].diffuse, 0x142 + 5 * 0x10):
        SyncLightDiffuse(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].diffuse, 0x142 + 6 * 0x10):
        SyncLightDiffuse(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].diffuse, 0x142 + 7 * 0x10):
        SyncLightDiffuse(7);
        break;

    // Fragment lighting ambient color
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].ambient, 0x143 + 0 * 0x10):
        SyncLightAmbient(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].ambient, 0x143 + 1 * 0x10):
        SyncLightAmbient(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].ambient, 0x143 + 2 * 0x10):
        SyncLightAmbient(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].ambient, 0x143 + 3 * 0x10):
        SyncLightAmbient(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].ambient, 0x143 + 4 * 0x10):
        SyncLightAmbient(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].ambient, 0x143 + 5 * 0x10):
        SyncLightAmbient(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].ambient, 0x143 + 6 * 0x10):
        SyncLightAmbient(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].ambient, 0x143 + 7 * 0x10):
        SyncLightAmbient(7);
        break;

     // Fragment lighting position
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].x, 0x144 + 0 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].z, 0x145 + 0 * 0x10):
        SyncLightPosition(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].x, 0x144 + 1 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].z, 0x145 + 1 * 0x10):
        SyncLightPosition(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].x, 0x144 + 2 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].z, 0x145 + 2 * 0x10):
        SyncLightPosition(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].x, 0x144 + 3 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].z, 0x145 + 3 * 0x10):
        SyncLightPosition(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].x, 0x144 + 4 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].z, 0x145 + 4 * 0x10):
        SyncLightPosition(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].x, 0x144 + 5 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].z, 0x145 + 5 * 0x10):
        SyncLightPosition(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].x, 0x144 + 6 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].z, 0x145 + 6 * 0x10):
        SyncLightPosition(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].x, 0x144 + 7 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].z, 0x145 + 7 * 0x10):
        SyncLightPosition(7);
        break;

    // Fragment lighting global ambient color (emission + ambient * ambient)
    case PICA_REG_INDEX_WORKAROUND(lighting.global_ambient, 0x1c0):
        SyncGlobalAmbient();
        break;

    // Fragment lighting lookup tables
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[0], 0x1c8):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[1], 0x1c9):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[2], 0x1ca):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[3], 0x1cb):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[4], 0x1cc):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[5], 0x1cd):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[6], 0x1ce):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[7], 0x1cf):
    {
        auto& lut_config = regs.lighting.lut_config;
        uniform_block_data.lut_dirty[lut_config.type / 4] = true;
        break;
    }

    }
}

void RasterizerOpenGL::FlushRegion(PAddr addr, u32 size) {
    const auto& regs = Pica::g_state.regs;

    u32 cached_fb_color_size = Pica::Regs::BytesPerColorPixel(fb_color_texture.format)
                               * fb_color_texture.width * fb_color_texture.height;

    u32 cached_fb_depth_size = Pica::Regs::BytesPerDepthPixel(fb_depth_texture.format)
                               * fb_depth_texture.width * fb_depth_texture.height;

    // If source memory region overlaps 3DS framebuffers, commit them before the copy happens
    if (MathUtil::IntervalsIntersect(addr, size, cached_fb_color_addr, cached_fb_color_size))
        CommitColorBuffer();

    if (MathUtil::IntervalsIntersect(addr, size, cached_fb_depth_addr, cached_fb_depth_size))
        CommitDepthBuffer();
}

void RasterizerOpenGL::InvalidateRegion(PAddr addr, u32 size) {
    const auto& regs = Pica::g_state.regs;

    u32 cached_fb_color_size = Pica::Regs::BytesPerColorPixel(fb_color_texture.format)
                               * fb_color_texture.width * fb_color_texture.height;

    u32 cached_fb_depth_size = Pica::Regs::BytesPerDepthPixel(fb_depth_texture.format)
                               * fb_depth_texture.width * fb_depth_texture.height;

    // If modified memory region overlaps 3DS framebuffers, reload their contents into OpenGL
    if (MathUtil::IntervalsIntersect(addr, size, cached_fb_color_addr, cached_fb_color_size))
        ReloadColorBuffer();

    if (MathUtil::IntervalsIntersect(addr, size, cached_fb_depth_addr, cached_fb_depth_size))
        ReloadDepthBuffer();

    // Notify cache of flush in case the region touches a cached resource
    res_cache.InvalidateInRange(addr, size);
}

void RasterizerOpenGL::SamplerInfo::Create() {
    sampler.Create();
    mag_filter = min_filter = TextureConfig::Linear;
    wrap_s = wrap_t = TextureConfig::Repeat;
    border_color = 0;

    glSamplerParameteri(sampler.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // default is GL_LINEAR_MIPMAP_LINEAR
    // Other attributes have correct defaults
}

void RasterizerOpenGL::SamplerInfo::SyncWithConfig(const Pica::Regs::TextureConfig& config) {
    GLuint s = sampler.handle;

    if (mag_filter != config.mag_filter) {
        mag_filter = config.mag_filter;
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, PicaToGL::TextureFilterMode(mag_filter));
    }
    if (min_filter != config.min_filter) {
        min_filter = config.min_filter;
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, PicaToGL::TextureFilterMode(min_filter));
    }

    if (wrap_s != config.wrap_s) {
        wrap_s = config.wrap_s;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, PicaToGL::WrapMode(wrap_s));
    }
    if (wrap_t != config.wrap_t) {
        wrap_t = config.wrap_t;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, PicaToGL::WrapMode(wrap_t));
    }

    if (wrap_s == TextureConfig::ClampToBorder || wrap_t == TextureConfig::ClampToBorder) {
        if (border_color != config.border_color.raw) {
            auto gl_color = PicaToGL::ColorRGBA8(border_color);
            glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, gl_color.data());
        }
    }
}

void RasterizerOpenGL::ReconfigureColorTexture(TextureInfo& texture, Pica::Regs::ColorFormat format, u32 width, u32 height) {
    GLint internal_format;

    texture.format = format;
    texture.width = width;
    texture.height = height;

    switch (format) {
    case Pica::Regs::ColorFormat::RGBA8:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8;
        break;

    case Pica::Regs::ColorFormat::RGB8:
        // This pixel format uses BGR since GL_UNSIGNED_BYTE specifies byte-order, unlike every
        // specific OpenGL type used in this function using native-endian (that is, little-endian
        // mostly everywhere) for words or half-words.
        // TODO: check how those behave on big-endian processors.
        internal_format = GL_RGB;
        texture.gl_format = GL_BGR;
        texture.gl_type = GL_UNSIGNED_BYTE;
        break;

    case Pica::Regs::ColorFormat::RGB5A1:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case Pica::Regs::ColorFormat::RGB565:
        internal_format = GL_RGB;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case Pica::Regs::ColorFormat::RGBA4:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown framebuffer texture color format %x", format);
        UNIMPLEMENTED();
        break;
    }

    state.texture_units[0].texture_2d = texture.texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0,
                 texture.gl_format, texture.gl_type, nullptr);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

void RasterizerOpenGL::ReconfigureDepthTexture(DepthTextureInfo& texture, Pica::Regs::DepthFormat format, u32 width, u32 height) {
    GLint internal_format;

    texture.format = format;
    texture.width = width;
    texture.height = height;

    switch (format) {
    case Pica::Regs::DepthFormat::D16:
        internal_format = GL_DEPTH_COMPONENT16;
        texture.gl_format = GL_DEPTH_COMPONENT;
        texture.gl_type = GL_UNSIGNED_SHORT;
        break;

    case Pica::Regs::DepthFormat::D24:
        internal_format = GL_DEPTH_COMPONENT24;
        texture.gl_format = GL_DEPTH_COMPONENT;
        texture.gl_type = GL_UNSIGNED_INT;
        break;

    case Pica::Regs::DepthFormat::D24S8:
        internal_format = GL_DEPTH24_STENCIL8;
        texture.gl_format = GL_DEPTH_STENCIL;
        texture.gl_type = GL_UNSIGNED_INT_24_8;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown framebuffer texture depth format %x", format);
        UNIMPLEMENTED();
        break;
    }

    state.texture_units[0].texture_2d = texture.texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0,
                 texture.gl_format, texture.gl_type, nullptr);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

void RasterizerOpenGL::SetShader() {
    PicaShaderConfig config = PicaShaderConfig::CurrentConfig();
    std::unique_ptr<PicaShader> shader = std::make_unique<PicaShader>();

    // Find (or generate) the GLSL shader for the current TEV state
    auto cached_shader = shader_cache.find(config);
    if (cached_shader != shader_cache.end()) {
        current_shader = cached_shader->second.get();

        state.draw.shader_program = current_shader->shader.handle;
        state.Apply();
    } else {
        LOG_DEBUG(Render_OpenGL, "Creating new shader");

        shader->shader.Create(GLShader::GenerateVertexShader().c_str(), GLShader::GenerateFragmentShader(config).c_str());

        state.draw.shader_program = shader->shader.handle;
        state.Apply();

        // Set the texture samplers to correspond to different texture units
        GLuint uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[0]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 0); }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[1]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 1); }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[2]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 2); }

        // Set the texture samplers to correspond to different lookup table texture units
        GLuint uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[0]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 3); }
        uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[1]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 4); }
        uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[2]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 5); }
        uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[3]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 6); }
        uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[4]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 7); }
        uniform_lut = glGetUniformLocation(shader->shader.handle, "lut[5]");
        if (uniform_lut != -1) { glUniform1i(uniform_lut, 8); }

        current_shader = shader_cache.emplace(config, std::move(shader)).first->second.get();

        unsigned int block_index = glGetUniformBlockIndex(current_shader->shader.handle, "shader_data");
        glUniformBlockBinding(current_shader->shader.handle, block_index, 0);

        // Update uniforms
        SyncAlphaTest();
        SyncCombinerColor();
        auto& tev_stages = Pica::g_state.regs.GetTevStages();
        for (int index = 0; index < tev_stages.size(); ++index)
            SyncTevConstColor(index, tev_stages[index]);

        SyncGlobalAmbient();
        for (int light_index = 0; light_index < 8; light_index++) {
            SyncLightDiffuse(light_index);
            SyncLightAmbient(light_index);
            SyncLightPosition(light_index);
        }
    }
}

void RasterizerOpenGL::SyncFramebuffer() {
    const auto& regs = Pica::g_state.regs;

    PAddr new_fb_color_addr = regs.framebuffer.GetColorBufferPhysicalAddress();
    Pica::Regs::ColorFormat new_fb_color_format = regs.framebuffer.color_format;

    PAddr new_fb_depth_addr = regs.framebuffer.GetDepthBufferPhysicalAddress();
    Pica::Regs::DepthFormat new_fb_depth_format = regs.framebuffer.depth_format;

    bool fb_size_changed = fb_color_texture.width != static_cast<GLsizei>(regs.framebuffer.GetWidth()) ||
                           fb_color_texture.height != static_cast<GLsizei>(regs.framebuffer.GetHeight());

    bool color_fb_prop_changed = fb_color_texture.format != new_fb_color_format ||
                                 fb_size_changed;

    bool depth_fb_prop_changed = fb_depth_texture.format != new_fb_depth_format ||
                                 fb_size_changed;

    bool color_fb_modified = cached_fb_color_addr != new_fb_color_addr ||
                             color_fb_prop_changed;

    bool depth_fb_modified = cached_fb_depth_addr != new_fb_depth_addr ||
                             depth_fb_prop_changed;

    // Commit if framebuffer modified in any way
    if (color_fb_modified)
        CommitColorBuffer();

    if (depth_fb_modified)
        CommitDepthBuffer();

    // Reconfigure framebuffer textures if any property has changed
    if (color_fb_prop_changed) {
        ReconfigureColorTexture(fb_color_texture, new_fb_color_format,
                                regs.framebuffer.GetWidth(), regs.framebuffer.GetHeight());
    }

    if (depth_fb_prop_changed) {
        ReconfigureDepthTexture(fb_depth_texture, new_fb_depth_format,
                                regs.framebuffer.GetWidth(), regs.framebuffer.GetHeight());

        // Only attach depth buffer as stencil if it supports stencil
        switch (new_fb_depth_format) {
        case Pica::Regs::DepthFormat::D16:
        case Pica::Regs::DepthFormat::D24:
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            break;

        case Pica::Regs::DepthFormat::D24S8:
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fb_depth_texture.texture.handle, 0);
            break;

        default:
            LOG_CRITICAL(Render_OpenGL, "Unknown framebuffer depth format %x", new_fb_depth_format);
            UNIMPLEMENTED();
            break;
        }
    }

    // Load buffer data again if fb modified in any way
    if (color_fb_modified) {
        cached_fb_color_addr = new_fb_color_addr;

        ReloadColorBuffer();
    }

    if (depth_fb_modified) {
        cached_fb_depth_addr = new_fb_depth_addr;

        ReloadDepthBuffer();
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_MSG(status == GL_FRAMEBUFFER_COMPLETE,
               "OpenGL rasterizer framebuffer setup failed, status %X", status);
}

void RasterizerOpenGL::SyncCullMode() {
    const auto& regs = Pica::g_state.regs;

    switch (regs.cull_mode) {
    case Pica::Regs::CullMode::KeepAll:
        state.cull.enabled = false;
        break;

    case Pica::Regs::CullMode::KeepClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CW;
        break;

    case Pica::Regs::CullMode::KeepCounterClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CCW;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown cull mode %d", regs.cull_mode.Value());
        UNIMPLEMENTED();
        break;
    }
}

void RasterizerOpenGL::SyncDepthModifiers() {
    float depth_scale = -Pica::float24::FromRaw(Pica::g_state.regs.viewport_depth_range).ToFloat32();
    float depth_offset = Pica::float24::FromRaw(Pica::g_state.regs.viewport_depth_far_plane).ToFloat32() / 2.0f;

    // TODO: Implement scale modifier
    uniform_block_data.data.depth_offset = depth_offset;
    uniform_block_data.dirty = true;
}

void RasterizerOpenGL::SyncBlendEnabled() {
    state.blend.enabled = (Pica::g_state.regs.output_merger.alphablend_enable == 1);
}

void RasterizerOpenGL::SyncBlendFuncs() {
    const auto& regs = Pica::g_state.regs;
    state.blend.src_rgb_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_source_rgb);
    state.blend.dst_rgb_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_dest_rgb);
    state.blend.src_a_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_source_a);
    state.blend.dst_a_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_dest_a);
}

void RasterizerOpenGL::SyncBlendColor() {
    auto blend_color = PicaToGL::ColorRGBA8(Pica::g_state.regs.output_merger.blend_const.raw);
    state.blend.color.red = blend_color[0];
    state.blend.color.green = blend_color[1];
    state.blend.color.blue = blend_color[2];
    state.blend.color.alpha = blend_color[3];
}

void RasterizerOpenGL::SyncAlphaTest() {
    const auto& regs = Pica::g_state.regs;
    if (regs.output_merger.alpha_test.ref != uniform_block_data.data.alphatest_ref) {
        uniform_block_data.data.alphatest_ref = regs.output_merger.alpha_test.ref;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLogicOp() {
    state.logic_op = PicaToGL::LogicOp(Pica::g_state.regs.output_merger.logic_op);
}

void RasterizerOpenGL::SyncStencilTest() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.test_enabled = regs.output_merger.stencil_test.enable && regs.framebuffer.depth_format == Pica::Regs::DepthFormat::D24S8;
    state.stencil.test_func = PicaToGL::CompareFunc(regs.output_merger.stencil_test.func);
    state.stencil.test_ref = regs.output_merger.stencil_test.reference_value;
    state.stencil.test_mask = regs.output_merger.stencil_test.input_mask;
    state.stencil.write_mask = regs.output_merger.stencil_test.write_mask;
    state.stencil.action_stencil_fail = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_stencil_fail);
    state.stencil.action_depth_fail = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_depth_fail);
    state.stencil.action_depth_pass = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_depth_pass);
}

void RasterizerOpenGL::SyncDepthTest() {
    const auto& regs = Pica::g_state.regs;
    state.depth.test_enabled = regs.output_merger.depth_test_enable  == 1 ||
                               regs.output_merger.depth_write_enable == 1;
    state.depth.test_func = regs.output_merger.depth_test_enable == 1 ?
                            PicaToGL::CompareFunc(regs.output_merger.depth_test_func) : GL_ALWAYS;
    state.color_mask.red_enabled = regs.output_merger.red_enable;
    state.color_mask.green_enabled = regs.output_merger.green_enable;
    state.color_mask.blue_enabled = regs.output_merger.blue_enable;
    state.color_mask.alpha_enabled = regs.output_merger.alpha_enable;
    state.depth.write_mask = regs.output_merger.depth_write_enable ? GL_TRUE : GL_FALSE;
}

void RasterizerOpenGL::SyncCombinerColor() {
    auto combiner_color = PicaToGL::ColorRGBA8(Pica::g_state.regs.tev_combiner_buffer_color.raw);
    if (combiner_color != uniform_block_data.data.tev_combiner_buffer_color) {
        uniform_block_data.data.tev_combiner_buffer_color = combiner_color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncTevConstColor(int stage_index, const Pica::Regs::TevStageConfig& tev_stage) {
    auto const_color = PicaToGL::ColorRGBA8(tev_stage.const_color);
    if (const_color != uniform_block_data.data.const_color[stage_index]) {
        uniform_block_data.data.const_color[stage_index] = const_color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncGlobalAmbient() {
    auto color = PicaToGL::LightColor(Pica::g_state.regs.lighting.global_ambient);
    if (color != uniform_block_data.data.lighting_global_ambient) {
        uniform_block_data.data.lighting_global_ambient = color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightingLUT(unsigned lut_index) {
    std::array<GLvec4, 256> new_data;

    for (unsigned offset = 0; offset < new_data.size(); ++offset) {
        new_data[offset][0] = Pica::g_state.lighting.luts[(lut_index * 4) + 0][offset].ToFloat();
        new_data[offset][1] = Pica::g_state.lighting.luts[(lut_index * 4) + 1][offset].ToFloat();
        new_data[offset][2] = Pica::g_state.lighting.luts[(lut_index * 4) + 2][offset].ToFloat();
        new_data[offset][3] = Pica::g_state.lighting.luts[(lut_index * 4) + 3][offset].ToFloat();
    }

    if (new_data != lighting_lut_data[lut_index]) {
        lighting_lut_data[lut_index] = new_data;
        glActiveTexture(GL_TEXTURE3 + lut_index);
        glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RGBA, GL_FLOAT, lighting_lut_data[lut_index].data());
    }
}

void RasterizerOpenGL::SyncLightSpecular0(int light_index) {
    auto color = PicaToGL::LightColor(Pica::g_state.regs.lighting.light[light_index].specular_0);
    if (color != uniform_block_data.data.light_src[light_index].specular_0) {
        uniform_block_data.data.light_src[light_index].specular_0 = color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightSpecular1(int light_index) {
    auto color = PicaToGL::LightColor(Pica::g_state.regs.lighting.light[light_index].specular_1);
    if (color != uniform_block_data.data.light_src[light_index].specular_1) {
        uniform_block_data.data.light_src[light_index].specular_1 = color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightDiffuse(int light_index) {
    auto color = PicaToGL::LightColor(Pica::g_state.regs.lighting.light[light_index].diffuse);
    if (color != uniform_block_data.data.light_src[light_index].diffuse) {
        uniform_block_data.data.light_src[light_index].diffuse = color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightAmbient(int light_index) {
    auto color = PicaToGL::LightColor(Pica::g_state.regs.lighting.light[light_index].ambient);
    if (color != uniform_block_data.data.light_src[light_index].ambient) {
        uniform_block_data.data.light_src[light_index].ambient = color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightPosition(int light_index) {
    GLvec3 position = {
        Pica::float16::FromRaw(Pica::g_state.regs.lighting.light[light_index].x).ToFloat32(),
        Pica::float16::FromRaw(Pica::g_state.regs.lighting.light[light_index].y).ToFloat32(),
        Pica::float16::FromRaw(Pica::g_state.regs.lighting.light[light_index].z).ToFloat32() };

    if (position != uniform_block_data.data.light_src[light_index].position) {
        uniform_block_data.data.light_src[light_index].position = position;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncDrawState() {
    const auto& regs = Pica::g_state.regs;

    // Sync the viewport
    GLsizei viewport_width = (GLsizei)Pica::float24::FromRaw(regs.viewport_size_x).ToFloat32() * 2;
    GLsizei viewport_height = (GLsizei)Pica::float24::FromRaw(regs.viewport_size_y).ToFloat32() * 2;

    // OpenGL uses different y coordinates, so negate corner offset and flip origin
    // TODO: Ensure viewport_corner.x should not be negated or origin flipped
    // TODO: Use floating-point viewports for accuracy if supported
    glViewport((GLsizei)regs.viewport_corner.x,
               (GLsizei)regs.viewport_corner.y,
                viewport_width, viewport_height);

    // Sync bound texture(s), upload if not cached
    const auto pica_textures = regs.GetTextures();
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        if (texture.enabled) {
            texture_samplers[texture_index].SyncWithConfig(texture.config);
            res_cache.LoadAndBindTexture(state, texture_index, texture);
        } else {
            state.texture_units[texture_index].texture_2d = 0;
        }
    }

    state.draw.uniform_buffer = uniform_buffer.handle;
    state.Apply();
}

MICROPROFILE_DEFINE(OpenGL_FramebufferReload, "OpenGL", "FB Reload", MP_RGB(70, 70, 200));

void RasterizerOpenGL::ReloadColorBuffer() {
    u8* color_buffer = Memory::GetPhysicalPointer(cached_fb_color_addr);

    if (color_buffer == nullptr)
        return;

    MICROPROFILE_SCOPE(OpenGL_FramebufferReload);

    u32 bytes_per_pixel = Pica::Regs::BytesPerColorPixel(fb_color_texture.format);

    std::unique_ptr<u8[]> temp_fb_color_buffer(new u8[fb_color_texture.width * fb_color_texture.height * bytes_per_pixel]);

    // Directly copy pixels. Internal OpenGL color formats are consistent so no conversion is necessary.
    for (int y = 0; y < fb_color_texture.height; ++y) {
        for (int x = 0; x < fb_color_texture.width; ++x) {
            const u32 coarse_y = y & ~7;
            u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_color_texture.width * bytes_per_pixel;
            u32 gl_pixel_index = (x + (fb_color_texture.height - 1 - y) * fb_color_texture.width) * bytes_per_pixel;

            u8* pixel = color_buffer + dst_offset;
            memcpy(&temp_fb_color_buffer[gl_pixel_index], pixel, bytes_per_pixel);
        }
    }

    state.texture_units[0].texture_2d = fb_color_texture.texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_color_texture.width, fb_color_texture.height,
                    fb_color_texture.gl_format, fb_color_texture.gl_type, temp_fb_color_buffer.get());

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

void RasterizerOpenGL::ReloadDepthBuffer() {
    if (cached_fb_depth_addr == 0)
        return;

    // TODO: Appears to work, but double-check endianness of depth values and order of depth-stencil
    u8* depth_buffer = Memory::GetPhysicalPointer(cached_fb_depth_addr);

    if (depth_buffer == nullptr)
        return;

    MICROPROFILE_SCOPE(OpenGL_FramebufferReload);

    u32 bytes_per_pixel = Pica::Regs::BytesPerDepthPixel(fb_depth_texture.format);

    // OpenGL needs 4 bpp alignment for D24
    u32 gl_bpp = bytes_per_pixel == 3 ? 4 : bytes_per_pixel;

    std::unique_ptr<u8[]> temp_fb_depth_buffer(new u8[fb_depth_texture.width * fb_depth_texture.height * gl_bpp]);

    u8* temp_fb_depth_data = bytes_per_pixel == 3 ? (temp_fb_depth_buffer.get() + 1) : temp_fb_depth_buffer.get();

    if (fb_depth_texture.format == Pica::Regs::DepthFormat::D24S8) {
        for (int y = 0; y < fb_depth_texture.height; ++y) {
            for (int x = 0; x < fb_depth_texture.width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_depth_texture.width * bytes_per_pixel;
                u32 gl_pixel_index = (x + (fb_depth_texture.height - 1 - y) * fb_depth_texture.width);

                u8* pixel = depth_buffer + dst_offset;
                u32 depth_stencil = *(u32*)pixel;
                ((u32*)temp_fb_depth_data)[gl_pixel_index] = (depth_stencil << 8) | (depth_stencil >> 24);
            }
        }
    } else {
        for (int y = 0; y < fb_depth_texture.height; ++y) {
            for (int x = 0; x < fb_depth_texture.width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_depth_texture.width * bytes_per_pixel;
                u32 gl_pixel_index = (x + (fb_depth_texture.height - 1 - y) * fb_depth_texture.width) * gl_bpp;

                u8* pixel = depth_buffer + dst_offset;
                memcpy(&temp_fb_depth_data[gl_pixel_index], pixel, bytes_per_pixel);
            }
        }
    }

    state.texture_units[0].texture_2d = fb_depth_texture.texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    if (fb_depth_texture.format == Pica::Regs::DepthFormat::D24S8) {
        // TODO(Subv): There is a bug with Intel Windows drivers that makes glTexSubImage2D not change the stencil buffer.
        // The bug has been reported to Intel (https://communities.intel.com/message/324464)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, fb_depth_texture.width, fb_depth_texture.height, 0,
            GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, temp_fb_depth_buffer.get());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_depth_texture.width, fb_depth_texture.height,
            fb_depth_texture.gl_format, fb_depth_texture.gl_type, temp_fb_depth_buffer.get());
    }

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

Common::Profiling::TimingCategory buffer_commit_category("Framebuffer Commit");
MICROPROFILE_DEFINE(OpenGL_FramebufferCommit, "OpenGL", "FB Commit", MP_RGB(70, 70, 200));

void RasterizerOpenGL::CommitColorBuffer() {
    if (cached_fb_color_addr != 0) {
        u8* color_buffer = Memory::GetPhysicalPointer(cached_fb_color_addr);

        if (color_buffer != nullptr) {
            Common::Profiling::ScopeTimer timer(buffer_commit_category);
            MICROPROFILE_SCOPE(OpenGL_FramebufferCommit);

            u32 bytes_per_pixel = Pica::Regs::BytesPerColorPixel(fb_color_texture.format);

            std::unique_ptr<u8[]> temp_gl_color_buffer(new u8[fb_color_texture.width * fb_color_texture.height * bytes_per_pixel]);

            state.texture_units[0].texture_2d = fb_color_texture.texture.handle;
            state.Apply();

            glActiveTexture(GL_TEXTURE0);
            glGetTexImage(GL_TEXTURE_2D, 0, fb_color_texture.gl_format, fb_color_texture.gl_type, temp_gl_color_buffer.get());

            state.texture_units[0].texture_2d = 0;
            state.Apply();

            // Directly copy pixels. Internal OpenGL color formats are consistent so no conversion is necessary.
            for (int y = 0; y < fb_color_texture.height; ++y) {
                for (int x = 0; x < fb_color_texture.width; ++x) {
                    const u32 coarse_y = y & ~7;
                    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_color_texture.width * bytes_per_pixel;
                    u32 gl_pixel_index = x * bytes_per_pixel + (fb_color_texture.height - 1 - y) * fb_color_texture.width * bytes_per_pixel;

                    u8* pixel = color_buffer + dst_offset;
                    memcpy(pixel, &temp_gl_color_buffer[gl_pixel_index], bytes_per_pixel);
                }
            }
        }
    }
}

void RasterizerOpenGL::CommitDepthBuffer() {
    if (cached_fb_depth_addr != 0) {
        // TODO: Output seems correct visually, but doesn't quite match sw renderer output. One of them is wrong.
        u8* depth_buffer = Memory::GetPhysicalPointer(cached_fb_depth_addr);

        if (depth_buffer != nullptr) {
            Common::Profiling::ScopeTimer timer(buffer_commit_category);
            MICROPROFILE_SCOPE(OpenGL_FramebufferCommit);

            u32 bytes_per_pixel = Pica::Regs::BytesPerDepthPixel(fb_depth_texture.format);

            // OpenGL needs 4 bpp alignment for D24
            u32 gl_bpp = bytes_per_pixel == 3 ? 4 : bytes_per_pixel;

            std::unique_ptr<u8[]> temp_gl_depth_buffer(new u8[fb_depth_texture.width * fb_depth_texture.height * gl_bpp]);

            state.texture_units[0].texture_2d = fb_depth_texture.texture.handle;
            state.Apply();

            glActiveTexture(GL_TEXTURE0);
            glGetTexImage(GL_TEXTURE_2D, 0, fb_depth_texture.gl_format, fb_depth_texture.gl_type, temp_gl_depth_buffer.get());

            state.texture_units[0].texture_2d = 0;
            state.Apply();

            u8* temp_gl_depth_data = bytes_per_pixel == 3 ? (temp_gl_depth_buffer.get() + 1) : temp_gl_depth_buffer.get();

            if (fb_depth_texture.format == Pica::Regs::DepthFormat::D24S8) {
                for (int y = 0; y < fb_depth_texture.height; ++y) {
                    for (int x = 0; x < fb_depth_texture.width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_depth_texture.width * bytes_per_pixel;
                        u32 gl_pixel_index = (x + (fb_depth_texture.height - 1 - y) * fb_depth_texture.width);

                        u8* pixel = depth_buffer + dst_offset;
                        u32 depth_stencil = ((u32*)temp_gl_depth_data)[gl_pixel_index];
                        *(u32*)pixel = (depth_stencil >> 8) | (depth_stencil << 24);
                    }
                }
            } else {
                for (int y = 0; y < fb_depth_texture.height; ++y) {
                    for (int x = 0; x < fb_depth_texture.width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * fb_depth_texture.width * bytes_per_pixel;
                        u32 gl_pixel_index = (x + (fb_depth_texture.height - 1 - y) * fb_depth_texture.width) * gl_bpp;

                        u8* pixel = depth_buffer + dst_offset;
                        memcpy(pixel, &temp_gl_depth_data[gl_pixel_index], bytes_per_pixel);
                    }
                }
            }
        }
    }
}
