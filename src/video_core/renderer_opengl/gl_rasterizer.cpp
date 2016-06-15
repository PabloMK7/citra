// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/color.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/vector_math.h"

#include "core/hw/gpu.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

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

RasterizerOpenGL::RasterizerOpenGL() : shader_dirty(true) {
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

    for (unsigned index = 0; index < lighting_luts.size(); index++) {
        uniform_block_data.lut_dirty[index] = true;
    }

    uniform_block_data.fog_lut_dirty = true;

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

    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD0_W, 1, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord0_w));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD0_W);

    glVertexAttribPointer(GLShader::ATTRIBUTE_NORMQUAT, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, normquat));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_NORMQUAT);

    glVertexAttribPointer(GLShader::ATTRIBUTE_VIEW, 3, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, view));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_VIEW);

    // Create render framebuffer
    framebuffer.Create();

    // Allocate and bind lighting lut textures
    for (size_t i = 0; i < lighting_luts.size(); ++i) {
        lighting_luts[i].Create();
        state.lighting_luts[i].texture_1d = lighting_luts[i].handle;
    }
    state.Apply();

    for (size_t i = 0; i < lighting_luts.size(); ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE3 + i));
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Setup the LUT for the fog
    {
        fog_lut.Create();
        state.fog_lut.texture_1d = fog_lut.handle;
    }
    state.Apply();

    glActiveTexture(GL_TEXTURE9);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32UI, 128, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Sync fixed function OpenGL state
    SyncCullMode();
    SyncBlendEnabled();
    SyncBlendFuncs();
    SyncBlendColor();
    SyncLogicOp();
    SyncStencilTest();
    SyncDepthTest();
    SyncColorWriteMask();
    SyncStencilWriteMask();
    SyncDepthWriteMask();
}

RasterizerOpenGL::~RasterizerOpenGL() {

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

    const auto& regs = Pica::g_state.regs;

    // Sync and bind the framebuffer surfaces
    CachedSurface* color_surface;
    CachedSurface* depth_surface;
    MathUtil::Rectangle<int> rect;
    std::tie(color_surface, depth_surface, rect) = res_cache.GetFramebufferSurfaces(regs.framebuffer);

    state.draw.draw_framebuffer = framebuffer.handle;
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_surface != nullptr ? color_surface->texture.handle : 0, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_surface != nullptr ? depth_surface->texture.handle : 0, 0);
    bool has_stencil = regs.framebuffer.depth_format == Pica::Regs::DepthFormat::D24S8;
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, (has_stencil && depth_surface != nullptr) ? depth_surface->texture.handle : 0, 0);

    if (OpenGLState::CheckFBStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        return;
    }

    // Sync the viewport
    // These registers hold half-width and half-height, so must be multiplied by 2
    GLsizei viewport_width = (GLsizei)Pica::float24::FromRaw(regs.viewport_size_x).ToFloat32() * 2;
    GLsizei viewport_height = (GLsizei)Pica::float24::FromRaw(regs.viewport_size_y).ToFloat32() * 2;

    glViewport((GLint)(rect.left + regs.viewport_corner.x * color_surface->res_scale_width),
               (GLint)(rect.bottom + regs.viewport_corner.y * color_surface->res_scale_height),
               (GLsizei)(viewport_width * color_surface->res_scale_width), (GLsizei)(viewport_height * color_surface->res_scale_height));

    // Sync and bind the texture surfaces
    const auto pica_textures = regs.GetTextures();
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        if (texture.enabled) {
            texture_samplers[texture_index].SyncWithConfig(texture.config);
            CachedSurface* surface = res_cache.GetTextureSurface(texture);
            if (surface != nullptr) {
                state.texture_units[texture_index].texture_2d = surface->texture.handle;
            } else {
                // Can occur when texture addr is null or its memory is unmapped/invalid
                state.texture_units[texture_index].texture_2d = 0;
            }
        } else {
            state.texture_units[texture_index].texture_2d = 0;
        }
    }

    // Sync and bind the shader
    if (shader_dirty) {
        SetShader();
        shader_dirty = false;
    }

    // Sync the lighting luts
    for (unsigned index = 0; index < lighting_luts.size(); index++) {
        if (uniform_block_data.lut_dirty[index]) {
            SyncLightingLUT(index);
            uniform_block_data.lut_dirty[index] = false;
        }
    }

    // Sync the fog lut
    if (uniform_block_data.fog_lut_dirty) {
        SyncFogLUT();
        uniform_block_data.fog_lut_dirty = false;
    }

    // Sync the uniform data
    if (uniform_block_data.dirty) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformData), &uniform_block_data.data, GL_STATIC_DRAW);
        uniform_block_data.dirty = false;
    }

    state.Apply();

    // Draw the vertex batch
    glBufferData(GL_ARRAY_BUFFER, vertex_batch.size() * sizeof(HardwareVertex), vertex_batch.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_batch.size());

    // Mark framebuffer surfaces as dirty
    // TODO: Restrict invalidation area to the viewport
    if (color_surface != nullptr) {
        color_surface->dirty = true;
        res_cache.FlushRegion(color_surface->addr, color_surface->size, color_surface, true);
    }
    if (depth_surface != nullptr) {
        depth_surface->dirty = true;
        res_cache.FlushRegion(depth_surface->addr, depth_surface->size, depth_surface, true);
    }

    vertex_batch.clear();

    // Unbind textures for potential future use as framebuffer attachments
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        state.texture_units[texture_index].texture_2d = 0;
    }
    state.Apply();
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
        SyncDepthScale();
        break;
    case PICA_REG_INDEX(viewport_depth_near_plane):
        SyncDepthOffset();
        break;

    // Depth buffering
    case PICA_REG_INDEX(depthmap_enable):
        shader_dirty = true;
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

    // Fog state
    case PICA_REG_INDEX(fog_color):
        SyncFogColor();
        break;
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[0], 0xe8):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[1], 0xe9):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[2], 0xea):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[3], 0xeb):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[4], 0xec):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[5], 0xed):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[6], 0xee):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[7], 0xef):
        uniform_block_data.fog_lut_dirty = true;
        break;

    // Alpha test
    case PICA_REG_INDEX(output_merger.alpha_test):
        SyncAlphaTest();
        shader_dirty = true;
        break;

    // Sync GL stencil test + stencil write mask
    // (Pica stencil test function register also contains a stencil write mask)
    case PICA_REG_INDEX(output_merger.stencil_test.raw_func):
        SyncStencilTest();
        SyncStencilWriteMask();
        break;
    case PICA_REG_INDEX(output_merger.stencil_test.raw_op):
    case PICA_REG_INDEX(framebuffer.depth_format):
        SyncStencilTest();
        break;

    // Sync GL depth test + depth and color write mask
    // (Pica depth test function register also contains a depth and color write mask)
    case PICA_REG_INDEX(output_merger.depth_test_enable):
        SyncDepthTest();
        SyncDepthWriteMask();
        SyncColorWriteMask();
        break;

    // Sync GL depth and stencil write mask
    // (This is a dedicated combined depth / stencil write-enable register)
    case PICA_REG_INDEX(framebuffer.allow_depth_stencil_write):
        SyncDepthWriteMask();
        SyncStencilWriteMask();
        break;

    // Sync GL color write mask
    // (This is a dedicated color write-enable register)
    case PICA_REG_INDEX(framebuffer.allow_color_write):
        SyncColorWriteMask();
        break;

    // Logic op
    case PICA_REG_INDEX(output_merger.logic_op):
        SyncLogicOp();
        break;

    // Texture 0 type
    case PICA_REG_INDEX(texture0.type):
        shader_dirty = true;
        break;

    // TEV stages
    // (This also syncs fog_mode and fog_flip which are part of tev_combiner_buffer_input)
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
        shader_dirty = true;
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

    // Fragment lighting switches
    case PICA_REG_INDEX(lighting.disable):
    case PICA_REG_INDEX(lighting.num_lights):
    case PICA_REG_INDEX(lighting.config0):
    case PICA_REG_INDEX(lighting.config1):
    case PICA_REG_INDEX(lighting.abs_lut_input):
    case PICA_REG_INDEX(lighting.lut_input):
    case PICA_REG_INDEX(lighting.lut_scale):
    case PICA_REG_INDEX(lighting.light_enable):
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

    // Fragment lighting light source config
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].config, 0x149 + 0 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].config, 0x149 + 1 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].config, 0x149 + 2 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].config, 0x149 + 3 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].config, 0x149 + 4 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].config, 0x149 + 5 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].config, 0x149 + 6 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].config, 0x149 + 7 * 0x10):
        shader_dirty = true;
        break;

    // Fragment lighting distance attenuation bias
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].dist_atten_bias, 0x014A + 0 * 0x10):
        SyncLightDistanceAttenuationBias(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].dist_atten_bias, 0x014A + 1 * 0x10):
        SyncLightDistanceAttenuationBias(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].dist_atten_bias, 0x014A + 2 * 0x10):
        SyncLightDistanceAttenuationBias(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].dist_atten_bias, 0x014A + 3 * 0x10):
        SyncLightDistanceAttenuationBias(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].dist_atten_bias, 0x014A + 4 * 0x10):
        SyncLightDistanceAttenuationBias(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].dist_atten_bias, 0x014A + 5 * 0x10):
        SyncLightDistanceAttenuationBias(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].dist_atten_bias, 0x014A + 6 * 0x10):
        SyncLightDistanceAttenuationBias(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].dist_atten_bias, 0x014A + 7 * 0x10):
        SyncLightDistanceAttenuationBias(7);
        break;

    // Fragment lighting distance attenuation scale
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].dist_atten_scale, 0x014B + 0 * 0x10):
        SyncLightDistanceAttenuationScale(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].dist_atten_scale, 0x014B + 1 * 0x10):
        SyncLightDistanceAttenuationScale(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].dist_atten_scale, 0x014B + 2 * 0x10):
        SyncLightDistanceAttenuationScale(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].dist_atten_scale, 0x014B + 3 * 0x10):
        SyncLightDistanceAttenuationScale(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].dist_atten_scale, 0x014B + 4 * 0x10):
        SyncLightDistanceAttenuationScale(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].dist_atten_scale, 0x014B + 5 * 0x10):
        SyncLightDistanceAttenuationScale(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].dist_atten_scale, 0x014B + 6 * 0x10):
        SyncLightDistanceAttenuationScale(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].dist_atten_scale, 0x014B + 7 * 0x10):
        SyncLightDistanceAttenuationScale(7);
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

void RasterizerOpenGL::FlushAll() {
    res_cache.FlushAll();
}

void RasterizerOpenGL::FlushRegion(PAddr addr, u32 size) {
    res_cache.FlushRegion(addr, size, nullptr, false);
}

void RasterizerOpenGL::FlushAndInvalidateRegion(PAddr addr, u32 size) {
    res_cache.FlushRegion(addr, size, nullptr, true);
}

bool RasterizerOpenGL::AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    using PixelFormat = CachedSurface::PixelFormat;
    using SurfaceType = CachedSurface::SurfaceType;

    if (config.is_texture_copy) {
        // TODO(tfarley): Try to hardware accelerate this
        return false;
    }

    CachedSurface src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    src_params.width = config.output_width;
    src_params.height = config.output_height;
    src_params.is_tiled = !config.input_linear;
    src_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.input_format);

    CachedSurface dst_params;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width = config.scaling != config.NoScale ? config.output_width / 2 : config.output_width.Value();
    dst_params.height = config.scaling == config.ScaleXY ? config.output_height / 2 : config.output_height.Value();
    dst_params.is_tiled = config.input_linear != config.dont_swizzle;
    dst_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.output_format);

    MathUtil::Rectangle<int> src_rect;
    CachedSurface* src_surface = res_cache.GetSurfaceRect(src_params, false, true, src_rect);

    if (src_surface == nullptr) {
        return false;
    }

    // Require destination surface to have same resolution scale as source to preserve scaling
    dst_params.res_scale_width = src_surface->res_scale_width;
    dst_params.res_scale_height = src_surface->res_scale_height;

    MathUtil::Rectangle<int> dst_rect;
    CachedSurface* dst_surface = res_cache.GetSurfaceRect(dst_params, true, false, dst_rect);

    if (dst_surface == nullptr) {
        return false;
    }

    // Don't accelerate if the src and dst surfaces are the same
    if (src_surface == dst_surface) {
        return false;
    }

    if (config.flip_vertically) {
        std::swap(dst_rect.top, dst_rect.bottom);
    }

    if (!res_cache.TryBlitSurfaces(src_surface, src_rect, dst_surface, dst_rect)) {
        return false;
    }

    u32 dst_size = dst_params.width * dst_params.height * CachedSurface::GetFormatBpp(dst_params.pixel_format) / 8;
    dst_surface->dirty = true;
    res_cache.FlushRegion(config.GetPhysicalOutputAddress(), dst_size, dst_surface, true);
    return true;
}

bool RasterizerOpenGL::AccelerateFill(const GPU::Regs::MemoryFillConfig& config) {
    using PixelFormat = CachedSurface::PixelFormat;
    using SurfaceType = CachedSurface::SurfaceType;

    CachedSurface* dst_surface = res_cache.TryGetFillSurface(config);

    if (dst_surface == nullptr) {
        return false;
    }

    OpenGLState cur_state = OpenGLState::GetCurState();

    SurfaceType dst_type = CachedSurface::GetFormatType(dst_surface->pixel_format);

    GLuint old_fb = cur_state.draw.draw_framebuffer;
    cur_state.draw.draw_framebuffer = framebuffer.handle;
    // TODO: When scissor test is implemented, need to disable scissor test in cur_state here so Clear call isn't affected
    cur_state.Apply();

    if (dst_type == SurfaceType::Color || dst_type == SurfaceType::Texture) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        if (OpenGLState::CheckFBStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }

        GLfloat color_values[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        // TODO: Handle additional pixel format and fill value size combinations to accelerate more cases
        //       For instance, checking if fill value's bytes/bits repeat to allow filling I8/A8/I4/A4/...
        //       Currently only handles formats that are multiples of the fill value size

        if (config.fill_24bit) {
            switch (dst_surface->pixel_format) {
            case PixelFormat::RGB8:
                color_values[0] = config.value_24bit_r / 255.0f;
                color_values[1] = config.value_24bit_g / 255.0f;
                color_values[2] = config.value_24bit_b / 255.0f;
                break;
            default:
                return false;
            }
        } else if (config.fill_32bit) {
            u32 value = config.value_32bit;

            switch (dst_surface->pixel_format) {
            case PixelFormat::RGBA8:
                color_values[0] = (value >> 24) / 255.0f;
                color_values[1] = ((value >> 16) & 0xFF) / 255.0f;
                color_values[2] = ((value >> 8) & 0xFF) / 255.0f;
                color_values[3] = (value & 0xFF) / 255.0f;
                break;
            default:
                return false;
            }
        } else {
            u16 value_16bit = config.value_16bit.Value();
            Math::Vec4<u8> color;

            switch (dst_surface->pixel_format) {
            case PixelFormat::RGBA8:
                color_values[0] = (value_16bit >> 8) / 255.0f;
                color_values[1] = (value_16bit & 0xFF) / 255.0f;
                color_values[2] = color_values[0];
                color_values[3] = color_values[1];
                break;
            case PixelFormat::RGB5A1:
                color = Color::DecodeRGB5A1((const u8*)&value_16bit);
                color_values[0] = color[0] / 31.0f;
                color_values[1] = color[1] / 31.0f;
                color_values[2] = color[2] / 31.0f;
                color_values[3] = color[3];
                break;
            case PixelFormat::RGB565:
                color = Color::DecodeRGB565((const u8*)&value_16bit);
                color_values[0] = color[0] / 31.0f;
                color_values[1] = color[1] / 63.0f;
                color_values[2] = color[2] / 31.0f;
                break;
            case PixelFormat::RGBA4:
                color = Color::DecodeRGBA4((const u8*)&value_16bit);
                color_values[0] = color[0] / 15.0f;
                color_values[1] = color[1] / 15.0f;
                color_values[2] = color[2] / 15.0f;
                color_values[3] = color[3] / 15.0f;
                break;
            case PixelFormat::IA8:
            case PixelFormat::RG8:
                color_values[0] = (value_16bit >> 8) / 255.0f;
                color_values[1] = (value_16bit & 0xFF) / 255.0f;
                break;
            default:
                return false;
            }
        }

        cur_state.color_mask.red_enabled = true;
        cur_state.color_mask.green_enabled = true;
        cur_state.color_mask.blue_enabled = true;
        cur_state.color_mask.alpha_enabled = true;
        cur_state.Apply();
        glClearBufferfv(GL_COLOR, 0, color_values);
    } else if (dst_type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dst_surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        if (OpenGLState::CheckFBStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }

        GLfloat value_float;
        if (dst_surface->pixel_format == CachedSurface::PixelFormat::D16) {
            value_float = config.value_32bit / 65535.0f; // 2^16 - 1
        } else if (dst_surface->pixel_format == CachedSurface::PixelFormat::D24) {
            value_float = config.value_32bit / 16777215.0f; // 2^24 - 1
        }

        cur_state.depth.write_mask = true;
        cur_state.Apply();
        glClearBufferfv(GL_DEPTH, 0, &value_float);
    } else if (dst_type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, dst_surface->texture.handle, 0);

        if (OpenGLState::CheckFBStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }

        GLfloat value_float = (config.value_32bit & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        GLint value_int = (config.value_32bit >> 24);

        cur_state.depth.write_mask = true;
        cur_state.stencil.write_mask = true;
        cur_state.Apply();
        glClearBufferfi(GL_DEPTH_STENCIL, 0, value_float, value_int);
    }

    cur_state.draw.draw_framebuffer = old_fb;
    // TODO: Return scissor test to previous value when scissor test is implemented
    cur_state.Apply();

    dst_surface->dirty = true;
    res_cache.FlushRegion(dst_surface->addr, dst_surface->size, dst_surface, true);
    return true;
}

bool RasterizerOpenGL::AccelerateDisplay(const GPU::Regs::FramebufferConfig& config, PAddr framebuffer_addr, u32 pixel_stride, ScreenInfo& screen_info) {
    if (framebuffer_addr == 0) {
        return false;
    }

    CachedSurface src_params;
    src_params.addr = framebuffer_addr;
    src_params.width = config.width;
    src_params.height = config.height;
    src_params.stride = pixel_stride;
    src_params.is_tiled = false;
    src_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.color_format);

    MathUtil::Rectangle<int> src_rect;
    CachedSurface* src_surface = res_cache.GetSurfaceRect(src_params, false, true, src_rect);

    if (src_surface == nullptr) {
        return false;
    }

    u32 scaled_width = src_surface->GetScaledWidth();
    u32 scaled_height = src_surface->GetScaledHeight();

    screen_info.display_texcoords = MathUtil::Rectangle<float>((float)src_rect.top / (float)scaled_height,
                                                               (float)src_rect.left / (float)scaled_width,
                                                               (float)src_rect.bottom / (float)scaled_height,
                                                               (float)src_rect.right / (float)scaled_width);

    screen_info.display_texture = src_surface->texture.handle;

    return true;
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
            border_color = config.border_color.raw;
            auto gl_color = PicaToGL::ColorRGBA8(border_color);
            glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, gl_color.data());
        }
    }
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

        GLuint uniform_fog_lut = glGetUniformLocation(shader->shader.handle, "fog_lut");
        if (uniform_fog_lut != -1) { glUniform1i(uniform_fog_lut, 9); }

        current_shader = shader_cache.emplace(config, std::move(shader)).first->second.get();

        unsigned int block_index = glGetUniformBlockIndex(current_shader->shader.handle, "shader_data");
        GLint block_size;
        glGetActiveUniformBlockiv(current_shader->shader.handle, block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &block_size);
        ASSERT_MSG(block_size == sizeof(UniformData), "Uniform block size did not match!");
        glUniformBlockBinding(current_shader->shader.handle, block_index, 0);

        // Update uniforms
        SyncDepthScale();
        SyncDepthOffset();
        SyncAlphaTest();
        SyncCombinerColor();
        auto& tev_stages = Pica::g_state.regs.GetTevStages();
        for (int index = 0; index < tev_stages.size(); ++index)
            SyncTevConstColor(index, tev_stages[index]);

        SyncGlobalAmbient();
        for (int light_index = 0; light_index < 8; light_index++) {
            SyncLightSpecular0(light_index);
            SyncLightSpecular1(light_index);
            SyncLightDiffuse(light_index);
            SyncLightAmbient(light_index);
            SyncLightPosition(light_index);
            SyncLightDistanceAttenuationBias(light_index);
            SyncLightDistanceAttenuationScale(light_index);
        }

        SyncFogColor();
    }
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

void RasterizerOpenGL::SyncDepthScale() {
    float depth_scale = Pica::float24::FromRaw(Pica::g_state.regs.viewport_depth_range).ToFloat32();
    if (depth_scale != uniform_block_data.data.depth_scale) {
        uniform_block_data.data.depth_scale = depth_scale;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncDepthOffset() {
    float depth_offset = Pica::float24::FromRaw(Pica::g_state.regs.viewport_depth_near_plane).ToFloat32();
    if (depth_offset != uniform_block_data.data.depth_offset) {
        uniform_block_data.data.depth_offset = depth_offset;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncBlendEnabled() {
    state.blend.enabled = (Pica::g_state.regs.output_merger.alphablend_enable == 1);
}

void RasterizerOpenGL::SyncBlendFuncs() {
    const auto& regs = Pica::g_state.regs;
    state.blend.rgb_equation = PicaToGL::BlendEquation(regs.output_merger.alpha_blending.blend_equation_rgb);
    state.blend.a_equation = PicaToGL::BlendEquation(regs.output_merger.alpha_blending.blend_equation_a);
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

void RasterizerOpenGL::SyncFogColor() {
    const auto& regs = Pica::g_state.regs;
    uniform_block_data.data.fog_color = {
      regs.fog_color.r.Value() / 255.0f,
      regs.fog_color.g.Value() / 255.0f,
      regs.fog_color.b.Value() / 255.0f
    };
    uniform_block_data.dirty = true;
}

void RasterizerOpenGL::SyncFogLUT() {
    std::array<GLuint, 128> new_data;

    std::transform(Pica::g_state.fog.lut.begin(), Pica::g_state.fog.lut.end(), new_data.begin(), [](const auto& entry) {
        return entry.raw;
    });

    if (new_data != fog_lut_data) {
        fog_lut_data = new_data;
        glActiveTexture(GL_TEXTURE9);
        glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 128, GL_RED_INTEGER, GL_UNSIGNED_INT, fog_lut_data.data());
    }
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

void RasterizerOpenGL::SyncColorWriteMask() {
    const auto& regs = Pica::g_state.regs;

    auto IsColorWriteEnabled = [&](u32 value) {
        return (regs.framebuffer.allow_color_write != 0 && value != 0) ? GL_TRUE : GL_FALSE;
    };

    state.color_mask.red_enabled = IsColorWriteEnabled(regs.output_merger.red_enable);
    state.color_mask.green_enabled = IsColorWriteEnabled(regs.output_merger.green_enable);
    state.color_mask.blue_enabled = IsColorWriteEnabled(regs.output_merger.blue_enable);
    state.color_mask.alpha_enabled = IsColorWriteEnabled(regs.output_merger.alpha_enable);
}

void RasterizerOpenGL::SyncStencilWriteMask() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.write_mask = (regs.framebuffer.allow_depth_stencil_write != 0)
                             ? static_cast<GLuint>(regs.output_merger.stencil_test.write_mask)
                             : 0;
}

void RasterizerOpenGL::SyncDepthWriteMask() {
    const auto& regs = Pica::g_state.regs;
    state.depth.write_mask = (regs.framebuffer.allow_depth_stencil_write != 0 && regs.output_merger.depth_write_enable)
                           ? GL_TRUE
                           : GL_FALSE;
}

void RasterizerOpenGL::SyncStencilTest() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.test_enabled = regs.output_merger.stencil_test.enable && regs.framebuffer.depth_format == Pica::Regs::DepthFormat::D24S8;
    state.stencil.test_func = PicaToGL::CompareFunc(regs.output_merger.stencil_test.func);
    state.stencil.test_ref = regs.output_merger.stencil_test.reference_value;
    state.stencil.test_mask = regs.output_merger.stencil_test.input_mask;
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

void RasterizerOpenGL::SyncLightDistanceAttenuationBias(int light_index) {
    GLfloat dist_atten_bias = Pica::float20::FromRaw(Pica::g_state.regs.lighting.light[light_index].dist_atten_bias).ToFloat32();

    if (dist_atten_bias != uniform_block_data.data.light_src[light_index].dist_atten_bias) {
        uniform_block_data.data.light_src[light_index].dist_atten_bias = dist_atten_bias;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightDistanceAttenuationScale(int light_index) {
    GLfloat dist_atten_scale = Pica::float20::FromRaw(Pica::g_state.regs.lighting.light[light_index].dist_atten_scale).ToFloat32();

    if (dist_atten_scale != uniform_block_data.data.light_src[light_index].dist_atten_scale) {
        uniform_block_data.data.light_src[light_index].dist_atten_scale = dist_atten_scale;
        uniform_block_data.dirty = true;
    }
}
