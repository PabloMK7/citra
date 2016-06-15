// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>
#include <unordered_map>

#include <glad/glad.h>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/hash.h"
#include "common/vector_math.h"

#include "core/hw/gpu.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/shader/shader.h"

struct ScreenInfo;

/**
 * This struct contains all state used to generate the GLSL shader program that emulates the current
 * Pica register configuration. This struct is used as a cache key for generated GLSL shader
 * programs. The functions in gl_shader_gen.cpp should retrieve state from this struct only, not by
 * directly accessing Pica registers. This should reduce the risk of bugs in shader generation where
 * Pica state is not being captured in the shader cache key, thereby resulting in (what should be)
 * two separate shaders sharing the same key.
 *
 * We use a union because "implicitly-defined copy/move constructor for a union X copies the object representation of X."
 * and "implicitly-defined copy assignment operator for a union X copies the object representation (3.9) of X."
 * = Bytewise copy instead of memberwise copy.
 * This is important because the padding bytes are included in the hash and comparison between objects.
 */
union PicaShaderConfig {

    /// Construct a PicaShaderConfig with the current Pica register configuration.
    static PicaShaderConfig CurrentConfig() {
        PicaShaderConfig res;

        auto& state = res.state;
        std::memset(&state, 0, sizeof(PicaShaderConfig::State));

        const auto& regs = Pica::g_state.regs;

        state.depthmap_enable = regs.depthmap_enable;

        state.alpha_test_func = regs.output_merger.alpha_test.enable ?
            regs.output_merger.alpha_test.func.Value() : Pica::Regs::CompareFunc::Always;

        state.texture0_type = regs.texture0.type;

        // Copy relevant tev stages fields.
        // We don't sync const_color here because of the high variance, it is a
        // shader uniform instead.
        const auto& tev_stages = regs.GetTevStages();
        DEBUG_ASSERT(state.tev_stages.size() == tev_stages.size());
        for (size_t i = 0; i < tev_stages.size(); i++) {
            const auto& tev_stage = tev_stages[i];
            state.tev_stages[i].sources_raw = tev_stage.sources_raw;
            state.tev_stages[i].modifiers_raw = tev_stage.modifiers_raw;
            state.tev_stages[i].ops_raw = tev_stage.ops_raw;
            state.tev_stages[i].scales_raw = tev_stage.scales_raw;
        }

        state.fog_mode = regs.fog_mode;
        state.fog_flip = regs.fog_flip;

        state.combiner_buffer_input =
            regs.tev_combiner_buffer_input.update_mask_rgb.Value() |
            regs.tev_combiner_buffer_input.update_mask_a.Value() << 4;

        // Fragment lighting

        state.lighting.enable = !regs.lighting.disable;
        state.lighting.src_num = regs.lighting.num_lights + 1;

        for (unsigned light_index = 0; light_index < state.lighting.src_num; ++light_index) {
            unsigned num = regs.lighting.light_enable.GetNum(light_index);
            const auto& light = regs.lighting.light[num];
            state.lighting.light[light_index].num = num;
            state.lighting.light[light_index].directional = light.config.directional != 0;
            state.lighting.light[light_index].two_sided_diffuse = light.config.two_sided_diffuse != 0;
            state.lighting.light[light_index].dist_atten_enable = !regs.lighting.IsDistAttenDisabled(num);
        }

        state.lighting.lut_d0.enable = regs.lighting.config1.disable_lut_d0 == 0;
        state.lighting.lut_d0.abs_input = regs.lighting.abs_lut_input.disable_d0 == 0;
        state.lighting.lut_d0.type = regs.lighting.lut_input.d0.Value();
        state.lighting.lut_d0.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d0);

        state.lighting.lut_d1.enable = regs.lighting.config1.disable_lut_d1 == 0;
        state.lighting.lut_d1.abs_input = regs.lighting.abs_lut_input.disable_d1 == 0;
        state.lighting.lut_d1.type = regs.lighting.lut_input.d1.Value();
        state.lighting.lut_d1.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d1);

        state.lighting.lut_fr.enable = regs.lighting.config1.disable_lut_fr == 0;
        state.lighting.lut_fr.abs_input = regs.lighting.abs_lut_input.disable_fr == 0;
        state.lighting.lut_fr.type = regs.lighting.lut_input.fr.Value();
        state.lighting.lut_fr.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.fr);

        state.lighting.lut_rr.enable = regs.lighting.config1.disable_lut_rr == 0;
        state.lighting.lut_rr.abs_input = regs.lighting.abs_lut_input.disable_rr == 0;
        state.lighting.lut_rr.type = regs.lighting.lut_input.rr.Value();
        state.lighting.lut_rr.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rr);

        state.lighting.lut_rg.enable = regs.lighting.config1.disable_lut_rg == 0;
        state.lighting.lut_rg.abs_input = regs.lighting.abs_lut_input.disable_rg == 0;
        state.lighting.lut_rg.type = regs.lighting.lut_input.rg.Value();
        state.lighting.lut_rg.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rg);

        state.lighting.lut_rb.enable = regs.lighting.config1.disable_lut_rb == 0;
        state.lighting.lut_rb.abs_input = regs.lighting.abs_lut_input.disable_rb == 0;
        state.lighting.lut_rb.type = regs.lighting.lut_input.rb.Value();
        state.lighting.lut_rb.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rb);

        state.lighting.config = regs.lighting.config0.config;
        state.lighting.fresnel_selector = regs.lighting.config0.fresnel_selector;
        state.lighting.bump_mode = regs.lighting.config0.bump_mode;
        state.lighting.bump_selector = regs.lighting.config0.bump_selector;
        state.lighting.bump_renorm = regs.lighting.config0.disable_bump_renorm == 0;
        state.lighting.clamp_highlights = regs.lighting.config0.clamp_highlights != 0;

        return res;
    }

    bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
        return (stage_index < 4) && (state.combiner_buffer_input & (1 << stage_index));
    }

    bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
        return (stage_index < 4) && ((state.combiner_buffer_input >> 4) & (1 << stage_index));
    }

    bool operator ==(const PicaShaderConfig& o) const {
        return std::memcmp(&state, &o.state, sizeof(PicaShaderConfig::State)) == 0;
    };

    // NOTE: MSVC15 (Update 2) doesn't think `delete`'d constructors and operators are TC.
    //       This makes BitField not TC when used in a union or struct so we have to resort
    //       to this ugly hack.
    //       Once that bug is fixed we can use Pica::Regs::TevStageConfig here.
    //       Doesn't include const_color because we don't sync it, see comment in CurrentConfig()
    struct TevStageConfigRaw {
        u32 sources_raw;
        u32 modifiers_raw;
        u32 ops_raw;
        u32 scales_raw;
        explicit operator Pica::Regs::TevStageConfig() const noexcept {
            Pica::Regs::TevStageConfig stage;
            stage.sources_raw = sources_raw;
            stage.modifiers_raw = modifiers_raw;
            stage.ops_raw = ops_raw;
            stage.const_color = 0;
            stage.scales_raw = scales_raw;
            return stage;
        }
    };

    struct State {
        Pica::Regs::CompareFunc alpha_test_func;
        Pica::Regs::TextureConfig::TextureType texture0_type;
        std::array<TevStageConfigRaw, 6> tev_stages;
        u8 combiner_buffer_input;

        Pica::Regs::DepthBuffering depthmap_enable;
        Pica::Regs::FogMode fog_mode;
        bool fog_flip;

        struct {
            struct {
                unsigned num;
                bool directional;
                bool two_sided_diffuse;
                bool dist_atten_enable;
            } light[8];

            bool enable;
            unsigned src_num;
            Pica::Regs::LightingBumpMode bump_mode;
            unsigned bump_selector;
            bool bump_renorm;
            bool clamp_highlights;

            Pica::Regs::LightingConfig config;
            Pica::Regs::LightingFresnelSelector fresnel_selector;

            struct {
                bool enable;
                bool abs_input;
                Pica::Regs::LightingLutInput type;
                float scale;
            } lut_d0, lut_d1, lut_fr, lut_rr, lut_rg, lut_rb;
        } lighting;

    } state;
};
#if (__GNUC__ >= 5) || defined(__clang__) || defined(_MSC_VER)
static_assert(std::is_trivially_copyable<PicaShaderConfig::State>::value, "PicaShaderConfig::State must be trivially copyable");
#endif

namespace std {

template <>
struct hash<PicaShaderConfig> {
    size_t operator()(const PicaShaderConfig& k) const {
        return Common::ComputeHash64(&k.state, sizeof(PicaShaderConfig::State));
    }
};

} // namespace std

class RasterizerOpenGL : public VideoCore::RasterizerInterface {
public:

    RasterizerOpenGL();
    ~RasterizerOpenGL() override;

    void AddTriangle(const Pica::Shader::OutputVertex& v0,
                     const Pica::Shader::OutputVertex& v1,
                     const Pica::Shader::OutputVertex& v2) override;
    void DrawTriangles() override;
    void NotifyPicaRegisterChanged(u32 id) override;
    void FlushAll() override;
    void FlushRegion(PAddr addr, u32 size) override;
    void FlushAndInvalidateRegion(PAddr addr, u32 size) override;
    bool AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) override;
    bool AccelerateFill(const GPU::Regs::MemoryFillConfig& config) override;
    bool AccelerateDisplay(const GPU::Regs::FramebufferConfig& config, PAddr framebuffer_addr, u32 pixel_stride, ScreenInfo& screen_info) override;

    /// OpenGL shader generated for a given Pica register state
    struct PicaShader {
        /// OpenGL shader resource
        OGLShader shader;
    };

private:

    struct SamplerInfo {
        using TextureConfig = Pica::Regs::TextureConfig;

        OGLSampler sampler;

        /// Creates the sampler object, initializing its state so that it's in sync with the SamplerInfo struct.
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
        GLfloat dist_atten_bias;
        GLfloat dist_atten_scale;
    };

    /// Uniform structure for the Uniform Buffer Object, all vectors must be 16-byte aligned
    // NOTE: Always keep a vec4 at the end. The GL spec is not clear wether the alignment at
    //       the end of a uniform block is included in UNIFORM_BLOCK_DATA_SIZE or not.
    //       Not following that rule will cause problems on some AMD drivers.
    struct UniformData {
        GLint alphatest_ref;
        GLfloat depth_scale;
        GLfloat depth_offset;
        alignas(16) GLvec3 fog_color;
        alignas(16) GLvec3 lighting_global_ambient;
        LightSrc light_src[8];
        alignas(16) GLvec4 const_color[6]; // A vec4 color for each of the six tev stages
        alignas(16) GLvec4 tev_combiner_buffer_color;
    };

    static_assert(sizeof(UniformData) == 0x3A0, "The size of the UniformData structure has changed, update the structure in the shader");
    static_assert(sizeof(UniformData) < 16384, "UniformData structure must be less than 16kb as per the OpenGL spec");

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
    void SyncTevConstColor(int tev_index, const Pica::Regs::TevStageConfig& tev_stage);

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

    /// Syncs the specified light's distance attenuation bias to match the PICA register
    void SyncLightDistanceAttenuationBias(int light_index);

    /// Syncs the specified light's distance attenuation scale to match the PICA register
    void SyncLightDistanceAttenuationScale(int light_index);

    OpenGLState state;

    RasterizerCacheOpenGL res_cache;

    std::vector<HardwareVertex> vertex_batch;

    std::unordered_map<PicaShaderConfig, std::unique_ptr<PicaShader>> shader_cache;
    const PicaShader* current_shader = nullptr;
    bool shader_dirty;

    struct {
        UniformData data;
        bool lut_dirty[6];
        bool fog_lut_dirty;
        bool dirty;
    } uniform_block_data = {};

    std::array<SamplerInfo, 3> texture_samplers;
    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLBuffer uniform_buffer;
    OGLFramebuffer framebuffer;

    std::array<OGLTexture, 6> lighting_luts;
    std::array<std::array<GLvec4, 256>, 6> lighting_lut_data{};

    OGLTexture fog_lut;
    std::array<GLuint, 128> fog_lut_data{};
};
