// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
#include <glad/glad.h>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/hash.h"
#include "common/vector_math.h"
#include "core/hw/gpu.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_lighting.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_texturing.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/shader/shader.h"

struct ScreenInfo;

class RasterizerOpenGL : public VideoCore::RasterizerInterface {
public:
    RasterizerOpenGL();
    ~RasterizerOpenGL() override;

    void AddTriangle(const Pica::Shader::OutputVertex& v0, const Pica::Shader::OutputVertex& v1,
                     const Pica::Shader::OutputVertex& v2) override;
    void DrawTriangles() override;
    void NotifyPicaRegisterChanged(u32 id) override;
    void FlushAll() override;
    void FlushRegion(PAddr addr, u32 size) override;
    void FlushAndInvalidateRegion(PAddr addr, u32 size) override;
    bool AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) override;
    bool AccelerateTextureCopy(const GPU::Regs::DisplayTransferConfig& config) override;
    bool AccelerateFill(const GPU::Regs::MemoryFillConfig& config) override;
    bool AccelerateDisplay(const GPU::Regs::FramebufferConfig& config, PAddr framebuffer_addr,
                           u32 pixel_stride, ScreenInfo& screen_info) override;

    /// OpenGL shader generated for a given Pica register state
    struct PicaShader {
        /// OpenGL shader resource
        OGLShader shader;
    };

private:
    struct SamplerInfo {
        using TextureConfig = Pica::TexturingRegs::TextureConfig;

        OGLSampler sampler;

        /// Creates the sampler object, initializing its state so that it's in sync with the
        /// SamplerInfo struct.
        void Create();
        /// Syncs the sampler object with the config, updating any necessary state.
        void SyncWithConfig(const TextureConfig& config);

    private:
        TextureConfig::TextureFilter mag_filter;
        TextureConfig::TextureFilter min_filter;
        TextureConfig::WrapMode wrap_s;
        TextureConfig::WrapMode wrap_t;
        u32 border_color;
    };

    /// Structure that the hardware rendered vertices are composed of
    struct HardwareVertex {
        HardwareVertex(const Pica::Shader::OutputVertex& v, bool flip_quaternion) {
            position[0] = v.pos.x.ToFloat32();
            position[1] = v.pos.y.ToFloat32();
            position[2] = v.pos.z.ToFloat32();
            position[3] = v.pos.w.ToFloat32();
            color[0] = v.color.x.ToFloat32();
            color[1] = v.color.y.ToFloat32();
            color[2] = v.color.z.ToFloat32();
            color[3] = v.color.w.ToFloat32();
            tex_coord0[0] = v.tc0.x.ToFloat32();
            tex_coord0[1] = v.tc0.y.ToFloat32();
            tex_coord1[0] = v.tc1.x.ToFloat32();
            tex_coord1[1] = v.tc1.y.ToFloat32();
            tex_coord2[0] = v.tc2.x.ToFloat32();
            tex_coord2[1] = v.tc2.y.ToFloat32();
            tex_coord0_w = v.tc0_w.ToFloat32();
            normquat[0] = v.quat.x.ToFloat32();
            normquat[1] = v.quat.y.ToFloat32();
            normquat[2] = v.quat.z.ToFloat32();
            normquat[3] = v.quat.w.ToFloat32();
            view[0] = v.view.x.ToFloat32();
            view[1] = v.view.y.ToFloat32();
            view[2] = v.view.z.ToFloat32();

            if (flip_quaternion) {
                for (float& x : normquat) {
                    x = -x;
                }
            }
        }

        GLfloat position[4];
        GLfloat color[4];
        GLfloat tex_coord0[2];
        GLfloat tex_coord1[2];
        GLfloat tex_coord2[2];
        GLfloat tex_coord0_w;
        GLfloat normquat[4];
        GLfloat view[3];
    };

    struct LightSrc {
        alignas(16) GLvec3 specular_0;
        alignas(16) GLvec3 specular_1;
        alignas(16) GLvec3 diffuse;
        alignas(16) GLvec3 ambient;
        alignas(16) GLvec3 position;
        alignas(16) GLvec3 spot_direction; // negated
        GLfloat dist_atten_bias;
        GLfloat dist_atten_scale;
    };

    /// Uniform structure for the Uniform Buffer Object, all vectors must be 16-byte aligned
    // NOTE: Always keep a vec4 at the end. The GL spec is not clear wether the alignment at
    //       the end of a uniform block is included in UNIFORM_BLOCK_DATA_SIZE or not.
    //       Not following that rule will cause problems on some AMD drivers.
    struct UniformData {
        alignas(8) GLvec2 framebuffer_scale;
        GLint alphatest_ref;
        GLfloat depth_scale;
        GLfloat depth_offset;
        GLint scissor_x1;
        GLint scissor_y1;
        GLint scissor_x2;
        GLint scissor_y2;
        alignas(16) GLvec3 fog_color;
        alignas(8) GLvec2 proctex_noise_f;
        alignas(8) GLvec2 proctex_noise_a;
        alignas(8) GLvec2 proctex_noise_p;
        alignas(16) GLvec3 lighting_global_ambient;
        LightSrc light_src[8];
        alignas(16) GLvec4 const_color[6]; // A vec4 color for each of the six tev stages
        alignas(16) GLvec4 tev_combiner_buffer_color;
        alignas(16) GLvec4 clip_coef;
    };

    static_assert(
        sizeof(UniformData) == 0x470,
        "The size of the UniformData structure has changed, update the structure in the shader");
    static_assert(sizeof(UniformData) < 16384,
                  "UniformData structure must be less than 16kb as per the OpenGL spec");

    /// Syncs the clip enabled status to match the PICA register
    void SyncClipEnabled();

    /// Syncs the clip coefficients to match the PICA register
    void SyncClipCoef();

    /// Sets the OpenGL shader in accordance with the current PICA register state
    void SetShader();

    /// Syncs the cull mode to match the PICA register
    void SyncCullMode();

    /// Syncs the depth scale to match the PICA register
    void SyncDepthScale();

    /// Syncs the depth offset to match the PICA register
    void SyncDepthOffset();

    /// Syncs the blend enabled status to match the PICA register
    void SyncBlendEnabled();

    /// Syncs the blend functions to match the PICA register
    void SyncBlendFuncs();

    /// Syncs the blend color to match the PICA register
    void SyncBlendColor();

    /// Syncs the fog states to match the PICA register
    void SyncFogColor();
    void SyncFogLUT();

    /// Sync the procedural texture noise configuration to match the PICA register
    void SyncProcTexNoise();

    /// Sync the procedural texture lookup tables
    void SyncProcTexNoiseLUT();
    void SyncProcTexColorMap();
    void SyncProcTexAlphaMap();
    void SyncProcTexLUT();
    void SyncProcTexDiffLUT();

    /// Syncs the alpha test states to match the PICA register
    void SyncAlphaTest();

    /// Syncs the logic op states to match the PICA register
    void SyncLogicOp();

    /// Syncs the color write mask to match the PICA register state
    void SyncColorWriteMask();

    /// Syncs the stencil write mask to match the PICA register state
    void SyncStencilWriteMask();

    /// Syncs the depth write mask to match the PICA register state
    void SyncDepthWriteMask();

    /// Syncs the stencil test states to match the PICA register
    void SyncStencilTest();

    /// Syncs the depth test states to match the PICA register
    void SyncDepthTest();

    /// Syncs the TEV combiner color buffer to match the PICA register
    void SyncCombinerColor();

    /// Syncs the TEV constant color to match the PICA register
    void SyncTevConstColor(int tev_index, const Pica::TexturingRegs::TevStageConfig& tev_stage);

    /// Syncs the lighting global ambient color to match the PICA register
    void SyncGlobalAmbient();

    /// Syncs the lighting lookup tables
    void SyncLightingLUT(unsigned index);

    /// Syncs the specified light's specular 0 color to match the PICA register
    void SyncLightSpecular0(int light_index);

    /// Syncs the specified light's specular 1 color to match the PICA register
    void SyncLightSpecular1(int light_index);

    /// Syncs the specified light's diffuse color to match the PICA register
    void SyncLightDiffuse(int light_index);

    /// Syncs the specified light's ambient color to match the PICA register
    void SyncLightAmbient(int light_index);

    /// Syncs the specified light's position to match the PICA register
    void SyncLightPosition(int light_index);

    /// Syncs the specified spot light direcition to match the PICA register
    void SyncLightSpotDirection(int light_index);

    /// Syncs the specified light's distance attenuation bias to match the PICA register
    void SyncLightDistanceAttenuationBias(int light_index);

    /// Syncs the specified light's distance attenuation scale to match the PICA register
    void SyncLightDistanceAttenuationScale(int light_index);

    OpenGLState state;

    RasterizerCacheOpenGL res_cache;

    std::vector<HardwareVertex> vertex_batch;

    std::unordered_map<GLShader::PicaShaderConfig, std::unique_ptr<PicaShader>> shader_cache;
    const PicaShader* current_shader = nullptr;
    bool shader_dirty;

    struct {
        UniformData data;
        std::array<bool, Pica::LightingRegs::NumLightingSampler> lut_dirty;
        bool fog_lut_dirty;
        bool proctex_noise_lut_dirty;
        bool proctex_color_map_dirty;
        bool proctex_alpha_map_dirty;
        bool proctex_lut_dirty;
        bool proctex_diff_lut_dirty;
        bool dirty;
    } uniform_block_data = {};

    std::array<SamplerInfo, 3> texture_samplers;
    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLBuffer uniform_buffer;
    OGLFramebuffer framebuffer;

    OGLBuffer lighting_lut_buffer;
    OGLTexture lighting_lut;
    std::array<std::array<GLvec2, 256>, Pica::LightingRegs::NumLightingSampler> lighting_lut_data{};

    OGLBuffer fog_lut_buffer;
    OGLTexture fog_lut;
    std::array<GLvec2, 128> fog_lut_data{};

    OGLBuffer proctex_noise_lut_buffer;
    OGLTexture proctex_noise_lut;
    std::array<GLvec2, 128> proctex_noise_lut_data{};

    OGLBuffer proctex_color_map_buffer;
    OGLTexture proctex_color_map;
    std::array<GLvec2, 128> proctex_color_map_data{};

    OGLBuffer proctex_alpha_map_buffer;
    OGLTexture proctex_alpha_map;
    std::array<GLvec2, 128> proctex_alpha_map_data{};

    OGLBuffer proctex_lut_buffer;
    OGLTexture proctex_lut;
    std::array<GLvec4, 256> proctex_lut_data{};

    OGLBuffer proctex_diff_lut_buffer;
    OGLTexture proctex_diff_lut;
    std::array<GLvec4, 256> proctex_diff_lut_data{};
};
