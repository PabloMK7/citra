// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <unordered_map>

#include "common/common_types.h"

#include "video_core/pica.h"
#include "video_core/hwrasterizer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/shader/shader_interpreter.h"

struct ShaderCacheKey {
    using Regs = Pica::Regs;

    bool operator ==(const ShaderCacheKey& o) const {
        return hash(*this) == hash(o);
    };

    Regs::CompareFunc alpha_test_func;
    std::array<Regs::TevStageConfig, 6> tev_stages = {};
    u8 combiner_buffer_input;

    bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
        return (stage_index < 4) && (combiner_buffer_input & (1 << stage_index));
    }

    bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
        return (stage_index < 4) && ((combiner_buffer_input >> 4) & (1 << stage_index));
    }

    static ShaderCacheKey CurrentConfig() {
        const auto& regs = Pica::g_state.regs;
        ShaderCacheKey config;

        config.alpha_test_func = regs.output_merger.alpha_test.enable ?
            regs.output_merger.alpha_test.func.Value() : Pica::Regs::CompareFunc::Always;

        config.tev_stages[0].source_raw = regs.tev_stage0.source_raw;
        config.tev_stages[1].source_raw = regs.tev_stage1.source_raw;
        config.tev_stages[2].source_raw = regs.tev_stage2.source_raw;
        config.tev_stages[3].source_raw = regs.tev_stage3.source_raw;
        config.tev_stages[4].source_raw = regs.tev_stage4.source_raw;
        config.tev_stages[5].source_raw = regs.tev_stage5.source_raw;

        config.tev_stages[0].modifier_raw = regs.tev_stage0.modifier_raw;
        config.tev_stages[1].modifier_raw = regs.tev_stage1.modifier_raw;
        config.tev_stages[2].modifier_raw = regs.tev_stage2.modifier_raw;
        config.tev_stages[3].modifier_raw = regs.tev_stage3.modifier_raw;
        config.tev_stages[4].modifier_raw = regs.tev_stage4.modifier_raw;
        config.tev_stages[5].modifier_raw = regs.tev_stage5.modifier_raw;

        config.tev_stages[0].op_raw = regs.tev_stage0.op_raw;
        config.tev_stages[1].op_raw = regs.tev_stage1.op_raw;
        config.tev_stages[2].op_raw = regs.tev_stage2.op_raw;
        config.tev_stages[3].op_raw = regs.tev_stage3.op_raw;
        config.tev_stages[4].op_raw = regs.tev_stage4.op_raw;
        config.tev_stages[5].op_raw = regs.tev_stage5.op_raw;

        config.tev_stages[0].scale_raw = regs.tev_stage0.scale_raw;
        config.tev_stages[1].scale_raw = regs.tev_stage1.scale_raw;
        config.tev_stages[2].scale_raw = regs.tev_stage2.scale_raw;
        config.tev_stages[3].scale_raw = regs.tev_stage3.scale_raw;
        config.tev_stages[4].scale_raw = regs.tev_stage4.scale_raw;
        config.tev_stages[5].scale_raw = regs.tev_stage5.scale_raw;

        config.combiner_buffer_input =
            regs.tev_combiner_buffer_input.update_mask_rgb.Value() |
            regs.tev_combiner_buffer_input.update_mask_a.Value() << 4;

        return config;
    }
};

namespace std {

template<> struct hash<::Pica::Regs::CompareFunc> {
    std::size_t operator()(const ::Pica::Regs::CompareFunc& o) {
        return ::hash((unsigned)o);
    }
};

template<> struct hash<::Pica::Regs::TevStageConfig> {
    std::size_t operator()(const ::Pica::Regs::TevStageConfig& o) {
        return ::combine_hash(
            ::hash(o.source_raw), ::hash(o.modifier_raw),
            ::hash(o.op_raw), ::hash(o.scale_raw));
    }
};

template<> struct hash<::ShaderCacheKey> {
    std::size_t operator()(const ::ShaderCacheKey& o) const {
        return ::combine_hash(o.alpha_test_func, o.combiner_buffer_input,
            o.tev_stages[0], o.tev_stages[1], o.tev_stages[2],
            o.tev_stages[3], o.tev_stages[4], o.tev_stages[5]);
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

private:
    /// Structure used for managing texture environment states
    struct TEVConfigUniforms {
        GLuint enabled;
        GLuint color_sources;
        GLuint alpha_sources;
        GLuint color_modifiers;
        GLuint alpha_modifiers;
        GLuint color_alpha_op;
        GLuint color_alpha_multiplier;
        GLuint const_color;
        GLuint updates_combiner_buffer_color_alpha;
    };

    struct TEVShader {
        OGLShader shader;

        // Hardware fragment shader
        GLuint uniform_alphatest_ref;
        GLuint uniform_tex;
        GLuint uniform_tev_combiner_buffer_color;
        GLuint uniform_tev_const_colors;

        TEVShader() = default;
        TEVShader(TEVShader&& o) : shader(std::move(o.shader)),
            uniform_alphatest_ref(o.uniform_alphatest_ref), uniform_tex(o.uniform_tex),
            uniform_tev_combiner_buffer_color(o.uniform_tev_combiner_buffer_color),
            uniform_tev_const_colors(o.uniform_tev_const_colors) {}
    };

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

    std::unordered_map<ShaderCacheKey, std::unique_ptr<TEVShader>> shader_cache;
    const TEVShader* current_shader = nullptr;

    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLFramebuffer framebuffer;
};
