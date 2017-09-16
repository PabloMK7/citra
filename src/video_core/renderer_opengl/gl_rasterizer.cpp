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
#include "common/microprofile.h"
#include "common/vector_math.h"
#include "core/hw/gpu.h"
#include "video_core/pica_state.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_texturing.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

MICROPROFILE_DEFINE(OpenGL_Drawing, "OpenGL", "Drawing", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Blits, "OpenGL", "Blits", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(OpenGL_CacheManagement, "OpenGL", "Cache Mgmt", MP_RGB(100, 255, 100));

RasterizerOpenGL::RasterizerOpenGL() : shader_dirty(true) {
    // Clipping plane 0 is always enabled for PICA fixed clip plane z <= 0
    state.clip_distance[0] = true;

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

    uniform_block_data.lut_dirty.fill(true);

    uniform_block_data.fog_lut_dirty = true;

    uniform_block_data.proctex_noise_lut_dirty = true;
    uniform_block_data.proctex_color_map_dirty = true;
    uniform_block_data.proctex_alpha_map_dirty = true;
    uniform_block_data.proctex_lut_dirty = true;
    uniform_block_data.proctex_diff_lut_dirty = true;

    // Set vertex attributes
    glVertexAttribPointer(GLShader::ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, position));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_POSITION);

    glVertexAttribPointer(GLShader::ATTRIBUTE_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, color));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_COLOR);

    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord0));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD1, 2, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord1));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord2));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD0);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD1);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD2);

    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD0_W, 1, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord0_w));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD0_W);

    glVertexAttribPointer(GLShader::ATTRIBUTE_NORMQUAT, 4, GL_FLOAT, GL_FALSE,
                          sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, normquat));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_NORMQUAT);

    glVertexAttribPointer(GLShader::ATTRIBUTE_VIEW, 3, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, view));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_VIEW);

    // Create render framebuffer
    framebuffer.Create();

    // Allocate and bind lighting lut textures
    lighting_lut.Create();
    state.lighting_lut.texture_buffer = lighting_lut.handle;
    state.Apply();
    lighting_lut_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, lighting_lut_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER,
                 sizeof(GLfloat) * 2 * 256 * Pica::LightingRegs::NumLightingSampler, nullptr,
                 GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::LightingLUT.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, lighting_lut_buffer.handle);

    // Setup the LUT for the fog
    fog_lut.Create();
    state.fog_lut.texture_buffer = fog_lut.handle;
    state.Apply();
    fog_lut_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, fog_lut_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 2 * 128, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::FogLUT.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, fog_lut_buffer.handle);

    // Setup the noise LUT for proctex
    proctex_noise_lut.Create();
    state.proctex_noise_lut.texture_buffer = proctex_noise_lut.handle;
    state.Apply();
    proctex_noise_lut_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, proctex_noise_lut_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 2 * 128, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::ProcTexNoiseLUT.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, proctex_noise_lut_buffer.handle);

    // Setup the color map for proctex
    proctex_color_map.Create();
    state.proctex_color_map.texture_buffer = proctex_color_map.handle;
    state.Apply();
    proctex_color_map_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, proctex_color_map_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 2 * 128, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::ProcTexColorMap.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, proctex_color_map_buffer.handle);

    // Setup the alpha map for proctex
    proctex_alpha_map.Create();
    state.proctex_alpha_map.texture_buffer = proctex_alpha_map.handle;
    state.Apply();
    proctex_alpha_map_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, proctex_alpha_map_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 2 * 128, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::ProcTexAlphaMap.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, proctex_alpha_map_buffer.handle);

    // Setup the LUT for proctex
    proctex_lut.Create();
    state.proctex_lut.texture_buffer = proctex_lut.handle;
    state.Apply();
    proctex_lut_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, proctex_lut_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 4 * 256, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::ProcTexLUT.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, proctex_lut_buffer.handle);

    // Setup the difference LUT for proctex
    proctex_diff_lut.Create();
    state.proctex_diff_lut.texture_buffer = proctex_diff_lut.handle;
    state.Apply();
    proctex_diff_lut_buffer.Create();
    glBindBuffer(GL_TEXTURE_BUFFER, proctex_diff_lut_buffer.handle);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * 4 * 256, nullptr, GL_DYNAMIC_DRAW);
    glActiveTexture(TextureUnits::ProcTexDiffLUT.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, proctex_diff_lut_buffer.handle);

    // Sync fixed function OpenGL state
    SyncClipEnabled();
    SyncClipCoef();
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

RasterizerOpenGL::~RasterizerOpenGL() {}

/**
 * This is a helper function to resolve an issue when interpolating opposite quaternions. See below
 * for a detailed description of this issue (yuriks):
 *
 * For any rotation, there are two quaternions Q, and -Q, that represent the same rotation. If you
 * interpolate two quaternions that are opposite, instead of going from one rotation to another
 * using the shortest path, you'll go around the longest path. You can test if two quaternions are
 * opposite by checking if Dot(Q1, Q2) < 0. In that case, you can flip either of them, therefore
 * making Dot(Q1, -Q2) positive.
 *
 * This solution corrects this issue per-vertex before passing the quaternions to OpenGL. This is
 * correct for most cases but can still rotate around the long way sometimes. An implementation
 * which did `lerp(lerp(Q1, Q2), Q3)` (with proper weighting), applying the dot product check
 * between each step would work for those cases at the cost of being more complex to implement.
 *
 * Fortunately however, the 3DS hardware happens to also use this exact same logic to work around
 * these issues, making this basic implementation actually more accurate to the hardware.
 */
static bool AreQuaternionsOpposite(Math::Vec4<Pica::float24> qa, Math::Vec4<Pica::float24> qb) {
    Math::Vec4f a{qa.x.ToFloat32(), qa.y.ToFloat32(), qa.z.ToFloat32(), qa.w.ToFloat32()};
    Math::Vec4f b{qb.x.ToFloat32(), qb.y.ToFloat32(), qb.z.ToFloat32(), qb.w.ToFloat32()};

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

    MICROPROFILE_SCOPE(OpenGL_Drawing);
    const auto& regs = Pica::g_state.regs;

    // Sync and bind the framebuffer surfaces
    CachedSurface* color_surface;
    CachedSurface* depth_surface;
    MathUtil::Rectangle<int> rect;
    std::tie(color_surface, depth_surface, rect) =
        res_cache.GetFramebufferSurfaces(regs.framebuffer.framebuffer);

    state.draw.draw_framebuffer = framebuffer.handle;
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_surface != nullptr ? color_surface->texture.handle : 0, 0);
    if (depth_surface != nullptr) {
        if (regs.framebuffer.framebuffer.depth_format ==
            Pica::FramebufferRegs::DepthFormat::D24S8) {
            // attach both depth and stencil
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   depth_surface->texture.handle, 0);
        } else {
            // attach depth
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                   depth_surface->texture.handle, 0);
            // clear stencil attachment
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
    } else {
        // clear both depth and stencil attachment
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);
    }

    // Sync the viewport
    // These registers hold half-width and half-height, so must be multiplied by 2
    GLsizei viewport_width =
        (GLsizei)Pica::float24::FromRaw(regs.rasterizer.viewport_size_x).ToFloat32() * 2;
    GLsizei viewport_height =
        (GLsizei)Pica::float24::FromRaw(regs.rasterizer.viewport_size_y).ToFloat32() * 2;

    glViewport(
        (GLint)(rect.left + regs.rasterizer.viewport_corner.x * color_surface->res_scale_width),
        (GLint)(rect.bottom + regs.rasterizer.viewport_corner.y * color_surface->res_scale_height),
        (GLsizei)(viewport_width * color_surface->res_scale_width),
        (GLsizei)(viewport_height * color_surface->res_scale_height));

    if (uniform_block_data.data.framebuffer_scale[0] != color_surface->res_scale_width ||
        uniform_block_data.data.framebuffer_scale[1] != color_surface->res_scale_height) {

        uniform_block_data.data.framebuffer_scale[0] = color_surface->res_scale_width;
        uniform_block_data.data.framebuffer_scale[1] = color_surface->res_scale_height;
        uniform_block_data.dirty = true;
    }

    // Scissor checks are window-, not viewport-relative, which means that if the cached texture
    // sub-rect changes, the scissor bounds also need to be updated.
    GLint scissor_x1 = static_cast<GLint>(
        rect.left + regs.rasterizer.scissor_test.x1 * color_surface->res_scale_width);
    GLint scissor_y1 = static_cast<GLint>(
        rect.bottom + regs.rasterizer.scissor_test.y1 * color_surface->res_scale_height);
    // x2, y2 have +1 added to cover the entire pixel area, otherwise you might get cracks when
    // scaling or doing multisampling.
    GLint scissor_x2 = static_cast<GLint>(
        rect.left + (regs.rasterizer.scissor_test.x2 + 1) * color_surface->res_scale_width);
    GLint scissor_y2 = static_cast<GLint>(
        rect.bottom + (regs.rasterizer.scissor_test.y2 + 1) * color_surface->res_scale_height);

    if (uniform_block_data.data.scissor_x1 != scissor_x1 ||
        uniform_block_data.data.scissor_x2 != scissor_x2 ||
        uniform_block_data.data.scissor_y1 != scissor_y1 ||
        uniform_block_data.data.scissor_y2 != scissor_y2) {

        uniform_block_data.data.scissor_x1 = scissor_x1;
        uniform_block_data.data.scissor_x2 = scissor_x2;
        uniform_block_data.data.scissor_y1 = scissor_y1;
        uniform_block_data.data.scissor_y2 = scissor_y2;
        uniform_block_data.dirty = true;
    }

    // Sync and bind the texture surfaces
    const auto pica_textures = regs.texturing.GetTextures();
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
    for (unsigned index = 0; index < uniform_block_data.lut_dirty.size(); index++) {
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

    // Sync the proctex noise lut
    if (uniform_block_data.proctex_noise_lut_dirty) {
        SyncProcTexNoiseLUT();
        uniform_block_data.proctex_noise_lut_dirty = false;
    }

    // Sync the proctex color map
    if (uniform_block_data.proctex_color_map_dirty) {
        SyncProcTexColorMap();
        uniform_block_data.proctex_color_map_dirty = false;
    }

    // Sync the proctex alpha map
    if (uniform_block_data.proctex_alpha_map_dirty) {
        SyncProcTexAlphaMap();
        uniform_block_data.proctex_alpha_map_dirty = false;
    }

    // Sync the proctex lut
    if (uniform_block_data.proctex_lut_dirty) {
        SyncProcTexLUT();
        uniform_block_data.proctex_lut_dirty = false;
    }

    // Sync the proctex difference lut
    if (uniform_block_data.proctex_diff_lut_dirty) {
        SyncProcTexDiffLUT();
        uniform_block_data.proctex_diff_lut_dirty = false;
    }

    // Sync the uniform data
    if (uniform_block_data.dirty) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformData), &uniform_block_data.data,
                     GL_STATIC_DRAW);
        uniform_block_data.dirty = false;
    }

    state.Apply();

    // Draw the vertex batch
    glBufferData(GL_ARRAY_BUFFER, vertex_batch.size() * sizeof(HardwareVertex), vertex_batch.data(),
                 GL_STREAM_DRAW);
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

    switch (id) {
    // Culling
    case PICA_REG_INDEX(rasterizer.cull_mode):
        SyncCullMode();
        break;

    // Clipping plane
    case PICA_REG_INDEX(rasterizer.clip_enable):
        SyncClipEnabled();
        break;

    case PICA_REG_INDEX_WORKAROUND(rasterizer.clip_coef[0], 0x48):
    case PICA_REG_INDEX_WORKAROUND(rasterizer.clip_coef[1], 0x49):
    case PICA_REG_INDEX_WORKAROUND(rasterizer.clip_coef[2], 0x4a):
    case PICA_REG_INDEX_WORKAROUND(rasterizer.clip_coef[3], 0x4b):
        SyncClipCoef();
        break;

    // Depth modifiers
    case PICA_REG_INDEX(rasterizer.viewport_depth_range):
        SyncDepthScale();
        break;
    case PICA_REG_INDEX(rasterizer.viewport_depth_near_plane):
        SyncDepthOffset();
        break;

    // Depth buffering
    case PICA_REG_INDEX(rasterizer.depthmap_enable):
        shader_dirty = true;
        break;

    // Blending
    case PICA_REG_INDEX(framebuffer.output_merger.alphablend_enable):
        SyncBlendEnabled();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.alpha_blending):
        SyncBlendFuncs();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.blend_const):
        SyncBlendColor();
        break;

    // Fog state
    case PICA_REG_INDEX(texturing.fog_color):
        SyncFogColor();
        break;
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[0], 0xe8):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[1], 0xe9):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[2], 0xea):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[3], 0xeb):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[4], 0xec):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[5], 0xed):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[6], 0xee):
    case PICA_REG_INDEX_WORKAROUND(texturing.fog_lut_data[7], 0xef):
        uniform_block_data.fog_lut_dirty = true;
        break;

    // ProcTex state
    case PICA_REG_INDEX(texturing.proctex):
    case PICA_REG_INDEX(texturing.proctex_lut):
    case PICA_REG_INDEX(texturing.proctex_lut_offset):
        shader_dirty = true;
        break;

    case PICA_REG_INDEX(texturing.proctex_noise_u):
    case PICA_REG_INDEX(texturing.proctex_noise_v):
    case PICA_REG_INDEX(texturing.proctex_noise_frequency):
        SyncProcTexNoise();
        break;

    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[0], 0xb0):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[1], 0xb1):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[2], 0xb2):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[3], 0xb3):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[4], 0xb4):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[5], 0xb5):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[6], 0xb6):
    case PICA_REG_INDEX_WORKAROUND(texturing.proctex_lut_data[7], 0xb7):
        using Pica::TexturingRegs;
        switch (regs.texturing.proctex_lut_config.ref_table.Value()) {
        case TexturingRegs::ProcTexLutTable::Noise:
            uniform_block_data.proctex_noise_lut_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::ColorMap:
            uniform_block_data.proctex_color_map_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::AlphaMap:
            uniform_block_data.proctex_alpha_map_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::Color:
            uniform_block_data.proctex_lut_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::ColorDiff:
            uniform_block_data.proctex_diff_lut_dirty = true;
            break;
        }
        break;

    // Alpha test
    case PICA_REG_INDEX(framebuffer.output_merger.alpha_test):
        SyncAlphaTest();
        shader_dirty = true;
        break;

    // Sync GL stencil test + stencil write mask
    // (Pica stencil test function register also contains a stencil write mask)
    case PICA_REG_INDEX(framebuffer.output_merger.stencil_test.raw_func):
        SyncStencilTest();
        SyncStencilWriteMask();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.stencil_test.raw_op):
    case PICA_REG_INDEX(framebuffer.framebuffer.depth_format):
        SyncStencilTest();
        break;

    // Sync GL depth test + depth and color write mask
    // (Pica depth test function register also contains a depth and color write mask)
    case PICA_REG_INDEX(framebuffer.output_merger.depth_test_enable):
        SyncDepthTest();
        SyncDepthWriteMask();
        SyncColorWriteMask();
        break;

    // Sync GL depth and stencil write mask
    // (This is a dedicated combined depth / stencil write-enable register)
    case PICA_REG_INDEX(framebuffer.framebuffer.allow_depth_stencil_write):
        SyncDepthWriteMask();
        SyncStencilWriteMask();
        break;

    // Sync GL color write mask
    // (This is a dedicated color write-enable register)
    case PICA_REG_INDEX(framebuffer.framebuffer.allow_color_write):
        SyncColorWriteMask();
        break;

    // Scissor test
    case PICA_REG_INDEX(rasterizer.scissor_test.mode):
        shader_dirty = true;
        break;

    // Logic op
    case PICA_REG_INDEX(framebuffer.output_merger.logic_op):
        SyncLogicOp();
        break;

    case PICA_REG_INDEX(texturing.main_config):
        shader_dirty = true;
        break;

    // Texture 0 type
    case PICA_REG_INDEX(texturing.texture0.type):
        shader_dirty = true;
        break;

    // TEV stages
    // (This also syncs fog_mode and fog_flip which are part of tev_combiner_buffer_input)
    case PICA_REG_INDEX(texturing.tev_stage0.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage0.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage0.color_op):
    case PICA_REG_INDEX(texturing.tev_stage0.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage1.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage1.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage1.color_op):
    case PICA_REG_INDEX(texturing.tev_stage1.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage2.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage2.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage2.color_op):
    case PICA_REG_INDEX(texturing.tev_stage2.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage3.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage3.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage3.color_op):
    case PICA_REG_INDEX(texturing.tev_stage3.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage4.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage4.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage4.color_op):
    case PICA_REG_INDEX(texturing.tev_stage4.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage5.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage5.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage5.color_op):
    case PICA_REG_INDEX(texturing.tev_stage5.color_scale):
    case PICA_REG_INDEX(texturing.tev_combiner_buffer_input):
        shader_dirty = true;
        break;
    case PICA_REG_INDEX(texturing.tev_stage0.const_r):
        SyncTevConstColor(0, regs.texturing.tev_stage0);
        break;
    case PICA_REG_INDEX(texturing.tev_stage1.const_r):
        SyncTevConstColor(1, regs.texturing.tev_stage1);
        break;
    case PICA_REG_INDEX(texturing.tev_stage2.const_r):
        SyncTevConstColor(2, regs.texturing.tev_stage2);
        break;
    case PICA_REG_INDEX(texturing.tev_stage3.const_r):
        SyncTevConstColor(3, regs.texturing.tev_stage3);
        break;
    case PICA_REG_INDEX(texturing.tev_stage4.const_r):
        SyncTevConstColor(4, regs.texturing.tev_stage4);
        break;
    case PICA_REG_INDEX(texturing.tev_stage5.const_r):
        SyncTevConstColor(5, regs.texturing.tev_stage5);
        break;

    // TEV combiner buffer color
    case PICA_REG_INDEX(texturing.tev_combiner_buffer_color):
        SyncCombinerColor();
        break;

    // Fragment lighting switches
    case PICA_REG_INDEX(lighting.disable):
    case PICA_REG_INDEX(lighting.max_light_index):
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

    // Fragment spot lighting direction
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].spot_x, 0x146 + 0 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[0].spot_z, 0x147 + 0 * 0x10):
        SyncLightSpotDirection(0);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].spot_x, 0x146 + 1 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[1].spot_z, 0x147 + 1 * 0x10):
        SyncLightSpotDirection(1);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].spot_x, 0x146 + 2 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[2].spot_z, 0x147 + 2 * 0x10):
        SyncLightSpotDirection(2);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].spot_x, 0x146 + 3 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[3].spot_z, 0x147 + 3 * 0x10):
        SyncLightSpotDirection(3);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].spot_x, 0x146 + 4 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[4].spot_z, 0x147 + 4 * 0x10):
        SyncLightSpotDirection(4);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].spot_x, 0x146 + 5 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[5].spot_z, 0x147 + 5 * 0x10):
        SyncLightSpotDirection(5);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].spot_x, 0x146 + 6 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[6].spot_z, 0x147 + 6 * 0x10):
        SyncLightSpotDirection(6);
        break;
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].spot_x, 0x146 + 7 * 0x10):
    case PICA_REG_INDEX_WORKAROUND(lighting.light[7].spot_z, 0x147 + 7 * 0x10):
        SyncLightSpotDirection(7);
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
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[7], 0x1cf): {
        auto& lut_config = regs.lighting.lut_config;
        uniform_block_data.lut_dirty[lut_config.type] = true;
        break;
    }
    }
}

void RasterizerOpenGL::FlushAll() {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushAll();
}

void RasterizerOpenGL::FlushRegion(PAddr addr, u32 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushRegion(addr, size, nullptr, false);
}

void RasterizerOpenGL::FlushAndInvalidateRegion(PAddr addr, u32 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushRegion(addr, size, nullptr, true);
}

bool RasterizerOpenGL::AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);

    CachedSurface src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    // It's important to use the correct source input width to properly skip over parts of the input
    // image which will be cropped from the output but still affect the stride of the input image.
    src_params.width = config.input_width;
    // Using the output's height is fine because we don't read or skip over the remaining part of
    // the image, and it allows for smaller texture cache lookup rectangles.
    src_params.height = config.output_height;
    src_params.is_tiled = !config.input_linear;
    src_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.input_format);

    CachedSurface dst_params;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width =
        config.scaling != config.NoScale ? config.output_width / 2 : config.output_width.Value();
    dst_params.height =
        config.scaling == config.ScaleXY ? config.output_height / 2 : config.output_height.Value();
    dst_params.is_tiled = config.input_linear != config.dont_swizzle;
    dst_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.output_format);

    MathUtil::Rectangle<int> src_rect;
    CachedSurface* src_surface = res_cache.GetSurfaceRect(src_params, false, true, src_rect);

    if (src_surface == nullptr) {
        return false;
    }

    // Adjust the source rectangle to take into account parts of the input lines being cropped
    if (config.input_width > config.output_width) {
        src_rect.right -= static_cast<int>((config.input_width - config.output_width) *
                                           src_surface->res_scale_width);
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

    u32 dst_size = dst_params.width * dst_params.height *
                   CachedSurface::GetFormatBpp(dst_params.pixel_format) / 8;
    dst_surface->dirty = true;
    res_cache.FlushRegion(config.GetPhysicalOutputAddress(), dst_size, dst_surface, true);
    return true;
}

bool RasterizerOpenGL::AccelerateTextureCopy(const GPU::Regs::DisplayTransferConfig& config) {
    // TODO(tfarley): Try to hardware accelerate this
    return false;
}

bool RasterizerOpenGL::AccelerateFill(const GPU::Regs::MemoryFillConfig& config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);
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
    // TODO: When scissor test is implemented, need to disable scissor test in cur_state here so
    // Clear call isn't affected
    cur_state.Apply();

    if (dst_type == SurfaceType::Color || dst_type == SurfaceType::Texture) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               dst_surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        GLfloat color_values[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        // TODO: Handle additional pixel format and fill value size combinations to accelerate more
        // cases
        //       For instance, checking if fill value's bytes/bits repeat to allow filling
        //       I8/A8/I4/A4/...
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

        cur_state.color_mask.red_enabled = GL_TRUE;
        cur_state.color_mask.green_enabled = GL_TRUE;
        cur_state.color_mask.blue_enabled = GL_TRUE;
        cur_state.color_mask.alpha_enabled = GL_TRUE;
        cur_state.Apply();
        glClearBufferfv(GL_COLOR, 0, color_values);
    } else if (dst_type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               dst_surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        GLfloat value_float;
        if (dst_surface->pixel_format == CachedSurface::PixelFormat::D16) {
            value_float = config.value_32bit / 65535.0f; // 2^16 - 1
        } else if (dst_surface->pixel_format == CachedSurface::PixelFormat::D24) {
            value_float = config.value_32bit / 16777215.0f; // 2^24 - 1
        }

        cur_state.depth.write_mask = GL_TRUE;
        cur_state.Apply();
        glClearBufferfv(GL_DEPTH, 0, &value_float);
    } else if (dst_type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               dst_surface->texture.handle, 0);

        GLfloat value_float = (config.value_32bit & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        GLint value_int = (config.value_32bit >> 24);

        cur_state.depth.write_mask = GL_TRUE;
        cur_state.stencil.write_mask = 0xFF;
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

bool RasterizerOpenGL::AccelerateDisplay(const GPU::Regs::FramebufferConfig& config,
                                         PAddr framebuffer_addr, u32 pixel_stride,
                                         ScreenInfo& screen_info) {
    if (framebuffer_addr == 0) {
        return false;
    }
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);

    CachedSurface src_params;
    src_params.addr = framebuffer_addr;
    src_params.width = config.width;
    src_params.height = config.height;
    src_params.pixel_stride = pixel_stride;
    src_params.is_tiled = false;
    src_params.pixel_format = CachedSurface::PixelFormatFromGPUPixelFormat(config.color_format);

    MathUtil::Rectangle<int> src_rect;
    CachedSurface* src_surface = res_cache.GetSurfaceRect(src_params, false, true, src_rect);

    if (src_surface == nullptr) {
        return false;
    }

    u32 scaled_width = src_surface->GetScaledWidth();
    u32 scaled_height = src_surface->GetScaledHeight();

    screen_info.display_texcoords = MathUtil::Rectangle<float>(
        (float)src_rect.top / (float)scaled_height, (float)src_rect.left / (float)scaled_width,
        (float)src_rect.bottom / (float)scaled_height, (float)src_rect.right / (float)scaled_width);

    screen_info.display_texture = src_surface->texture.handle;

    return true;
}

void RasterizerOpenGL::SamplerInfo::Create() {
    sampler.Create();
    mag_filter = min_filter = TextureConfig::Linear;
    wrap_s = wrap_t = TextureConfig::Repeat;
    border_color = 0;

    glSamplerParameteri(sampler.handle, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR); // default is GL_LINEAR_MIPMAP_LINEAR
    // Other attributes have correct defaults
}

void RasterizerOpenGL::SamplerInfo::SyncWithConfig(
    const Pica::TexturingRegs::TextureConfig& config) {

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
    auto config = GLShader::PicaShaderConfig::BuildFromRegs(Pica::g_state.regs);
    std::unique_ptr<PicaShader> shader = std::make_unique<PicaShader>();

    // Find (or generate) the GLSL shader for the current TEV state
    auto cached_shader = shader_cache.find(config);
    if (cached_shader != shader_cache.end()) {
        current_shader = cached_shader->second.get();

        state.draw.shader_program = current_shader->shader.handle;
        state.Apply();
    } else {
        LOG_DEBUG(Render_OpenGL, "Creating new shader");

        shader->shader.Create(GLShader::GenerateVertexShader().c_str(),
                              GLShader::GenerateFragmentShader(config).c_str());

        state.draw.shader_program = shader->shader.handle;
        state.Apply();

        // Set the texture samplers to correspond to different texture units
        GLint uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[0]");
        if (uniform_tex != -1) {
            glUniform1i(uniform_tex, TextureUnits::PicaTexture(0).id);
        }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[1]");
        if (uniform_tex != -1) {
            glUniform1i(uniform_tex, TextureUnits::PicaTexture(1).id);
        }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[2]");
        if (uniform_tex != -1) {
            glUniform1i(uniform_tex, TextureUnits::PicaTexture(2).id);
        }

        // Set the texture samplers to correspond to different lookup table texture units
        GLint uniform_lut = glGetUniformLocation(shader->shader.handle, "lighting_lut");
        if (uniform_lut != -1) {
            glUniform1i(uniform_lut, TextureUnits::LightingLUT.id);
        }

        GLint uniform_fog_lut = glGetUniformLocation(shader->shader.handle, "fog_lut");
        if (uniform_fog_lut != -1) {
            glUniform1i(uniform_fog_lut, TextureUnits::FogLUT.id);
        }

        GLint uniform_proctex_noise_lut =
            glGetUniformLocation(shader->shader.handle, "proctex_noise_lut");
        if (uniform_proctex_noise_lut != -1) {
            glUniform1i(uniform_proctex_noise_lut, TextureUnits::ProcTexNoiseLUT.id);
        }

        GLint uniform_proctex_color_map =
            glGetUniformLocation(shader->shader.handle, "proctex_color_map");
        if (uniform_proctex_color_map != -1) {
            glUniform1i(uniform_proctex_color_map, TextureUnits::ProcTexColorMap.id);
        }

        GLint uniform_proctex_alpha_map =
            glGetUniformLocation(shader->shader.handle, "proctex_alpha_map");
        if (uniform_proctex_alpha_map != -1) {
            glUniform1i(uniform_proctex_alpha_map, TextureUnits::ProcTexAlphaMap.id);
        }

        GLint uniform_proctex_lut = glGetUniformLocation(shader->shader.handle, "proctex_lut");
        if (uniform_proctex_lut != -1) {
            glUniform1i(uniform_proctex_lut, TextureUnits::ProcTexLUT.id);
        }

        GLint uniform_proctex_diff_lut =
            glGetUniformLocation(shader->shader.handle, "proctex_diff_lut");
        if (uniform_proctex_diff_lut != -1) {
            glUniform1i(uniform_proctex_diff_lut, TextureUnits::ProcTexDiffLUT.id);
        }

        current_shader = shader_cache.emplace(config, std::move(shader)).first->second.get();

        GLuint block_index = glGetUniformBlockIndex(current_shader->shader.handle, "shader_data");
        if (block_index != GL_INVALID_INDEX) {
            GLint block_size;
            glGetActiveUniformBlockiv(current_shader->shader.handle, block_index,
                                      GL_UNIFORM_BLOCK_DATA_SIZE, &block_size);
            ASSERT_MSG(block_size == sizeof(UniformData),
                       "Uniform block size did not match! Got %d, expected %zu",
                       static_cast<int>(block_size), sizeof(UniformData));
            glUniformBlockBinding(current_shader->shader.handle, block_index, 0);

            // Update uniforms
            SyncDepthScale();
            SyncDepthOffset();
            SyncAlphaTest();
            SyncCombinerColor();
            auto& tev_stages = Pica::g_state.regs.texturing.GetTevStages();
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
            SyncProcTexNoise();
        }
    }
}

void RasterizerOpenGL::SyncClipEnabled() {
    state.clip_distance[1] = Pica::g_state.regs.rasterizer.clip_enable != 0;
}

void RasterizerOpenGL::SyncClipCoef() {
    const auto raw_clip_coef = Pica::g_state.regs.rasterizer.GetClipCoef();
    const GLvec4 new_clip_coef = {raw_clip_coef.x.ToFloat32(), raw_clip_coef.y.ToFloat32(),
                                  raw_clip_coef.z.ToFloat32(), raw_clip_coef.w.ToFloat32()};
    if (new_clip_coef != uniform_block_data.data.clip_coef) {
        uniform_block_data.data.clip_coef = new_clip_coef;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncCullMode() {
    const auto& regs = Pica::g_state.regs;

    switch (regs.rasterizer.cull_mode) {
    case Pica::RasterizerRegs::CullMode::KeepAll:
        state.cull.enabled = false;
        break;

    case Pica::RasterizerRegs::CullMode::KeepClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CW;
        break;

    case Pica::RasterizerRegs::CullMode::KeepCounterClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CCW;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown cull mode %d", regs.rasterizer.cull_mode.Value());
        UNIMPLEMENTED();
        break;
    }
}

void RasterizerOpenGL::SyncDepthScale() {
    float depth_scale =
        Pica::float24::FromRaw(Pica::g_state.regs.rasterizer.viewport_depth_range).ToFloat32();
    if (depth_scale != uniform_block_data.data.depth_scale) {
        uniform_block_data.data.depth_scale = depth_scale;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncDepthOffset() {
    float depth_offset =
        Pica::float24::FromRaw(Pica::g_state.regs.rasterizer.viewport_depth_near_plane).ToFloat32();
    if (depth_offset != uniform_block_data.data.depth_offset) {
        uniform_block_data.data.depth_offset = depth_offset;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncBlendEnabled() {
    state.blend.enabled = (Pica::g_state.regs.framebuffer.output_merger.alphablend_enable == 1);
}

void RasterizerOpenGL::SyncBlendFuncs() {
    const auto& regs = Pica::g_state.regs;
    state.blend.rgb_equation =
        PicaToGL::BlendEquation(regs.framebuffer.output_merger.alpha_blending.blend_equation_rgb);
    state.blend.a_equation =
        PicaToGL::BlendEquation(regs.framebuffer.output_merger.alpha_blending.blend_equation_a);
    state.blend.src_rgb_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_source_rgb);
    state.blend.dst_rgb_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_dest_rgb);
    state.blend.src_a_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_source_a);
    state.blend.dst_a_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_dest_a);
}

void RasterizerOpenGL::SyncBlendColor() {
    auto blend_color =
        PicaToGL::ColorRGBA8(Pica::g_state.regs.framebuffer.output_merger.blend_const.raw);
    state.blend.color.red = blend_color[0];
    state.blend.color.green = blend_color[1];
    state.blend.color.blue = blend_color[2];
    state.blend.color.alpha = blend_color[3];
}

void RasterizerOpenGL::SyncFogColor() {
    const auto& regs = Pica::g_state.regs;
    uniform_block_data.data.fog_color = {
        regs.texturing.fog_color.r.Value() / 255.0f, regs.texturing.fog_color.g.Value() / 255.0f,
        regs.texturing.fog_color.b.Value() / 255.0f,
    };
    uniform_block_data.dirty = true;
}

void RasterizerOpenGL::SyncFogLUT() {
    std::array<GLvec2, 128> new_data;

    std::transform(Pica::g_state.fog.lut.begin(), Pica::g_state.fog.lut.end(), new_data.begin(),
                   [](const auto& entry) {
                       return GLvec2{entry.ToFloat(), entry.DiffToFloat()};
                   });

    if (new_data != fog_lut_data) {
        fog_lut_data = new_data;
        glBindBuffer(GL_TEXTURE_BUFFER, fog_lut_buffer.handle);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, new_data.size() * sizeof(GLvec2), new_data.data());
    }
}

void RasterizerOpenGL::SyncProcTexNoise() {
    const auto& regs = Pica::g_state.regs.texturing;
    uniform_block_data.data.proctex_noise_f = {
        Pica::float16::FromRaw(regs.proctex_noise_frequency.u).ToFloat32(),
        Pica::float16::FromRaw(regs.proctex_noise_frequency.v).ToFloat32(),
    };
    uniform_block_data.data.proctex_noise_a = {
        regs.proctex_noise_u.amplitude / 4095.0f, regs.proctex_noise_v.amplitude / 4095.0f,
    };
    uniform_block_data.data.proctex_noise_p = {
        Pica::float16::FromRaw(regs.proctex_noise_u.phase).ToFloat32(),
        Pica::float16::FromRaw(regs.proctex_noise_v.phase).ToFloat32(),
    };

    uniform_block_data.dirty = true;
}

// helper function for SyncProcTexNoiseLUT/ColorMap/AlphaMap
static void SyncProcTexValueLUT(const std::array<Pica::State::ProcTex::ValueEntry, 128>& lut,
                                std::array<GLvec2, 128>& lut_data, GLuint buffer) {
    std::array<GLvec2, 128> new_data;
    std::transform(lut.begin(), lut.end(), new_data.begin(), [](const auto& entry) {
        return GLvec2{entry.ToFloat(), entry.DiffToFloat()};
    });

    if (new_data != lut_data) {
        lut_data = new_data;
        glBindBuffer(GL_TEXTURE_BUFFER, buffer);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, new_data.size() * sizeof(GLvec2), new_data.data());
    }
}

void RasterizerOpenGL::SyncProcTexNoiseLUT() {
    SyncProcTexValueLUT(Pica::g_state.proctex.noise_table, proctex_noise_lut_data,
                        proctex_noise_lut_buffer.handle);
}

void RasterizerOpenGL::SyncProcTexColorMap() {
    SyncProcTexValueLUT(Pica::g_state.proctex.color_map_table, proctex_color_map_data,
                        proctex_color_map_buffer.handle);
}

void RasterizerOpenGL::SyncProcTexAlphaMap() {
    SyncProcTexValueLUT(Pica::g_state.proctex.alpha_map_table, proctex_alpha_map_data,
                        proctex_alpha_map_buffer.handle);
}

void RasterizerOpenGL::SyncProcTexLUT() {
    std::array<GLvec4, 256> new_data;

    std::transform(Pica::g_state.proctex.color_table.begin(),
                   Pica::g_state.proctex.color_table.end(), new_data.begin(),
                   [](const auto& entry) {
                       auto rgba = entry.ToVector() / 255.0f;
                       return GLvec4{rgba.r(), rgba.g(), rgba.b(), rgba.a()};
                   });

    if (new_data != proctex_lut_data) {
        proctex_lut_data = new_data;
        glBindBuffer(GL_TEXTURE_BUFFER, proctex_lut_buffer.handle);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, new_data.size() * sizeof(GLvec4), new_data.data());
    }
}

void RasterizerOpenGL::SyncProcTexDiffLUT() {
    std::array<GLvec4, 256> new_data;

    std::transform(Pica::g_state.proctex.color_diff_table.begin(),
                   Pica::g_state.proctex.color_diff_table.end(), new_data.begin(),
                   [](const auto& entry) {
                       auto rgba = entry.ToVector() / 255.0f;
                       return GLvec4{rgba.r(), rgba.g(), rgba.b(), rgba.a()};
                   });

    if (new_data != proctex_diff_lut_data) {
        proctex_diff_lut_data = new_data;
        glBindBuffer(GL_TEXTURE_BUFFER, proctex_diff_lut_buffer.handle);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, new_data.size() * sizeof(GLvec4), new_data.data());
    }
}

void RasterizerOpenGL::SyncAlphaTest() {
    const auto& regs = Pica::g_state.regs;
    if (regs.framebuffer.output_merger.alpha_test.ref != uniform_block_data.data.alphatest_ref) {
        uniform_block_data.data.alphatest_ref = regs.framebuffer.output_merger.alpha_test.ref;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLogicOp() {
    state.logic_op = PicaToGL::LogicOp(Pica::g_state.regs.framebuffer.output_merger.logic_op);
}

void RasterizerOpenGL::SyncColorWriteMask() {
    const auto& regs = Pica::g_state.regs;

    auto IsColorWriteEnabled = [&](u32 value) {
        return (regs.framebuffer.framebuffer.allow_color_write != 0 && value != 0) ? GL_TRUE
                                                                                   : GL_FALSE;
    };

    state.color_mask.red_enabled = IsColorWriteEnabled(regs.framebuffer.output_merger.red_enable);
    state.color_mask.green_enabled =
        IsColorWriteEnabled(regs.framebuffer.output_merger.green_enable);
    state.color_mask.blue_enabled = IsColorWriteEnabled(regs.framebuffer.output_merger.blue_enable);
    state.color_mask.alpha_enabled =
        IsColorWriteEnabled(regs.framebuffer.output_merger.alpha_enable);
}

void RasterizerOpenGL::SyncStencilWriteMask() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.write_mask =
        (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0)
            ? static_cast<GLuint>(regs.framebuffer.output_merger.stencil_test.write_mask)
            : 0;
}

void RasterizerOpenGL::SyncDepthWriteMask() {
    const auto& regs = Pica::g_state.regs;
    state.depth.write_mask = (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0 &&
                              regs.framebuffer.output_merger.depth_write_enable)
                                 ? GL_TRUE
                                 : GL_FALSE;
}

void RasterizerOpenGL::SyncStencilTest() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.test_enabled =
        regs.framebuffer.output_merger.stencil_test.enable &&
        regs.framebuffer.framebuffer.depth_format == Pica::FramebufferRegs::DepthFormat::D24S8;
    state.stencil.test_func =
        PicaToGL::CompareFunc(regs.framebuffer.output_merger.stencil_test.func);
    state.stencil.test_ref = regs.framebuffer.output_merger.stencil_test.reference_value;
    state.stencil.test_mask = regs.framebuffer.output_merger.stencil_test.input_mask;
    state.stencil.action_stencil_fail =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_stencil_fail);
    state.stencil.action_depth_fail =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_depth_fail);
    state.stencil.action_depth_pass =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_depth_pass);
}

void RasterizerOpenGL::SyncDepthTest() {
    const auto& regs = Pica::g_state.regs;
    state.depth.test_enabled = regs.framebuffer.output_merger.depth_test_enable == 1 ||
                               regs.framebuffer.output_merger.depth_write_enable == 1;
    state.depth.test_func =
        regs.framebuffer.output_merger.depth_test_enable == 1
            ? PicaToGL::CompareFunc(regs.framebuffer.output_merger.depth_test_func)
            : GL_ALWAYS;
}

void RasterizerOpenGL::SyncCombinerColor() {
    auto combiner_color =
        PicaToGL::ColorRGBA8(Pica::g_state.regs.texturing.tev_combiner_buffer_color.raw);
    if (combiner_color != uniform_block_data.data.tev_combiner_buffer_color) {
        uniform_block_data.data.tev_combiner_buffer_color = combiner_color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncTevConstColor(int stage_index,
                                         const Pica::TexturingRegs::TevStageConfig& tev_stage) {
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
    std::array<GLvec2, 256> new_data;
    const auto& source_lut = Pica::g_state.lighting.luts[lut_index];
    std::transform(source_lut.begin(), source_lut.end(), new_data.begin(), [](const auto& entry) {
        return GLvec2{entry.ToFloat(), entry.DiffToFloat()};
    });

    if (new_data != lighting_lut_data[lut_index]) {
        lighting_lut_data[lut_index] = new_data;
        glBindBuffer(GL_TEXTURE_BUFFER, lighting_lut_buffer.handle);
        glBufferSubData(GL_TEXTURE_BUFFER, lut_index * new_data.size() * sizeof(GLvec2),
                        new_data.size() * sizeof(GLvec2), new_data.data());
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
        Pica::float16::FromRaw(Pica::g_state.regs.lighting.light[light_index].z).ToFloat32()};

    if (position != uniform_block_data.data.light_src[light_index].position) {
        uniform_block_data.data.light_src[light_index].position = position;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightSpotDirection(int light_index) {
    const auto& light = Pica::g_state.regs.lighting.light[light_index];
    GLvec3 spot_direction = {light.spot_x / 2047.0f, light.spot_y / 2047.0f,
                             light.spot_z / 2047.0f};

    if (spot_direction != uniform_block_data.data.light_src[light_index].spot_direction) {
        uniform_block_data.data.light_src[light_index].spot_direction = spot_direction;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightDistanceAttenuationBias(int light_index) {
    GLfloat dist_atten_bias =
        Pica::float20::FromRaw(Pica::g_state.regs.lighting.light[light_index].dist_atten_bias)
            .ToFloat32();

    if (dist_atten_bias != uniform_block_data.data.light_src[light_index].dist_atten_bias) {
        uniform_block_data.data.light_src[light_index].dist_atten_bias = dist_atten_bias;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLightDistanceAttenuationScale(int light_index) {
    GLfloat dist_atten_scale =
        Pica::float20::FromRaw(Pica::g_state.regs.lighting.light[light_index].dist_atten_scale)
            .ToFloat32();

    if (dist_atten_scale != uniform_block_data.data.light_src[light_index].dist_atten_scale) {
        uniform_block_data.data.light_src[light_index].dist_atten_scale = dist_atten_scale;
        uniform_block_data.dirty = true;
    }
}
