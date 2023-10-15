// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/hash.h"
#include "video_core/regs.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader::Generator {

// NOTE: Changing the order impacts shader transferable and precompiled cache loading.
enum ProgramType : u32 {
    VS = 0,
    FS = 1,
    GS = 2,
};

enum Attributes {
    ATTRIBUTE_POSITION,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_TEXCOORD0,
    ATTRIBUTE_TEXCOORD1,
    ATTRIBUTE_TEXCOORD2,
    ATTRIBUTE_TEXCOORD0_W,
    ATTRIBUTE_NORMQUAT,
    ATTRIBUTE_VIEW,
};

// Doesn't include const_color because we don't sync it, see comment in BuildFromRegs()
struct TevStageConfigRaw {
    u32 sources_raw;
    u32 modifiers_raw;
    u32 ops_raw;
    u32 scales_raw;
    explicit operator Pica::TexturingRegs::TevStageConfig() const noexcept {
        return {
            .sources_raw = sources_raw,
            .modifiers_raw = modifiers_raw,
            .ops_raw = ops_raw,
            .const_color = 0,
            .scales_raw = scales_raw,
        };
    }
};

struct PicaFSConfigState {
    union {
        BitField<0, 3, Pica::FramebufferRegs::CompareFunc> alpha_test_func;
        BitField<3, 2, Pica::RasterizerRegs::ScissorMode> scissor_test_mode;
        BitField<5, 3, Pica::TexturingRegs::TextureConfig::TextureType> texture0_type;
        BitField<8, 1, u32> texture2_use_coord1;
        BitField<9, 8, u32> combiner_buffer_input;
        BitField<17, 1, Pica::RasterizerRegs::DepthBuffering> depthmap_enable;
        BitField<18, 3, Pica::TexturingRegs::FogMode> fog_mode;
        BitField<21, 1, u32> fog_flip;
        BitField<22, 1, u32> emulate_logic_op;
        BitField<23, 4, Pica::FramebufferRegs::LogicOp> logic_op;
        BitField<27, 1, u32> shadow_rendering;
        BitField<28, 1, u32> shadow_texture_orthographic;
        BitField<29, 1, u32> use_fragment_shader_interlock;
        BitField<30, 1, u32> use_custom_normal_map;
    };

    union {
        BitField<0, 1, u32> enable_s;
        BitField<1, 1, u32> enable_t;
    } texture_border_color[3];

    std::array<TevStageConfigRaw, 6> tev_stages;

    struct {
        union {
            BitField<0, 3, u16> num;
            BitField<3, 1, u16> directional;
            BitField<4, 1, u16> two_sided_diffuse;
            BitField<5, 1, u16> dist_atten_enable;
            BitField<6, 1, u16> spot_atten_enable;
            BitField<7, 1, u16> geometric_factor_0;
            BitField<8, 1, u16> geometric_factor_1;
            BitField<9, 1, u16> shadow_enable;
        } light[8];

        union {
            BitField<0, 1, u32> enable;
            BitField<1, 4, u32> src_num;
            BitField<5, 2, Pica::LightingRegs::LightingBumpMode> bump_mode;
            BitField<7, 2, u32> bump_selector;
            BitField<9, 1, u32> bump_renorm;
            BitField<10, 1, u32> clamp_highlights;
            BitField<11, 4, Pica::LightingRegs::LightingConfig> config;
            BitField<15, 1, u32> enable_primary_alpha;
            BitField<16, 1, u32> enable_secondary_alpha;
            BitField<17, 1, u32> enable_shadow;
            BitField<18, 1, u32> shadow_primary;
            BitField<19, 1, u32> shadow_secondary;
            BitField<20, 1, u32> shadow_invert;
            BitField<21, 1, u32> shadow_alpha;
            BitField<22, 2, u32> shadow_selector;
        };

        struct {
            union {
                BitField<0, 1, u32> enable;
                BitField<1, 1, u32> abs_input;
                BitField<2, 3, Pica::LightingRegs::LightingLutInput> type;
            };
            float scale;
        } lut_d0, lut_d1, lut_sp, lut_fr, lut_rr, lut_rg, lut_rb;
    } lighting;

    struct {
        union {
            BitField<0, 1, u32> enable;
            BitField<1, 2, u32> coord;
            BitField<3, 3, Pica::TexturingRegs::ProcTexClamp> u_clamp;
            BitField<6, 3, Pica::TexturingRegs::ProcTexClamp> v_clamp;
            BitField<9, 4, Pica::TexturingRegs::ProcTexCombiner> color_combiner;
            BitField<13, 4, Pica::TexturingRegs::ProcTexCombiner> alpha_combiner;
            BitField<17, 3, Pica::TexturingRegs::ProcTexFilter> lut_filter;
            BitField<20, 1, u32> separate_alpha;
            BitField<21, 1, u32> noise_enable;
            BitField<22, 2, Pica::TexturingRegs::ProcTexShift> u_shift;
            BitField<24, 2, Pica::TexturingRegs::ProcTexShift> v_shift;
        };
        s32 lut_width;
        s32 lut_offset0;
        s32 lut_offset1;
        s32 lut_offset2;
        s32 lut_offset3;
        u8 lod_min;
        u8 lod_max;
    } proctex;

    struct {
        bool emulate_blending;
        Pica::FramebufferRegs::BlendEquation eq;
        Pica::FramebufferRegs::BlendFactor src_factor;
        Pica::FramebufferRegs::BlendFactor dst_factor;
    } rgb_blend, alpha_blend;
};

/**
 * This struct contains all state used to generate the GLSL fragment shader that emulates the
 * current Pica register configuration. This struct is used as a cache key for generated GLSL shader
 * programs. The functions in glsl_shader_gen.cpp should retrieve state from this struct only, not
 * by directly accessing Pica registers. This should reduce the risk of bugs in shader generation
 * where Pica state is not being captured in the shader cache key, thereby resulting in (what should
 * be) two separate shaders sharing the same key.
 */
struct PicaFSConfig : Common::HashableStruct<PicaFSConfigState> {
    PicaFSConfig(const Pica::Regs& regs, bool has_fragment_shader_interlock, bool emulate_logic_op,
                 bool emulate_custom_border_color, bool emulate_blend_minmax_factor,
                 bool use_custom_normal_map = false);

    [[nodiscard]] bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
        return (stage_index < 4) && (state.combiner_buffer_input & (1 << stage_index));
    }

    [[nodiscard]] bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
        return (stage_index < 4) && ((state.combiner_buffer_input >> 4) & (1 << stage_index));
    }
};

enum class AttribLoadFlags {
    Float = 1 << 0,
    Sint = 1 << 1,
    Uint = 1 << 2,
    ZeroW = 1 << 3,
};
DECLARE_ENUM_FLAG_OPERATORS(AttribLoadFlags)

/**
 * This struct contains common information to identify a GLSL geometry shader generated from
 * PICA geometry shader.
 */
struct PicaGSConfigState {
    void Init(const Pica::Regs& regs, bool use_clip_planes_);

    bool use_clip_planes;

    u32 vs_output_attributes;
    u32 gs_output_attributes;

    struct SemanticMap {
        u32 attribute_index;
        u32 component_index;
    };

    // semantic_maps[semantic name] -> GS output attribute index + component index
    std::array<SemanticMap, 24> semantic_maps;
};

/**
 * This struct contains common information to identify a GLSL vertex shader generated from
 * PICA vertex shader.
 */
struct PicaVSConfigState {
    void Init(const Pica::Regs& regs, Pica::Shader::ShaderSetup& setup, bool use_clip_planes_,
              bool use_geometry_shader_);

    bool use_clip_planes;
    bool use_geometry_shader;

    u64 program_hash;
    u64 swizzle_hash;
    u32 main_offset;
    bool sanitize_mul;

    u32 num_outputs;
    // Load operations to apply to the input vertex data
    std::array<AttribLoadFlags, 16> load_flags;

    // output_map[output register index] -> output attribute index
    std::array<u32, 16> output_map;

    PicaGSConfigState gs_state;
};

/**
 * This struct contains information to identify a GL vertex shader generated from PICA vertex
 * shader.
 */
struct PicaVSConfig : Common::HashableStruct<PicaVSConfigState> {
    explicit PicaVSConfig(const Pica::Regs& regs, Pica::Shader::ShaderSetup& setup,
                          bool use_clip_planes_, bool use_geometry_shader_);
};

/**
 * This struct contains information to identify a GL geometry shader generated from PICA no-geometry
 * shader pipeline
 */
struct PicaFixedGSConfig : Common::HashableStruct<PicaGSConfigState> {
    explicit PicaFixedGSConfig(const Pica::Regs& regs, bool use_clip_planes_);
};

} // namespace Pica::Shader::Generator

namespace std {
template <>
struct hash<Pica::Shader::Generator::PicaFSConfig> {
    std::size_t operator()(const Pica::Shader::Generator::PicaFSConfig& k) const noexcept {
        return k.Hash();
    }
};

template <>
struct hash<Pica::Shader::Generator::PicaVSConfig> {
    std::size_t operator()(const Pica::Shader::Generator::PicaVSConfig& k) const noexcept {
        return k.Hash();
    }
};

template <>
struct hash<Pica::Shader::Generator::PicaFixedGSConfig> {
    std::size_t operator()(const Pica::Shader::Generator::PicaFixedGSConfig& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
