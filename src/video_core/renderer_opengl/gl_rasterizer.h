// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>
#include <unordered_map>

#include "common/common_types.h"
#include "common/hash.h"

#include "video_core/pica.h"
#include "video_core/hwrasterizer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/shader/shader_interpreter.h"

/**
 * This struct contains all state used to generate the GLSL shader program that emulates the current
 * Pica register configuration. This struct is used as a cache key for generated GLSL shader
 * programs. The functions in gl_shader_gen.cpp should retrieve state from this struct only, not by
 * directly accessing Pica registers. This should reduce the risk of bugs in shader generation where
 * Pica state is not being captured in the shader cache key, thereby resulting in (what should be)
 * two separate shaders sharing the same key.
 */
struct PicaShaderConfig {
    /// Construct a PicaShaderConfig with the current Pica register configuration.
    static PicaShaderConfig CurrentConfig() {
        PicaShaderConfig res;
        const auto& regs = Pica::g_state.regs;

        res.alpha_test_func = regs.output_merger.alpha_test.enable ?
            regs.output_merger.alpha_test.func.Value() : Pica::Regs::CompareFunc::Always;

        // Copy relevant TevStageConfig fields only. We're doing this manually (instead of calling
        // the GetTevStages() function) because BitField explicitly disables copies.

        res.tev_stages[0].sources_raw = regs.tev_stage0.sources_raw;
        res.tev_stages[1].sources_raw = regs.tev_stage1.sources_raw;
        res.tev_stages[2].sources_raw = regs.tev_stage2.sources_raw;
        res.tev_stages[3].sources_raw = regs.tev_stage3.sources_raw;
        res.tev_stages[4].sources_raw = regs.tev_stage4.sources_raw;
        res.tev_stages[5].sources_raw = regs.tev_stage5.sources_raw;

        res.tev_stages[0].modifiers_raw = regs.tev_stage0.modifiers_raw;
        res.tev_stages[1].modifiers_raw = regs.tev_stage1.modifiers_raw;
        res.tev_stages[2].modifiers_raw = regs.tev_stage2.modifiers_raw;
        res.tev_stages[3].modifiers_raw = regs.tev_stage3.modifiers_raw;
        res.tev_stages[4].modifiers_raw = regs.tev_stage4.modifiers_raw;
        res.tev_stages[5].modifiers_raw = regs.tev_stage5.modifiers_raw;

        res.tev_stages[0].ops_raw = regs.tev_stage0.ops_raw;
        res.tev_stages[1].ops_raw = regs.tev_stage1.ops_raw;
        res.tev_stages[2].ops_raw = regs.tev_stage2.ops_raw;
        res.tev_stages[3].ops_raw = regs.tev_stage3.ops_raw;
        res.tev_stages[4].ops_raw = regs.tev_stage4.ops_raw;
        res.tev_stages[5].ops_raw = regs.tev_stage5.ops_raw;

        res.tev_stages[0].scales_raw = regs.tev_stage0.scales_raw;
        res.tev_stages[1].scales_raw = regs.tev_stage1.scales_raw;
        res.tev_stages[2].scales_raw = regs.tev_stage2.scales_raw;
        res.tev_stages[3].scales_raw = regs.tev_stage3.scales_raw;
        res.tev_stages[4].scales_raw = regs.tev_stage4.scales_raw;
        res.tev_stages[5].scales_raw = regs.tev_stage5.scales_raw;

        res.combiner_buffer_input =
            regs.tev_combiner_buffer_input.update_mask_rgb.Value() |
            regs.tev_combiner_buffer_input.update_mask_a.Value() << 4;

        return res;
    }

    bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
        return (stage_index < 4) && (combiner_buffer_input & (1 << stage_index));
    }

    bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
        return (stage_index < 4) && ((combiner_buffer_input >> 4) & (1 << stage_index));
    }

    bool operator ==(const PicaShaderConfig& o) const {
        return std::memcmp(this, &o, sizeof(PicaShaderConfig)) == 0;
    };

    Pica::Regs::CompareFunc alpha_test_func;
    std::array<Pica::Regs::TevStageConfig, 6> tev_stages = {};
    u8 combiner_buffer_input;
};

namespace std {

template <>
struct hash<PicaShaderConfig> {
    size_t operator()(const PicaShaderConfig& k) const {
        return Common::ComputeHash64(&k, sizeof(PicaShaderConfig));
    }
};

} // namespace std

class RasterizerOpenGL : public HWRasterizer {
public:

    RasterizerOpenGL();
    ~RasterizerOpenGL() override;

    /// Initialize API-specific GPU objects
    void InitObjects() override;

    /// Reset the rasterizer, such as flushing all caches and updating all state
    void Reset() override;

    /// Queues the primitive formed by the given vertices for rendering
    void AddTriangle(const Pica::Shader::OutputVertex& v0,
                     const Pica::Shader::OutputVertex& v1,
                     const Pica::Shader::OutputVertex& v2) override;

    /// Draw the current batch of triangles
    void DrawTriangles() override;

    /// Commit the rasterizer's framebuffer contents immediately to the current 3DS memory framebuffer
    void CommitFramebuffer() override;

    /// Notify rasterizer that the specified PICA register has been changed
    void NotifyPicaRegisterChanged(u32 id) override;

    /// Notify rasterizer that the specified 3DS memory region will be read from after this notification
    void NotifyPreRead(PAddr addr, u32 size) override;

    /// Notify rasterizer that a 3DS memory region has been changed
    void NotifyFlush(PAddr addr, u32 size) override;

    /// OpenGL shader generated for a given Pica register state
    struct PicaShader {
        /// OpenGL shader resource
        OGLShader shader;

        /// Fragment shader uniforms
        enum Uniform : GLuint {
            AlphaTestRef = 0,
            TevConstColors = 1,
            Texture0 = 7,
            Texture1 = 8,
            Texture2 = 9,
            TevCombinerBufferColor = 10,
        };
    };

private:

    /// Structure used for storing information about color textures
    struct TextureInfo {
        OGLTexture texture;
        GLsizei width;
        GLsizei height;
        Pica::Regs::ColorFormat format;
        GLenum gl_format;
        GLenum gl_type;
    };

    /// Structure used for storing information about depth textures
    struct DepthTextureInfo {
        OGLTexture texture;
        GLsizei width;
        GLsizei height;
        Pica::Regs::DepthFormat format;
        GLenum gl_format;
        GLenum gl_type;
    };

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
        HardwareVertex(const Pica::Shader::OutputVertex& v) {
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
        }

        GLfloat position[4];
        GLfloat color[4];
        GLfloat tex_coord0[2];
        GLfloat tex_coord1[2];
        GLfloat tex_coord2[2];
    };

    /// Reconfigure the OpenGL color texture to use the given format and dimensions
    void ReconfigureColorTexture(TextureInfo& texture, Pica::Regs::ColorFormat format, u32 width, u32 height);

    /// Reconfigure the OpenGL depth texture to use the given format and dimensions
    void ReconfigureDepthTexture(DepthTextureInfo& texture, Pica::Regs::DepthFormat format, u32 width, u32 height);

    /// Sets the OpenGL shader in accordance with the current PICA register state
    void SetShader();

    /// Syncs the state and contents of the OpenGL framebuffer to match the current PICA framebuffer
    void SyncFramebuffer();

    /// Syncs the cull mode to match the PICA register
    void SyncCullMode();

    /// Syncs the blend enabled status to match the PICA register
    void SyncBlendEnabled();

    /// Syncs the blend functions to match the PICA register
    void SyncBlendFuncs();

    /// Syncs the blend color to match the PICA register
    void SyncBlendColor();

    /// Syncs the alpha test states to match the PICA register
    void SyncAlphaTest();

    /// Syncs the logic op states to match the PICA register
    void SyncLogicOp();

    /// Syncs the stencil test states to match the PICA register
    void SyncStencilTest();

    /// Syncs the depth test states to match the PICA register
    void SyncDepthTest();

    /// Syncs the TEV constant color to match the PICA register
    void SyncTevConstColor(int tev_index, const Pica::Regs::TevStageConfig& tev_stage);

    /// Syncs the TEV combiner color buffer to match the PICA register
    void SyncCombinerColor();

    /// Syncs the remaining OpenGL drawing state to match the current PICA state
    void SyncDrawState();

    /// Copies the 3DS color framebuffer into the OpenGL color framebuffer texture
    void ReloadColorBuffer();

    /// Copies the 3DS depth framebuffer into the OpenGL depth framebuffer texture
    void ReloadDepthBuffer();

    /**
     * Save the current OpenGL color framebuffer to the current PICA framebuffer in 3DS memory
     * Loads the OpenGL framebuffer textures into temporary buffers
     * Then copies into the 3DS framebuffer using proper Morton order
     */
    void CommitColorBuffer();

    /**
     * Save the current OpenGL depth framebuffer to the current PICA framebuffer in 3DS memory
     * Loads the OpenGL framebuffer textures into temporary buffers
     * Then copies into the 3DS framebuffer using proper Morton order
     */
    void CommitDepthBuffer();

    RasterizerCacheOpenGL res_cache;

    std::vector<HardwareVertex> vertex_batch;

    OpenGLState state;

    PAddr last_fb_color_addr;
    PAddr last_fb_depth_addr;

    // Hardware rasterizer
    std::array<SamplerInfo, 3> texture_samplers;
    TextureInfo fb_color_texture;
    DepthTextureInfo fb_depth_texture;

    std::unordered_map<PicaShaderConfig, std::unique_ptr<PicaShader>> shader_cache;
    const PicaShader* current_shader = nullptr;

    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLFramebuffer framebuffer;
};
