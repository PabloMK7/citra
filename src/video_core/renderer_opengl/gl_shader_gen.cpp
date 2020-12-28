// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <string_view>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/bit_set.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_lighting.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_texturing.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/video_core.h"

using Pica::FramebufferRegs;
using Pica::LightingRegs;
using Pica::RasterizerRegs;
using Pica::TexturingRegs;
using TevStageConfig = TexturingRegs::TevStageConfig;
using VSOutputAttributes = RasterizerRegs::VSOutputAttributes;

namespace OpenGL {

constexpr std::string_view UniformBlockDef = R"(
#define NUM_TEV_STAGES 6
#define NUM_LIGHTS 8
#define NUM_LIGHTING_SAMPLERS 24

struct LightSrc {
    vec3 specular_0;
    vec3 specular_1;
    vec3 diffuse;
    vec3 ambient;
    vec3 position;
    vec3 spot_direction;
    float dist_atten_bias;
    float dist_atten_scale;
};

layout (std140) uniform shader_data {
    int framebuffer_scale;
    int alphatest_ref;
    float depth_scale;
    float depth_offset;
    float shadow_bias_constant;
    float shadow_bias_linear;
    int scissor_x1;
    int scissor_y1;
    int scissor_x2;
    int scissor_y2;
    int fog_lut_offset;
    int proctex_noise_lut_offset;
    int proctex_color_map_offset;
    int proctex_alpha_map_offset;
    int proctex_lut_offset;
    int proctex_diff_lut_offset;
    float proctex_bias;
    int shadow_texture_bias;
    ivec4 lighting_lut_offset[NUM_LIGHTING_SAMPLERS / 4];
    vec3 fog_color;
    vec2 proctex_noise_f;
    vec2 proctex_noise_a;
    vec2 proctex_noise_p;
    vec3 lighting_global_ambient;
    LightSrc light_src[NUM_LIGHTS];
    vec4 const_color[NUM_TEV_STAGES];
    vec4 tev_combiner_buffer_color;
    vec4 clip_coef;
};
)";

static std::string GetVertexInterfaceDeclaration(bool is_output, bool separable_shader) {
    std::string out;

    const auto append_variable = [&](std::string_view var, int location) {
        if (separable_shader) {
            out += fmt::format("layout (location={}) ", location);
        }
        out += fmt::format("{}{};\n", is_output ? "out " : "in ", var);
    };

    append_variable("vec4 primary_color", ATTRIBUTE_COLOR);
    append_variable("vec2 texcoord0", ATTRIBUTE_TEXCOORD0);
    append_variable("vec2 texcoord1", ATTRIBUTE_TEXCOORD1);
    append_variable("vec2 texcoord2", ATTRIBUTE_TEXCOORD2);
    append_variable("float texcoord0_w", ATTRIBUTE_TEXCOORD0_W);
    append_variable("vec4 normquat", ATTRIBUTE_NORMQUAT);
    append_variable("vec3 view", ATTRIBUTE_VIEW);

    if (is_output && separable_shader) {
        // gl_PerVertex redeclaration is required for separate shader object
        out += R"(
out gl_PerVertex {
    vec4 gl_Position;
    float gl_ClipDistance[2];
};
)";
    }

    return out;
}

PicaFSConfig PicaFSConfig::BuildFromRegs(const Pica::Regs& regs) {
    PicaFSConfig res;

    auto& state = res.state;

    state.scissor_test_mode = regs.rasterizer.scissor_test.mode;

    state.depthmap_enable = regs.rasterizer.depthmap_enable;

    state.alpha_test_func = regs.framebuffer.output_merger.alpha_test.enable
                                ? regs.framebuffer.output_merger.alpha_test.func.Value()
                                : FramebufferRegs::CompareFunc::Always;

    state.texture0_type = regs.texturing.texture0.type;

    state.texture2_use_coord1 = regs.texturing.main_config.texture2_use_coord1 != 0;

    // Copy relevant tev stages fields.
    // We don't sync const_color here because of the high variance, it is a
    // shader uniform instead.
    const auto& tev_stages = regs.texturing.GetTevStages();
    DEBUG_ASSERT(state.tev_stages.size() == tev_stages.size());
    for (std::size_t i = 0; i < tev_stages.size(); i++) {
        const auto& tev_stage = tev_stages[i];
        state.tev_stages[i].sources_raw = tev_stage.sources_raw;
        state.tev_stages[i].modifiers_raw = tev_stage.modifiers_raw;
        state.tev_stages[i].ops_raw = tev_stage.ops_raw;
        state.tev_stages[i].scales_raw = tev_stage.scales_raw;
    }

    state.fog_mode = regs.texturing.fog_mode;
    state.fog_flip = regs.texturing.fog_flip != 0;

    state.combiner_buffer_input = regs.texturing.tev_combiner_buffer_input.update_mask_rgb.Value() |
                                  regs.texturing.tev_combiner_buffer_input.update_mask_a.Value()
                                      << 4;

    // Fragment lighting

    state.lighting.enable = !regs.lighting.disable;
    state.lighting.src_num = regs.lighting.max_light_index + 1;

    for (unsigned light_index = 0; light_index < state.lighting.src_num; ++light_index) {
        unsigned num = regs.lighting.light_enable.GetNum(light_index);
        const auto& light = regs.lighting.light[num];
        state.lighting.light[light_index].num = num;
        state.lighting.light[light_index].directional = light.config.directional != 0;
        state.lighting.light[light_index].two_sided_diffuse = light.config.two_sided_diffuse != 0;
        state.lighting.light[light_index].geometric_factor_0 = light.config.geometric_factor_0 != 0;
        state.lighting.light[light_index].geometric_factor_1 = light.config.geometric_factor_1 != 0;
        state.lighting.light[light_index].dist_atten_enable =
            !regs.lighting.IsDistAttenDisabled(num);
        state.lighting.light[light_index].spot_atten_enable =
            !regs.lighting.IsSpotAttenDisabled(num);
        state.lighting.light[light_index].shadow_enable = !regs.lighting.IsShadowDisabled(num);
    }

    state.lighting.lut_d0.enable = regs.lighting.config1.disable_lut_d0 == 0;
    state.lighting.lut_d0.abs_input = regs.lighting.abs_lut_input.disable_d0 == 0;
    state.lighting.lut_d0.type = regs.lighting.lut_input.d0.Value();
    state.lighting.lut_d0.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d0);

    state.lighting.lut_d1.enable = regs.lighting.config1.disable_lut_d1 == 0;
    state.lighting.lut_d1.abs_input = regs.lighting.abs_lut_input.disable_d1 == 0;
    state.lighting.lut_d1.type = regs.lighting.lut_input.d1.Value();
    state.lighting.lut_d1.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d1);

    // this is a dummy field due to lack of the corresponding register
    state.lighting.lut_sp.enable = true;
    state.lighting.lut_sp.abs_input = regs.lighting.abs_lut_input.disable_sp == 0;
    state.lighting.lut_sp.type = regs.lighting.lut_input.sp.Value();
    state.lighting.lut_sp.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.sp);

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
    state.lighting.enable_primary_alpha = regs.lighting.config0.enable_primary_alpha;
    state.lighting.enable_secondary_alpha = regs.lighting.config0.enable_secondary_alpha;
    state.lighting.bump_mode = regs.lighting.config0.bump_mode;
    state.lighting.bump_selector = regs.lighting.config0.bump_selector;
    state.lighting.bump_renorm = regs.lighting.config0.disable_bump_renorm == 0;
    state.lighting.clamp_highlights = regs.lighting.config0.clamp_highlights != 0;

    state.lighting.enable_shadow = regs.lighting.config0.enable_shadow != 0;
    state.lighting.shadow_primary = regs.lighting.config0.shadow_primary != 0;
    state.lighting.shadow_secondary = regs.lighting.config0.shadow_secondary != 0;
    state.lighting.shadow_invert = regs.lighting.config0.shadow_invert != 0;
    state.lighting.shadow_alpha = regs.lighting.config0.shadow_alpha != 0;
    state.lighting.shadow_selector = regs.lighting.config0.shadow_selector;

    state.proctex.enable = regs.texturing.main_config.texture3_enable;
    if (state.proctex.enable) {
        state.proctex.coord = regs.texturing.main_config.texture3_coordinates;
        state.proctex.u_clamp = regs.texturing.proctex.u_clamp;
        state.proctex.v_clamp = regs.texturing.proctex.v_clamp;
        state.proctex.color_combiner = regs.texturing.proctex.color_combiner;
        state.proctex.alpha_combiner = regs.texturing.proctex.alpha_combiner;
        state.proctex.separate_alpha = regs.texturing.proctex.separate_alpha;
        state.proctex.noise_enable = regs.texturing.proctex.noise_enable;
        state.proctex.u_shift = regs.texturing.proctex.u_shift;
        state.proctex.v_shift = regs.texturing.proctex.v_shift;
        state.proctex.lut_width = regs.texturing.proctex_lut.width;
        state.proctex.lut_offset0 = regs.texturing.proctex_lut_offset.level0;
        state.proctex.lut_offset1 = regs.texturing.proctex_lut_offset.level1;
        state.proctex.lut_offset2 = regs.texturing.proctex_lut_offset.level2;
        state.proctex.lut_offset3 = regs.texturing.proctex_lut_offset.level3;
        state.proctex.lod_min = regs.texturing.proctex_lut.lod_min;
        state.proctex.lod_max = regs.texturing.proctex_lut.lod_max;
        state.proctex.lut_filter = regs.texturing.proctex_lut.filter;
    }

    state.shadow_rendering = regs.framebuffer.output_merger.fragment_operation_mode ==
                             FramebufferRegs::FragmentOperationMode::Shadow;

    state.shadow_texture_orthographic = regs.texturing.shadow.orthographic != 0;

    return res;
}

void PicaShaderConfigCommon::Init(const Pica::ShaderRegs& regs, Pica::Shader::ShaderSetup& setup) {
    program_hash = setup.GetProgramCodeHash();
    swizzle_hash = setup.GetSwizzleDataHash();
    main_offset = regs.main_offset;
    sanitize_mul = VideoCore::g_hw_shader_accurate_mul;

    num_outputs = 0;
    output_map.fill(16);

    for (int reg : Common::BitSet<u32>(regs.output_mask)) {
        output_map[reg] = num_outputs++;
    }
}

void PicaGSConfigCommonRaw::Init(const Pica::Regs& regs) {
    vs_output_attributes = Common::BitSet<u32>(regs.vs.output_mask).Count();
    gs_output_attributes = vs_output_attributes;

    semantic_maps.fill({16, 0});
    for (u32 attrib = 0; attrib < regs.rasterizer.vs_output_total; ++attrib) {
        const std::array semantics{
            regs.rasterizer.vs_output_attributes[attrib].map_x.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_y.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_z.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_w.Value(),
        };
        for (u32 comp = 0; comp < 4; ++comp) {
            const auto semantic = semantics[comp];
            if (static_cast<std::size_t>(semantic) < 24) {
                semantic_maps[static_cast<std::size_t>(semantic)] = {attrib, comp};
            } else if (semantic != VSOutputAttributes::INVALID) {
                LOG_ERROR(Render_OpenGL, "Invalid/unknown semantic id: {}",
                          static_cast<u32>(semantic));
            }
        }
    }
}

/// Detects if a TEV stage is configured to be skipped (to avoid generating unnecessary code)
static bool IsPassThroughTevStage(const TevStageConfig& stage) {
    return (stage.color_op == TevStageConfig::Operation::Replace &&
            stage.alpha_op == TevStageConfig::Operation::Replace &&
            stage.color_source1 == TevStageConfig::Source::Previous &&
            stage.alpha_source1 == TevStageConfig::Source::Previous &&
            stage.color_modifier1 == TevStageConfig::ColorModifier::SourceColor &&
            stage.alpha_modifier1 == TevStageConfig::AlphaModifier::SourceAlpha &&
            stage.GetColorMultiplier() == 1 && stage.GetAlphaMultiplier() == 1);
}

static std::string SampleTexture(const PicaFSConfig& config, unsigned texture_unit) {
    const auto& state = config.state;
    switch (texture_unit) {
    case 0:
        // Only unit 0 respects the texturing type
        switch (state.texture0_type) {
        case TexturingRegs::TextureConfig::Texture2D:
            return "textureLod(tex0, texcoord0, getLod(texcoord0 * vec2(textureSize(tex0, 0))))";
        case TexturingRegs::TextureConfig::Projection2D:
            // TODO (wwylele): find the exact LOD formula for projection texture
            return "textureProj(tex0, vec3(texcoord0, texcoord0_w))";
        case TexturingRegs::TextureConfig::TextureCube:
            return "texture(tex_cube, vec3(texcoord0, texcoord0_w))";
        case TexturingRegs::TextureConfig::Shadow2D:
            return "shadowTexture(texcoord0, texcoord0_w)";
        case TexturingRegs::TextureConfig::ShadowCube:
            return "shadowTextureCube(texcoord0, texcoord0_w)";
        case TexturingRegs::TextureConfig::Disabled:
            return "vec4(0.0)";
        default:
            LOG_CRITICAL(HW_GPU, "Unhandled texture type {:x}",
                         static_cast<int>(state.texture0_type));
            UNIMPLEMENTED();
            return "texture(tex0, texcoord0)";
        }
    case 1:
        return "textureLod(tex1, texcoord1, getLod(texcoord1 * vec2(textureSize(tex1, 0))))";
    case 2:
        if (state.texture2_use_coord1)
            return "textureLod(tex2, texcoord1, getLod(texcoord1 * vec2(textureSize(tex2, 0))))";
        else
            return "textureLod(tex2, texcoord2, getLod(texcoord2 * vec2(textureSize(tex2, 0))))";
    case 3:
        if (state.proctex.enable) {
            return "ProcTex()";
        } else {
            LOG_DEBUG(Render_OpenGL, "Using Texture3 without enabling it");
            return "vec4(0.0)";
        }
    default:
        UNREACHABLE();
        return "";
    }
}

/// Writes the specified TEV stage source component(s)
static void AppendSource(std::string& out, const PicaFSConfig& config,
                         TevStageConfig::Source source, std::string_view index_name) {
    using Source = TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        out += "rounded_primary_color";
        break;
    case Source::PrimaryFragmentColor:
        out += "primary_fragment_color";
        break;
    case Source::SecondaryFragmentColor:
        out += "secondary_fragment_color";
        break;
    case Source::Texture0:
        out += SampleTexture(config, 0);
        break;
    case Source::Texture1:
        out += SampleTexture(config, 1);
        break;
    case Source::Texture2:
        out += SampleTexture(config, 2);
        break;
    case Source::Texture3:
        out += SampleTexture(config, 3);
        break;
    case Source::PreviousBuffer:
        out += "combiner_buffer";
        break;
    case Source::Constant:
        out += "const_color[";
        out += index_name;
        out += ']';
        break;
    case Source::Previous:
        out += "last_tex_env_out";
        break;
    default:
        out += "vec4(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown source op {}", static_cast<u32>(source));
        break;
    }
}

/// Writes the color components to use for the specified TEV stage color modifier
static void AppendColorModifier(std::string& out, const PicaFSConfig& config,
                                TevStageConfig::ColorModifier modifier,
                                TevStageConfig::Source source, std::string_view index_name) {
    using ColorModifier = TevStageConfig::ColorModifier;
    switch (modifier) {
    case ColorModifier::SourceColor:
        AppendSource(out, config, source, index_name);
        out += ".rgb";
        break;
    case ColorModifier::OneMinusSourceColor:
        out += "vec3(1.0) - ";
        AppendSource(out, config, source, index_name);
        out += ".rgb";
        break;
    case ColorModifier::SourceAlpha:
        AppendSource(out, config, source, index_name);
        out += ".aaa";
        break;
    case ColorModifier::OneMinusSourceAlpha:
        out += "vec3(1.0) - ";
        AppendSource(out, config, source, index_name);
        out += ".aaa";
        break;
    case ColorModifier::SourceRed:
        AppendSource(out, config, source, index_name);
        out += ".rrr";
        break;
    case ColorModifier::OneMinusSourceRed:
        out += "vec3(1.0) - ";
        AppendSource(out, config, source, index_name);
        out += ".rrr";
        break;
    case ColorModifier::SourceGreen:
        AppendSource(out, config, source, index_name);
        out += ".ggg";
        break;
    case ColorModifier::OneMinusSourceGreen:
        out += "vec3(1.0) - ";
        AppendSource(out, config, source, index_name);
        out += ".ggg";
        break;
    case ColorModifier::SourceBlue:
        AppendSource(out, config, source, index_name);
        out += ".bbb";
        break;
    case ColorModifier::OneMinusSourceBlue:
        out += "vec3(1.0) - ";
        AppendSource(out, config, source, index_name);
        out += ".bbb";
        break;
    default:
        out += "vec3(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown color modifier op {}", static_cast<u32>(modifier));
        break;
    }
}

/// Writes the alpha component to use for the specified TEV stage alpha modifier
static void AppendAlphaModifier(std::string& out, const PicaFSConfig& config,
                                TevStageConfig::AlphaModifier modifier,
                                TevStageConfig::Source source, const std::string& index_name) {
    using AlphaModifier = TevStageConfig::AlphaModifier;
    switch (modifier) {
    case AlphaModifier::SourceAlpha:
        AppendSource(out, config, source, index_name);
        out += ".a";
        break;
    case AlphaModifier::OneMinusSourceAlpha:
        out += "1.0 - ";
        AppendSource(out, config, source, index_name);
        out += ".a";
        break;
    case AlphaModifier::SourceRed:
        AppendSource(out, config, source, index_name);
        out += ".r";
        break;
    case AlphaModifier::OneMinusSourceRed:
        out += "1.0 - ";
        AppendSource(out, config, source, index_name);
        out += ".r";
        break;
    case AlphaModifier::SourceGreen:
        AppendSource(out, config, source, index_name);
        out += ".g";
        break;
    case AlphaModifier::OneMinusSourceGreen:
        out += "1.0 - ";
        AppendSource(out, config, source, index_name);
        out += ".g";
        break;
    case AlphaModifier::SourceBlue:
        AppendSource(out, config, source, index_name);
        out += ".b";
        break;
    case AlphaModifier::OneMinusSourceBlue:
        out += "1.0 - ";
        AppendSource(out, config, source, index_name);
        out += ".b";
        break;
    default:
        out += "0.0";
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha modifier op {}", static_cast<u32>(modifier));
        break;
    }
}

/// Writes the combiner function for the color components for the specified TEV stage operation
static void AppendColorCombiner(std::string& out, TevStageConfig::Operation operation,
                                std::string_view variable_name) {
    out += "clamp(";
    using Operation = TevStageConfig::Operation;
    switch (operation) {
    case Operation::Replace:
        out += fmt::format("{}[0]", variable_name);
        break;
    case Operation::Modulate:
        out += fmt::format("{0}[0] * {0}[1]", variable_name);
        break;
    case Operation::Add:
        out += fmt::format("{0}[0] + {0}[1]", variable_name);
        break;
    case Operation::AddSigned:
        out += fmt::format("{0}[0] + {0}[1] - vec3(0.5)", variable_name);
        break;
    case Operation::Lerp:
        out += fmt::format("{0}[0] * {0}[2] + {0}[1] * (vec3(1.0) - {0}[2])", variable_name);
        break;
    case Operation::Subtract:
        out += fmt::format("{0}[0] - {0}[1]", variable_name);
        break;
    case Operation::MultiplyThenAdd:
        out += fmt::format("{0}[0] * {0}[1] + {0}[2]", variable_name);
        break;
    case Operation::AddThenMultiply:
        out += fmt::format("min({0}[0] + {0}[1], vec3(1.0)) * {0}[2]", variable_name);
        break;
    case Operation::Dot3_RGB:
    case Operation::Dot3_RGBA:
        out +=
            fmt::format("vec3(dot({0}[0] - vec3(0.5), {0}[1] - vec3(0.5)) * 4.0)", variable_name);
        break;
    default:
        out += "vec3(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown color combiner operation: {}",
                     static_cast<u32>(operation));
        break;
    }
    out += ", vec3(0.0), vec3(1.0))"; // Clamp result to 0.0, 1.0
}

/// Writes the combiner function for the alpha component for the specified TEV stage operation
static void AppendAlphaCombiner(std::string& out, TevStageConfig::Operation operation,
                                std::string_view variable_name) {
    out += "clamp(";
    using Operation = TevStageConfig::Operation;
    switch (operation) {
    case Operation::Replace:
        out += fmt::format("{}[0]", variable_name);
        break;
    case Operation::Modulate:
        out += fmt::format("{0}[0] * {0}[1]", variable_name);
        break;
    case Operation::Add:
        out += fmt::format("{0}[0] + {0}[1]", variable_name);
        break;
    case Operation::AddSigned:
        out += fmt::format("{0}[0] + {0}[1] - 0.5", variable_name);
        break;
    case Operation::Lerp:
        out += fmt::format("{0}[0] * {0}[2] + {0}[1] * (1.0 - {0}[2])", variable_name);
        break;
    case Operation::Subtract:
        out += fmt::format("{0}[0] - {0}[1]", variable_name);
        break;
    case Operation::MultiplyThenAdd:
        out += fmt::format("{0}[0] * {0}[1] + {0}[2]", variable_name);
        break;
    case Operation::AddThenMultiply:
        out += fmt::format("min({0}[0] + {0}[1], 1.0) * {0}[2]", variable_name);
        break;
    default:
        out += "0.0";
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha combiner operation: {}",
                     static_cast<u32>(operation));
        break;
    }
    out += ", 0.0, 1.0)";
}

/// Writes the if-statement condition used to evaluate alpha testing
static void AppendAlphaTestCondition(std::string& out, FramebufferRegs::CompareFunc func) {
    using CompareFunc = FramebufferRegs::CompareFunc;
    switch (func) {
    case CompareFunc::Never:
        out += "true";
        break;
    case CompareFunc::Always:
        out += "false";
        break;
    case CompareFunc::Equal:
    case CompareFunc::NotEqual:
    case CompareFunc::LessThan:
    case CompareFunc::LessThanOrEqual:
    case CompareFunc::GreaterThan:
    case CompareFunc::GreaterThanOrEqual: {
        static constexpr std::array op{"!=", "==", ">=", ">", "<=", "<"};
        const auto index = static_cast<u32>(func) - static_cast<u32>(CompareFunc::Equal);
        out += fmt::format("int(last_tex_env_out.a * 255.0) {} alphatest_ref", op[index]);
        break;
    }

    default:
        out += "false";
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha test condition {}", static_cast<u32>(func));
        break;
    }
}

/// Writes the code to emulate the specified TEV stage
static void WriteTevStage(std::string& out, const PicaFSConfig& config, unsigned index) {
    const auto stage =
        static_cast<const TexturingRegs::TevStageConfig>(config.state.tev_stages[index]);
    if (!IsPassThroughTevStage(stage)) {
        const std::string index_name = std::to_string(index);

        out += fmt::format("vec3 color_results_{}[3] = vec3[3](", index_name);
        AppendColorModifier(out, config, stage.color_modifier1, stage.color_source1, index_name);
        out += ", ";
        AppendColorModifier(out, config, stage.color_modifier2, stage.color_source2, index_name);
        out += ", ";
        AppendColorModifier(out, config, stage.color_modifier3, stage.color_source3, index_name);
        out += ");\n";

        // Round the output of each TEV stage to maintain the PICA's 8 bits of precision
        out += fmt::format("vec3 color_output_{} = byteround(", index_name);
        AppendColorCombiner(out, stage.color_op, "color_results_" + index_name);
        out += ");\n";

        if (stage.color_op == TevStageConfig::Operation::Dot3_RGBA) {
            // result of Dot3_RGBA operation is also placed to the alpha component
            out += fmt::format("float alpha_output_{0} = color_output_{0}[0];\n", index_name);
        } else {
            out += fmt::format("float alpha_results_{}[3] = float[3](", index_name);
            AppendAlphaModifier(out, config, stage.alpha_modifier1, stage.alpha_source1,
                                index_name);
            out += ", ";
            AppendAlphaModifier(out, config, stage.alpha_modifier2, stage.alpha_source2,
                                index_name);
            out += ", ";
            AppendAlphaModifier(out, config, stage.alpha_modifier3, stage.alpha_source3,
                                index_name);
            out += ");\n";

            out += fmt::format("float alpha_output_{} = byteround(", index_name);
            AppendAlphaCombiner(out, stage.alpha_op, "alpha_results_" + index_name);
            out += ");\n";
        }

        out += fmt::format("last_tex_env_out = vec4("
                           "clamp(color_output_{} * {}.0, vec3(0.0), vec3(1.0)), "
                           "clamp(alpha_output_{} * {}.0, 0.0, 1.0));\n",
                           index_name, stage.GetColorMultiplier(), index_name,
                           stage.GetAlphaMultiplier());
    }

    out += "combiner_buffer = next_combiner_buffer;\n";

    if (config.TevStageUpdatesCombinerBufferColor(index))
        out += "next_combiner_buffer.rgb = last_tex_env_out.rgb;\n";

    if (config.TevStageUpdatesCombinerBufferAlpha(index))
        out += "next_combiner_buffer.a = last_tex_env_out.a;\n";
}

/// Writes the code to emulate fragment lighting
static void WriteLighting(std::string& out, const PicaFSConfig& config) {
    const auto& lighting = config.state.lighting;

    // Define lighting globals
    out += "vec4 diffuse_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec4 specular_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec3 light_vector = vec3(0.0);\n"
           "vec3 refl_value = vec3(0.0);\n"
           "vec3 spot_dir = vec3(0.0);\n"
           "vec3 half_vector = vec3(0.0);\n"
           "float dot_product = 0.0;\n"
           "float clamp_highlights = 1.0;\n"
           "float geo_factor = 1.0;\n";

    // Compute fragment normals and tangents
    const auto Perturbation = [&] {
        return fmt::format("2.0 * ({}).rgb - 1.0", SampleTexture(config, lighting.bump_selector));
    };
    if (lighting.bump_mode == LightingRegs::LightingBumpMode::NormalMap) {
        // Bump mapping is enabled using a normal map
        out += fmt::format("vec3 surface_normal = {};\n", Perturbation());

        // Recompute Z-component of perturbation if 'renorm' is enabled, this provides a higher
        // precision result
        if (lighting.bump_renorm) {
            constexpr std::string_view val =
                "(1.0 - (surface_normal.x*surface_normal.x + surface_normal.y*surface_normal.y))";
            out += fmt::format("surface_normal.z = sqrt(max({}, 0.0));\n", val);
        }

        // The tangent vector is not perturbed by the normal map and is just a unit vector.
        out += "vec3 surface_tangent = vec3(1.0, 0.0, 0.0);\n";
    } else if (lighting.bump_mode == LightingRegs::LightingBumpMode::TangentMap) {
        // Bump mapping is enabled using a tangent map
        out += fmt::format("vec3 surface_tangent = {};\n", Perturbation());
        // Mathematically, recomputing Z-component of the tangent vector won't affect the relevant
        // computation below, which is also confirmed on 3DS. So we don't bother recomputing here
        // even if 'renorm' is enabled.

        // The normal vector is not perturbed by the tangent map and is just a unit vector.
        out += "vec3 surface_normal = vec3(0.0, 0.0, 1.0);\n";
    } else {
        // No bump mapping - surface local normal and tangent are just unit vectors
        out += "vec3 surface_normal = vec3(0.0, 0.0, 1.0);\n"
               "vec3 surface_tangent = vec3(1.0, 0.0, 0.0);\n";
    }

    // Rotate the surface-local normal by the interpolated normal quaternion to convert it to
    // eyespace.
    out += "vec4 normalized_normquat = normalize(normquat);\n"
           "vec3 normal = quaternion_rotate(normalized_normquat, surface_normal);\n"
           "vec3 tangent = quaternion_rotate(normalized_normquat, surface_tangent);\n";

    if (lighting.enable_shadow) {
        std::string shadow_texture = SampleTexture(config, lighting.shadow_selector);
        if (lighting.shadow_invert) {
            out += fmt::format("vec4 shadow = vec4(1.0) - {};\n", shadow_texture);
        } else {
            out += fmt::format("vec4 shadow = {};\n", shadow_texture);
        }
    } else {
        out += "vec4 shadow = vec4(1.0);\n";
    }

    // Samples the specified lookup table for specular lighting
    auto GetLutValue = [&lighting](LightingRegs::LightingSampler sampler, unsigned light_num,
                                   LightingRegs::LightingLutInput input, bool abs) {
        std::string index;
        switch (input) {
        case LightingRegs::LightingLutInput::NH:
            index = "dot(normal, normalize(half_vector))";
            break;

        case LightingRegs::LightingLutInput::VH:
            index = "dot(normalize(view), normalize(half_vector))";
            break;

        case LightingRegs::LightingLutInput::NV:
            index = "dot(normal, normalize(view))";
            break;

        case LightingRegs::LightingLutInput::LN:
            index = "dot(light_vector, normal)";
            break;

        case LightingRegs::LightingLutInput::SP:
            index = "dot(light_vector, spot_dir)";
            break;

        case LightingRegs::LightingLutInput::CP:
            // CP input is only available with configuration 7
            if (lighting.config == LightingRegs::LightingConfig::Config7) {
                // Note: even if the normal vector is modified by normal map, which is not the
                // normal of the tangent plane anymore, the half angle vector is still projected
                // using the modified normal vector.
                constexpr std::string_view half_angle_proj =
                    "normalize(half_vector) - normal * dot(normal, normalize(half_vector))";
                // Note: the half angle vector projection is confirmed not normalized before the dot
                // product. The result is in fact not cos(phi) as the name suggested.
                index = fmt::format("dot({}, tangent)", half_angle_proj);
            } else {
                index = "0.0";
            }
            break;

        default:
            LOG_CRITICAL(HW_GPU, "Unknown lighting LUT input {}", (int)input);
            UNIMPLEMENTED();
            index = "0.0";
            break;
        }

        const auto sampler_index = static_cast<u32>(sampler);

        if (abs) {
            // LUT index is in the range of (0.0, 1.0)
            index = lighting.light[light_num].two_sided_diffuse
                        ? fmt::format("abs({})", index)
                        : fmt::format("max({}, 0.0)", index);
            return fmt::format("LookupLightingLUTUnsigned({}, {})", sampler_index, index);
        } else {
            // LUT index is in the range of (-1.0, 1.0)
            return fmt::format("LookupLightingLUTSigned({}, {})", sampler_index, index);
        }
    };

    // Write the code to emulate each enabled light
    for (unsigned light_index = 0; light_index < lighting.src_num; ++light_index) {
        const auto& light_config = lighting.light[light_index];
        const std::string light_src = fmt::format("light_src[{}]", light_config.num);

        // Compute light vector (directional or positional)
        if (light_config.directional) {
            out += fmt::format("light_vector = normalize({}.position);\n", light_src);
        } else {
            out += fmt::format("light_vector = normalize({}.position + view);\n", light_src);
        }

        out += fmt::format("spot_dir = {}.spot_direction;\n", light_src);
        out += "half_vector = normalize(view) + light_vector;\n";

        // Compute dot product of light_vector and normal, adjust if lighting is one-sided or
        // two-sided
        out += std::string("dot_product = ") + (light_config.two_sided_diffuse
                                                    ? "abs(dot(light_vector, normal));\n"
                                                    : "max(dot(light_vector, normal), 0.0);\n");

        // If enabled, clamp specular component if lighting result is zero
        if (lighting.clamp_highlights) {
            out += "clamp_highlights = sign(dot_product);\n";
        }

        // If enabled, compute spot light attenuation value
        std::string spot_atten = "1.0";
        if (light_config.spot_atten_enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::SpotlightAttenuation)) {
            const std::string value =
                GetLutValue(LightingRegs::SpotlightAttenuationSampler(light_config.num),
                            light_config.num, lighting.lut_sp.type, lighting.lut_sp.abs_input);
            spot_atten = fmt::format("({:#} * {})", lighting.lut_sp.scale, value);
        }

        // If enabled, compute distance attenuation value
        std::string dist_atten = "1.0";
        if (light_config.dist_atten_enable) {
            const std::string index = fmt::format("clamp({}.dist_atten_scale * length(-view - "
                                                  "{}.position) + {}.dist_atten_bias, 0.0, 1.0)",
                                                  light_src, light_src, light_src);
            const auto sampler = LightingRegs::DistanceAttenuationSampler(light_config.num);
            dist_atten =
                fmt::format("LookupLightingLUTUnsigned({}, {})", static_cast<u32>(sampler), index);
        }

        if (light_config.geometric_factor_0 || light_config.geometric_factor_1) {
            out += "geo_factor = dot(half_vector, half_vector);\n"
                   "geo_factor = geo_factor == 0.0 ? 0.0 : min("
                   "dot_product / geo_factor, 1.0);\n";
        }

        // Specular 0 component
        std::string d0_lut_value = "1.0";
        if (lighting.lut_d0.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution0)) {
            // Lookup specular "distribution 0" LUT value
            const std::string value =
                GetLutValue(LightingRegs::LightingSampler::Distribution0, light_config.num,
                            lighting.lut_d0.type, lighting.lut_d0.abs_input);
            d0_lut_value = fmt::format("({:#} * {})", lighting.lut_d0.scale, value);
        }
        std::string specular_0 = fmt::format("({} * {}.specular_0)", d0_lut_value, light_src);
        if (light_config.geometric_factor_0) {
            specular_0 = fmt::format("({} * geo_factor)", specular_0);
        }

        // If enabled, lookup ReflectRed value, otherwise, 1.0 is used
        if (lighting.lut_rr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectRed)) {
            std::string value =
                GetLutValue(LightingRegs::LightingSampler::ReflectRed, light_config.num,
                            lighting.lut_rr.type, lighting.lut_rr.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rr.scale, value);
            out += fmt::format("refl_value.r = {};\n", value);
        } else {
            out += "refl_value.r = 1.0;\n";
        }

        // If enabled, lookup ReflectGreen value, otherwise, ReflectRed value is used
        if (lighting.lut_rg.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectGreen)) {
            std::string value =
                GetLutValue(LightingRegs::LightingSampler::ReflectGreen, light_config.num,
                            lighting.lut_rg.type, lighting.lut_rg.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rg.scale, value);
            out += fmt::format("refl_value.g = {};\n", value);
        } else {
            out += "refl_value.g = refl_value.r;\n";
        }

        // If enabled, lookup ReflectBlue value, otherwise, ReflectRed value is used
        if (lighting.lut_rb.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectBlue)) {
            std::string value =
                GetLutValue(LightingRegs::LightingSampler::ReflectBlue, light_config.num,
                            lighting.lut_rb.type, lighting.lut_rb.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rb.scale, value);
            out += fmt::format("refl_value.b = {};\n", value);
        } else {
            out += "refl_value.b = refl_value.r;\n";
        }

        // Specular 1 component
        std::string d1_lut_value = "1.0";
        if (lighting.lut_d1.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution1)) {
            // Lookup specular "distribution 1" LUT value
            const std::string value =
                GetLutValue(LightingRegs::LightingSampler::Distribution1, light_config.num,
                            lighting.lut_d1.type, lighting.lut_d1.abs_input);
            d1_lut_value = fmt::format("({:#} * {})", lighting.lut_d1.scale, value);
        }
        std::string specular_1 =
            fmt::format("({} * refl_value * {}.specular_1)", d1_lut_value, light_src);
        if (light_config.geometric_factor_1) {
            specular_1 = fmt::format("({} * geo_factor)", specular_1);
        }

        // Fresnel
        // Note: only the last entry in the light slots applies the Fresnel factor
        if (light_index == lighting.src_num - 1 && lighting.lut_fr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::Fresnel)) {
            // Lookup fresnel LUT value
            std::string value =
                GetLutValue(LightingRegs::LightingSampler::Fresnel, light_config.num,
                            lighting.lut_fr.type, lighting.lut_fr.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_fr.scale, value);

            // Enabled for diffuse lighting alpha component
            if (lighting.enable_primary_alpha) {
                out += fmt::format("diffuse_sum.a = {};\n", value);
            }

            // Enabled for the specular lighting alpha component
            if (lighting.enable_secondary_alpha) {
                out += fmt::format("specular_sum.a = {};\n", value);
            }
        }

        bool shadow_primary_enable = lighting.shadow_primary && light_config.shadow_enable;
        bool shadow_secondary_enable = lighting.shadow_secondary && light_config.shadow_enable;
        std::string shadow_primary = shadow_primary_enable ? " * shadow.rgb" : "";
        std::string shadow_secondary = shadow_secondary_enable ? " * shadow.rgb" : "";

        // Compute primary fragment color (diffuse lighting) function
        out += fmt::format(
            "diffuse_sum.rgb += (({}.diffuse * dot_product) + {}.ambient) * {} * {}{};\n",
            light_src, light_src, dist_atten, spot_atten, shadow_primary);

        // Compute secondary fragment color (specular lighting) function
        out += fmt::format("specular_sum.rgb += ({} + {}) * clamp_highlights * {} * {}{};\n",
                           specular_0, specular_1, dist_atten, spot_atten, shadow_secondary);
    }

    // Apply shadow attenuation to alpha components if enabled
    if (lighting.shadow_alpha) {
        if (lighting.enable_primary_alpha) {
            out += "diffuse_sum.a *= shadow.a;\n";
        }
        if (lighting.enable_secondary_alpha) {
            out += "specular_sum.a *= shadow.a;\n";
        }
    }

    // Sum final lighting result
    out += "diffuse_sum.rgb += lighting_global_ambient;\n"
           "primary_fragment_color = clamp(diffuse_sum, vec4(0.0), vec4(1.0));\n"
           "secondary_fragment_color = clamp(specular_sum, vec4(0.0), vec4(1.0));\n";
}

using ProcTexClamp = TexturingRegs::ProcTexClamp;
using ProcTexShift = TexturingRegs::ProcTexShift;
using ProcTexCombiner = TexturingRegs::ProcTexCombiner;
using ProcTexFilter = TexturingRegs::ProcTexFilter;

static void AppendProcTexShiftOffset(std::string& out, std::string_view v, ProcTexShift mode,
                                     ProcTexClamp clamp_mode) {
    const std::string_view offset = (clamp_mode == ProcTexClamp::MirroredRepeat) ? "1.0" : "0.5";
    switch (mode) {
    case ProcTexShift::None:
        out += "0.0";
        break;
    case ProcTexShift::Odd:
        out += fmt::format("{} * float((int({}) / 2) % 2)", offset, v);
        break;
    case ProcTexShift::Even:
        out += fmt::format("{} * float(((int({}) + 1) / 2) % 2)", offset, v);
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown shift mode {}", static_cast<u32>(mode));
        out += "0.0";
        break;
    }
}

static void AppendProcTexClamp(std::string& out, std::string_view var, ProcTexClamp mode) {
    switch (mode) {
    case ProcTexClamp::ToZero:
        out += fmt::format("{0} = {0} > 1.0 ? 0 : {0};\n", var);
        break;
    case ProcTexClamp::ToEdge:
        out += fmt::format("{0} = min({0}, 1.0);\n", var);
        break;
    case ProcTexClamp::SymmetricalRepeat:
        out += fmt::format("{0} = fract({0});\n", var);
        break;
    case ProcTexClamp::MirroredRepeat: {
        out += fmt::format("{0} = int({0}) % 2 == 0 ? fract({0}) : 1.0 - fract({0});\n", var);
        break;
    }
    case ProcTexClamp::Pulse:
        out += fmt::format("{0} = {0} > 0.5 ? 1.0 : 0.0;\n", var);
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown clamp mode {}", static_cast<u32>(mode));
        out += fmt::format("{0} = min({0}, 1.0);\n", var);
        break;
    }
}

static void AppendProcTexCombineAndMap(std::string& out, ProcTexCombiner combiner,
                                       std::string_view offset) {
    const auto combined = [combiner]() -> std::string_view {
        switch (combiner) {
        case ProcTexCombiner::U:
            return "u";
        case ProcTexCombiner::U2:
            return "(u * u)";
        case TexturingRegs::ProcTexCombiner::V:
            return "v";
        case TexturingRegs::ProcTexCombiner::V2:
            return "(v * v)";
        case TexturingRegs::ProcTexCombiner::Add:
            return "((u + v) * 0.5)";
        case TexturingRegs::ProcTexCombiner::Add2:
            return "((u * u + v * v) * 0.5)";
        case TexturingRegs::ProcTexCombiner::SqrtAdd2:
            return "min(sqrt(u * u + v * v), 1.0)";
        case TexturingRegs::ProcTexCombiner::Min:
            return "min(u, v)";
        case TexturingRegs::ProcTexCombiner::Max:
            return "max(u, v)";
        case TexturingRegs::ProcTexCombiner::RMax:
            return "min(((u + v) * 0.5 + sqrt(u * u + v * v)) * 0.5, 1.0)";
        default:
            LOG_CRITICAL(HW_GPU, "Unknown combiner {}", static_cast<u32>(combiner));
            return "0.0";
        }
    }();

    out += fmt::format("ProcTexLookupLUT({}, {})", offset, combined);
}

static void AppendProcTexSampler(std::string& out, const PicaFSConfig& config) {
    // LUT sampling uitlity
    // For NoiseLUT/ColorMap/AlphaMap, coord=0.0 is lut[0], coord=127.0/128.0 is lut[127] and
    // coord=1.0 is lut[127]+lut_diff[127]. For other indices, the result is interpolated using
    // value entries and difference entries.
    out += R"(
float ProcTexLookupLUT(int offset, float coord) {
    coord *= 128.0;
    float index_i = clamp(floor(coord), 0.0, 127.0);
    float index_f = coord - index_i; // fract() cannot be used here because 128.0 needs to be
                                     // extracted as index_i = 127.0 and index_f = 1.0
    vec2 entry = texelFetch(texture_buffer_lut_rg, int(index_i) + offset).rg;
    return clamp(entry.r + entry.g * index_f, 0.0, 1.0);
}
    )";

    // Noise utility
    if (config.state.proctex.noise_enable) {
        // See swrasterizer/proctex.cpp for more information about these functions
        out += R"(
int ProcTexNoiseRand1D(int v) {
    const int table[] = int[](0,4,10,8,4,9,7,12,5,15,13,14,11,15,2,11);
    return ((v % 9 + 2) * 3 & 0xF) ^ table[(v / 9) & 0xF];
}

float ProcTexNoiseRand2D(vec2 point) {
    const int table[] = int[](10,2,15,8,0,7,4,5,5,13,2,6,13,9,3,14);
    int u2 = ProcTexNoiseRand1D(int(point.x));
    int v2 = ProcTexNoiseRand1D(int(point.y));
    v2 += ((u2 & 3) == 1) ? 4 : 0;
    v2 ^= (u2 & 1) * 6;
    v2 += 10 + u2;
    v2 &= 0xF;
    v2 ^= table[u2];
    return -1.0 + float(v2) * 2.0/ 15.0;
}

float ProcTexNoiseCoef(vec2 x) {
    vec2 grid  = 9.0 * proctex_noise_f * abs(x + proctex_noise_p);
    vec2 point = floor(grid);
    vec2 frac  = grid - point;

    float g0 = ProcTexNoiseRand2D(point) * (frac.x + frac.y);
    float g1 = ProcTexNoiseRand2D(point + vec2(1.0, 0.0)) * (frac.x + frac.y - 1.0);
    float g2 = ProcTexNoiseRand2D(point + vec2(0.0, 1.0)) * (frac.x + frac.y - 1.0);
    float g3 = ProcTexNoiseRand2D(point + vec2(1.0, 1.0)) * (frac.x + frac.y - 2.0);

    float x_noise = ProcTexLookupLUT(proctex_noise_lut_offset, frac.x);
    float y_noise = ProcTexLookupLUT(proctex_noise_lut_offset, frac.y);
    float x0 = mix(g0, g1, x_noise);
    float x1 = mix(g2, g3, x_noise);
    return mix(x0, x1, y_noise);
}
        )";
    }

    out += "vec4 SampleProcTexColor(float lut_coord, int level) {\n";
    out += fmt::format("int lut_width = {} >> level;\n", config.state.proctex.lut_width);
    // Offsets for level 4-7 seem to be hardcoded
    out += fmt::format("int lut_offsets[8] = int[]({}, {}, {}, {}, 0xF0, 0xF8, 0xFC, 0xFE);\n",
                       config.state.proctex.lut_offset0, config.state.proctex.lut_offset1,
                       config.state.proctex.lut_offset2, config.state.proctex.lut_offset3);
    out += "int lut_offset = lut_offsets[level];\n";
    // For the color lut, coord=0.0 is lut[offset] and coord=1.0 is lut[offset+width-1]
    out += "lut_coord *= float(lut_width - 1);\n";

    switch (config.state.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::LinearMipmapLinear:
    case ProcTexFilter::LinearMipmapNearest:
        out += "int lut_index_i = int(lut_coord) + lut_offset;\n";
        out += "float lut_index_f = fract(lut_coord);\n";
        out += "return texelFetch(texture_buffer_lut_rgba, lut_index_i + "
               "proctex_lut_offset) + "
               "lut_index_f * "
               "texelFetch(texture_buffer_lut_rgba, lut_index_i + proctex_diff_lut_offset);\n";
        break;
    case ProcTexFilter::Nearest:
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::NearestMipmapNearest:
        out += "lut_coord += float(lut_offset);\n";
        out += "return texelFetch(texture_buffer_lut_rgba, int(round(lut_coord)) + "
               "proctex_lut_offset);\n";
        break;
    }

    out += "}\n";

    out += "vec4 ProcTex() {\n";
    if (config.state.proctex.coord < 3) {
        out += fmt::format("vec2 uv = abs(texcoord{});\n", config.state.proctex.coord);
    } else {
        LOG_CRITICAL(Render_OpenGL, "Unexpected proctex.coord >= 3");
        out += "vec2 uv = abs(texcoord0);\n";
    }

    // This LOD formula is the same as the LOD upper limit defined in OpenGL.
    // f(x, y) <= m_u + m_v + m_w
    // (See OpenGL 4.6 spec, 8.14.1 - Scale Factor and Level-of-Detail)
    // Note: this is different from the one normal 2D textures use.
    out += "vec2 duv = max(abs(dFdx(uv)), abs(dFdy(uv)));\n";
    // unlike normal texture, the bias is inside the log2
    out += fmt::format("float lod = log2(abs(float({}) * proctex_bias) * (duv.x + duv.y));\n",
                       config.state.proctex.lut_width);
    out += "if (proctex_bias == 0.0) lod = 0.0;\n";
    out += fmt::format("lod = clamp(lod, {:#}, {:#});\n",
                       std::max(0.0f, static_cast<float>(config.state.proctex.lod_min)),
                       std::min(7.0f, static_cast<float>(config.state.proctex.lod_max)));
    // Get shift offset before noise generation
    out += "float u_shift = ";
    AppendProcTexShiftOffset(out, "uv.y", config.state.proctex.u_shift,
                             config.state.proctex.u_clamp);
    out += ";\n";
    out += "float v_shift = ";
    AppendProcTexShiftOffset(out, "uv.x", config.state.proctex.v_shift,
                             config.state.proctex.v_clamp);
    out += ";\n";

    // Generate noise
    if (config.state.proctex.noise_enable) {
        out += "uv += proctex_noise_a * ProcTexNoiseCoef(uv);\n"
               "uv = abs(uv);\n";
    }

    // Shift
    out += "float u = uv.x + u_shift;\n"
           "float v = uv.y + v_shift;\n";

    // Clamp
    AppendProcTexClamp(out, "u", config.state.proctex.u_clamp);
    AppendProcTexClamp(out, "v", config.state.proctex.v_clamp);

    // Combine and map
    out += "float lut_coord = ";
    AppendProcTexCombineAndMap(out, config.state.proctex.color_combiner,
                               "proctex_color_map_offset");
    out += ";\n";

    switch (config.state.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::Nearest:
        out += "vec4 final_color = SampleProcTexColor(lut_coord, 0);\n";
        break;
    case ProcTexFilter::NearestMipmapNearest:
    case ProcTexFilter::LinearMipmapNearest:
        out += "vec4 final_color = SampleProcTexColor(lut_coord, int(round(lod)));\n";
        break;
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::LinearMipmapLinear:
        out += "int lod_i = int(lod);\n"
               "float lod_f = fract(lod);\n"
               "vec4 final_color = mix(SampleProcTexColor(lut_coord, lod_i), "
               "SampleProcTexColor(lut_coord, lod_i + 1), lod_f);\n";
        break;
    }

    if (config.state.proctex.separate_alpha) {
        // Note: in separate alpha mode, the alpha channel skips the color LUT look up stage. It
        // uses the output of CombineAndMap directly instead.
        out += "float final_alpha = ";
        AppendProcTexCombineAndMap(out, config.state.proctex.alpha_combiner,
                                   "proctex_alpha_map_offset");
        out += ";\n";
        out += "return vec4(final_color.xyz, final_alpha);\n}\n";
    } else {
        out += "return final_color;\n}\n";
    }
}

ShaderDecompiler::ProgramResult GenerateFragmentShader(const PicaFSConfig& config,
                                                       bool separable_shader) {
    const auto& state = config.state;

    std::string out = R"(
#extension GL_ARB_shader_image_load_store : enable
#extension GL_ARB_shader_image_size : enable
#define ALLOW_SHADOW (defined(GL_ARB_shader_image_load_store) && defined(GL_ARB_shader_image_size))
)";

    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    if (GLES) {
        out += fragment_shader_precision_OES;
    }

    out += GetVertexInterfaceDeclaration(false, separable_shader);

    out += R"(
#ifndef CITRA_GLES
in vec4 gl_FragCoord;
#endif // CITRA_GLES

out vec4 color;

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform samplerCube tex_cube;
uniform samplerBuffer texture_buffer_lut_rg;
uniform samplerBuffer texture_buffer_lut_rgba;

#if ALLOW_SHADOW
layout(r32ui) uniform readonly uimage2D shadow_texture_px;
layout(r32ui) uniform readonly uimage2D shadow_texture_nx;
layout(r32ui) uniform readonly uimage2D shadow_texture_py;
layout(r32ui) uniform readonly uimage2D shadow_texture_ny;
layout(r32ui) uniform readonly uimage2D shadow_texture_pz;
layout(r32ui) uniform readonly uimage2D shadow_texture_nz;
layout(r32ui) uniform uimage2D shadow_buffer;
#endif
)";

    out += UniformBlockDef;

    out += R"(
// Rotate the vector v by the quaternion q
vec3 quaternion_rotate(vec4 q, vec3 v) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

float LookupLightingLUT(int lut_index, int index, float delta) {
    vec2 entry = texelFetch(texture_buffer_lut_rg, lighting_lut_offset[lut_index >> 2][lut_index & 3] + index).rg;
    return entry.r + entry.g * delta;
}

float LookupLightingLUTUnsigned(int lut_index, float pos) {
    int index = clamp(int(pos * 256.0), 0, 255);
    float delta = pos * 256.0 - float(index);
    return LookupLightingLUT(lut_index, index, delta);
}

float LookupLightingLUTSigned(int lut_index, float pos) {
    int index = clamp(int(pos * 128.0), -128, 127);
    float delta = pos * 128.0 - float(index);
    if (index < 0) index += 256;
    return LookupLightingLUT(lut_index, index, delta);
}

float byteround(float x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec2 byteround(vec2 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec3 byteround(vec3 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec4 byteround(vec4 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

// PICA's LOD formula for 2D textures.
// This LOD formula is the same as the LOD lower limit defined in OpenGL.
// f(x, y) >= max{m_u, m_v, m_w}
// (See OpenGL 4.6 spec, 8.14.1 - Scale Factor and Level-of-Detail)
float getLod(vec2 coord) {
    vec2 d = max(abs(dFdx(coord)), abs(dFdy(coord)));
    return log2(max(d.x, d.y));
}

#if ALLOW_SHADOW

uvec2 DecodeShadow(uint pixel) {
    return uvec2(pixel >> 8, pixel & 0xFFu);
}

uint EncodeShadow(uvec2 pixel) {
    return (pixel.x << 8) | pixel.y;
}

float CompareShadow(uint pixel, uint z) {
    uvec2 p = DecodeShadow(pixel);
    return mix(float(p.y) * (1.0 / 255.0), 0.0, p.x <= z);
}

float SampleShadow2D(ivec2 uv, uint z) {
    if (any(bvec4( lessThan(uv, ivec2(0)), greaterThanEqual(uv, imageSize(shadow_texture_px)) )))
        return 1.0;
    return CompareShadow(imageLoad(shadow_texture_px, uv).x, z);
}

float mix2(vec4 s, vec2 a) {
    vec2 t = mix(s.xy, s.zw, a.yy);
    return mix(t.x, t.y, a.x);
}

vec4 shadowTexture(vec2 uv, float w) {
)";
    if (!config.state.shadow_texture_orthographic) {
        out += "uv /= w;";
    }
    out += "uint z = uint(max(0, int(min(abs(w), 1.0) * float(0xFFFFFF)) - shadow_texture_bias));";
    out += R"(
    vec2 coord = vec2(imageSize(shadow_texture_px)) * uv - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i = ivec2(coord_floor);
    vec4 s = vec4(
        SampleShadow2D(i              , z),
        SampleShadow2D(i + ivec2(1, 0), z),
        SampleShadow2D(i + ivec2(0, 1), z),
        SampleShadow2D(i + ivec2(1, 1), z));
    return vec4(mix2(s, f));
}

vec4 shadowTextureCube(vec2 uv, float w) {
    ivec2 size = imageSize(shadow_texture_px);
    vec3 c = vec3(uv, w);
    vec3 a = abs(c);
    if (a.x > a.y && a.x > a.z) {
        w = a.x;
        uv = -c.zy;
        if (c.x < 0.0) uv.x = -uv.x;
    } else if (a.y > a.z) {
        w = a.y;
        uv = c.xz;
        if (c.y < 0.0) uv.y = -uv.y;
    } else {
        w = a.z;
        uv = -c.xy;
        if (c.z > 0.0) uv.x = -uv.x;
    }
)";
    out += "uint z = uint(max(0, int(min(w, 1.0) * float(0xFFFFFF)) - shadow_texture_bias));";
    out += R"(
    vec2 coord = vec2(size) * (uv / w * vec2(0.5) + vec2(0.5)) - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i00 = ivec2(coord_floor);
    ivec2 i10 = i00 + ivec2(1, 0);
    ivec2 i01 = i00 + ivec2(0, 1);
    ivec2 i11 = i00 + ivec2(1, 1);
    ivec2 cmin = ivec2(0), cmax = size - ivec2(1, 1);
    i00 = clamp(i00, cmin, cmax);
    i10 = clamp(i10, cmin, cmax);
    i01 = clamp(i01, cmin, cmax);
    i11 = clamp(i11, cmin, cmax);
    uvec4 pixels;
    // This part should have been refactored into functions,
    // but many drivers don't like passing uimage2D as parameters
    if (a.x > a.y && a.x > a.z) {
        if (c.x > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_px, i00).r,
                imageLoad(shadow_texture_px, i10).r,
                imageLoad(shadow_texture_px, i01).r,
                imageLoad(shadow_texture_px, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_nx, i00).r,
                imageLoad(shadow_texture_nx, i10).r,
                imageLoad(shadow_texture_nx, i01).r,
                imageLoad(shadow_texture_nx, i11).r);
    } else if (a.y > a.z) {
        if (c.y > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_py, i00).r,
                imageLoad(shadow_texture_py, i10).r,
                imageLoad(shadow_texture_py, i01).r,
                imageLoad(shadow_texture_py, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_ny, i00).r,
                imageLoad(shadow_texture_ny, i10).r,
                imageLoad(shadow_texture_ny, i01).r,
                imageLoad(shadow_texture_ny, i11).r);
    } else {
        if (c.z > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_pz, i00).r,
                imageLoad(shadow_texture_pz, i10).r,
                imageLoad(shadow_texture_pz, i01).r,
                imageLoad(shadow_texture_pz, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_nz, i00).r,
                imageLoad(shadow_texture_nz, i10).r,
                imageLoad(shadow_texture_nz, i01).r,
                imageLoad(shadow_texture_nz, i11).r);
    }
    vec4 s = vec4(
        CompareShadow(pixels.x, z),
        CompareShadow(pixels.y, z),
        CompareShadow(pixels.z, z),
        CompareShadow(pixels.w, z));
    return vec4(mix2(s, f));
}

#else

vec4 shadowTexture(vec2 uv, float w) {
    return vec4(1.0);
}

vec4 shadowTextureCube(vec2 uv, float w) {
    return vec4(1.0);
}

#endif
)";

    if (config.state.proctex.enable)
        AppendProcTexSampler(out, config);

    // We round the interpolated primary color to the nearest 1/255th
    // This maintains the PICA's 8 bits of precision
    out += R"(
void main() {
vec4 rounded_primary_color = byteround(primary_color);
vec4 primary_fragment_color = vec4(0.0);
vec4 secondary_fragment_color = vec4(0.0);
)";

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (state.alpha_test_func == FramebufferRegs::CompareFunc::Never) {
        out += "discard; }";
        return {std::move(out)};
    }

    // Append the scissor test
    if (state.scissor_test_mode != RasterizerRegs::ScissorMode::Disabled) {
        out += "if (";
        // Negate the condition if we have to keep only the pixels outside the scissor box
        if (state.scissor_test_mode == RasterizerRegs::ScissorMode::Include) {
            out += '!';
        }
        out += "(gl_FragCoord.x >= float(scissor_x1) && "
               "gl_FragCoord.y >= float(scissor_y1) && "
               "gl_FragCoord.x < float(scissor_x2) && "
               "gl_FragCoord.y < float(scissor_y2))) discard;\n";
    }

    // After perspective divide, OpenGL transform z_over_w from [-1, 1] to [near, far]. Here we use
    // default near = 0 and far = 1, and undo the transformation to get the original z_over_w, then
    // do our own transformation according to PICA specification.
    out += "float z_over_w = 2.0 * gl_FragCoord.z - 1.0;\n"
           "float depth = z_over_w * depth_scale + depth_offset;\n";
    if (state.depthmap_enable == RasterizerRegs::DepthBuffering::WBuffering) {
        out += "depth /= gl_FragCoord.w;\n";
    }

    if (state.lighting.enable)
        WriteLighting(out, config);

    out += "vec4 combiner_buffer = vec4(0.0);\n"
           "vec4 next_combiner_buffer = tev_combiner_buffer_color;\n"
           "vec4 last_tex_env_out = vec4(0.0);\n";

    for (std::size_t index = 0; index < state.tev_stages.size(); ++index) {
        WriteTevStage(out, config, static_cast<u32>(index));
    }

    if (state.alpha_test_func != FramebufferRegs::CompareFunc::Always) {
        out += "if (";
        AppendAlphaTestCondition(out, state.alpha_test_func);
        out += ") discard;\n";
    }

    // Append fog combiner
    if (state.fog_mode == TexturingRegs::FogMode::Fog) {
        // Get index into fog LUT
        if (state.fog_flip) {
            out += "float fog_index = (1.0 - float(depth)) * 128.0;\n";
        } else {
            out += "float fog_index = depth * 128.0;\n";
        }

        // Generate clamped fog factor from LUT for given fog index
        out += "float fog_i = clamp(floor(fog_index), 0.0, 127.0);\n"
               "float fog_f = fog_index - fog_i;\n"
               "vec2 fog_lut_entry = texelFetch(texture_buffer_lut_rg, int(fog_i) + "
               "fog_lut_offset).rg;\n"
               "float fog_factor = fog_lut_entry.r + fog_lut_entry.g * fog_f;\n"
               "fog_factor = clamp(fog_factor, 0.0, 1.0);\n";

        // Blend the fog
        out += "last_tex_env_out.rgb = mix(fog_color.rgb, last_tex_env_out.rgb, fog_factor);\n";
    } else if (state.fog_mode == TexturingRegs::FogMode::Gas) {
        Core::System::GetInstance().TelemetrySession().AddField(Telemetry::FieldType::Session,
                                                                "VideoCore_Pica_UseGasMode", true);
        LOG_CRITICAL(Render_OpenGL, "Unimplemented gas mode");
        out += "discard; }";
        return {std::move(out)};
    }

    if (state.shadow_rendering) {
        out += R"(
#if ALLOW_SHADOW
uint d = uint(clamp(depth, 0.0, 1.0) * 0xFFFFFF);
uint s = uint(last_tex_env_out.g * 0xFF);
ivec2 image_coord = ivec2(gl_FragCoord.xy);

uint old = imageLoad(shadow_buffer, image_coord).x;
uint new;
uint old2;
do {
    old2 = old;

    uvec2 ref = DecodeShadow(old);
    if (d < ref.x) {
        if (s == 0u) {
            ref.x = d;
        } else {
            s = uint(float(s) / (shadow_bias_constant + shadow_bias_linear * float(d) / float(ref.x)));
            ref.y = min(s, ref.y);
        }
    }
    new = EncodeShadow(ref);

} while ((old = imageAtomicCompSwap(shadow_buffer, image_coord, old, new)) != old2);
#endif // ALLOW_SHADOW
)";
    } else {
        out += "gl_FragDepth = depth;\n";
        // Round the final fragment color to maintain the PICA's 8 bits of precision
        out += "color = byteround(last_tex_env_out);\n";
    }

    out += '}';

    return {std::move(out)};
}

ShaderDecompiler::ProgramResult GenerateTrivialVertexShader(bool separable_shader) {
    std::string out;
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    out +=
        fmt::format("layout(location = {}) in vec4 vert_position;\n"
                    "layout(location = {}) in vec4 vert_color;\n"
                    "layout(location = {}) in vec2 vert_texcoord0;\n"
                    "layout(location = {}) in vec2 vert_texcoord1;\n"
                    "layout(location = {}) in vec2 vert_texcoord2;\n"
                    "layout(location = {}) in float vert_texcoord0_w;\n"
                    "layout(location = {}) in vec4 vert_normquat;\n"
                    "layout(location = {}) in vec3 vert_view;\n",
                    static_cast<int>(ATTRIBUTE_POSITION), static_cast<int>(ATTRIBUTE_COLOR),
                    static_cast<int>(ATTRIBUTE_TEXCOORD0), static_cast<int>(ATTRIBUTE_TEXCOORD1),
                    static_cast<int>(ATTRIBUTE_TEXCOORD2), static_cast<int>(ATTRIBUTE_TEXCOORD0_W),
                    static_cast<int>(ATTRIBUTE_NORMQUAT), static_cast<int>(ATTRIBUTE_VIEW));

    out += GetVertexInterfaceDeclaration(true, separable_shader);

    out += UniformBlockDef;

    out += R"(

void main() {
    primary_color = vert_color;
    texcoord0 = vert_texcoord0;
    texcoord1 = vert_texcoord1;
    texcoord2 = vert_texcoord2;
    texcoord0_w = vert_texcoord0_w;
    normquat = vert_normquat;
    view = vert_view;
    gl_Position = vert_position;
#if !defined(CITRA_GLES) || defined(GL_EXT_clip_cull_distance)
    gl_ClipDistance[0] = -vert_position.z; // fixed PICA clipping plane z <= 0
    gl_ClipDistance[1] = dot(clip_coef, vert_position);
#endif // !defined(CITRA_GLES) || defined(GL_EXT_clip_cull_distance)
}
)";

    return {std::move(out)};
}

std::optional<ShaderDecompiler::ProgramResult> GenerateVertexShader(
    const Pica::Shader::ShaderSetup& setup, const PicaVSConfig& config, bool separable_shader) {
    std::string out = "";
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    out += ShaderDecompiler::GetCommonDeclarations();

    std::array<bool, 16> used_regs{};
    const auto get_input_reg = [&used_regs](u32 reg) {
        ASSERT(reg < 16);
        used_regs[reg] = true;
        return fmt::format("vs_in_reg{}", reg);
    };

    const auto get_output_reg = [&](u32 reg) -> std::string {
        ASSERT(reg < 16);
        if (config.state.output_map[reg] < config.state.num_outputs) {
            return fmt::format("vs_out_attr{}", config.state.output_map[reg]);
        }
        return "";
    };

    auto program_source_opt = ShaderDecompiler::DecompileProgram(
        setup.program_code, setup.swizzle_data, config.state.main_offset, get_input_reg,
        get_output_reg, config.state.sanitize_mul);

    if (!program_source_opt)
        return std::nullopt;

    std::string& program_source = program_source_opt->code;

    out += R"(
#define uniforms vs_uniforms
layout (std140) uniform vs_config {
    pica_uniforms uniforms;
};

)";
    // input attributes declaration
    for (std::size_t i = 0; i < used_regs.size(); ++i) {
        if (used_regs[i]) {
            out += fmt::format("layout(location = {0}) in vec4 vs_in_reg{0};\n", i);
        }
    }
    out += '\n';

    // output attributes declaration
    for (u32 i = 0; i < config.state.num_outputs; ++i) {
        out += (separable_shader ? "layout(location = " + std::to_string(i) + ")" : std::string{}) +
               " out vec4 vs_out_attr" + std::to_string(i) + ";\n";
    }

    out += "\nvoid main() {\n";
    for (u32 i = 0; i < config.state.num_outputs; ++i) {
        out += fmt::format("    vs_out_attr{} = vec4(0.0, 0.0, 0.0, 1.0);\n", i);
    }
    out += "\n    exec_shader();\n}\n\n";

    out += program_source;

    return {{std::move(out)}};
}

static std::string GetGSCommonSource(const PicaGSConfigCommonRaw& config, bool separable_shader) {
    std::string out = GetVertexInterfaceDeclaration(true, separable_shader);
    out += UniformBlockDef;
    out += ShaderDecompiler::GetCommonDeclarations();

    out += '\n';
    for (u32 i = 0; i < config.vs_output_attributes; ++i) {
        out += (separable_shader ? "layout(location = " + std::to_string(i) + ")" : std::string{}) +
               " in vec4 vs_out_attr" + std::to_string(i) + "[];\n";
    }

    out += R"(
struct Vertex {
)";
    out += fmt::format("    vec4 attributes[{}];\n", config.gs_output_attributes);
    out += "};\n\n";

    const auto semantic = [&config](VSOutputAttributes::Semantic slot_semantic) -> std::string {
        const u32 slot = static_cast<u32>(slot_semantic);
        const u32 attrib = config.semantic_maps[slot].attribute_index;
        const u32 comp = config.semantic_maps[slot].component_index;
        if (attrib < config.gs_output_attributes) {
            return fmt::format("vtx.attributes[{}].{}", attrib, "xyzw"[comp]);
        }
        return "0.0";
    };

    out += "vec4 GetVertexQuaternion(Vertex vtx) {\n";
    out += "    return vec4(" + semantic(VSOutputAttributes::QUATERNION_X) + ", " +
           semantic(VSOutputAttributes::QUATERNION_Y) + ", " +
           semantic(VSOutputAttributes::QUATERNION_Z) + ", " +
           semantic(VSOutputAttributes::QUATERNION_W) + ");\n";
    out += "}\n\n";

    out += "void EmitVtx(Vertex vtx, bool quats_opposite) {\n";
    out += "    vec4 vtx_pos = vec4(" + semantic(VSOutputAttributes::POSITION_X) + ", " +
           semantic(VSOutputAttributes::POSITION_Y) + ", " +
           semantic(VSOutputAttributes::POSITION_Z) + ", " +
           semantic(VSOutputAttributes::POSITION_W) + ");\n";
    out += "    gl_Position = vtx_pos;\n";
    out += "#if !defined(CITRA_GLES) || defined(GL_EXT_clip_cull_distance)\n";
    out += "    gl_ClipDistance[0] = -vtx_pos.z;\n"; // fixed PICA clipping plane z <= 0
    out += "    gl_ClipDistance[1] = dot(clip_coef, vtx_pos);\n";
    out += "#endif // !defined(CITRA_GLES) || defined(GL_EXT_clip_cull_distance)\n\n";

    out += "    vec4 vtx_quat = GetVertexQuaternion(vtx);\n";
    out += "    normquat = mix(vtx_quat, -vtx_quat, bvec4(quats_opposite));\n\n";

    out += "    vec4 vtx_color = vec4(" + semantic(VSOutputAttributes::COLOR_R) + ", " +
           semantic(VSOutputAttributes::COLOR_G) + ", " + semantic(VSOutputAttributes::COLOR_B) +
           ", " + semantic(VSOutputAttributes::COLOR_A) + ");\n";
    out += "    primary_color = min(abs(vtx_color), vec4(1.0));\n\n";

    out += "    texcoord0 = vec2(" + semantic(VSOutputAttributes::TEXCOORD0_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD0_V) + ");\n";
    out += "    texcoord1 = vec2(" + semantic(VSOutputAttributes::TEXCOORD1_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD1_V) + ");\n\n";

    out += "    texcoord0_w = " + semantic(VSOutputAttributes::TEXCOORD0_W) + ";\n";
    out += "    view = vec3(" + semantic(VSOutputAttributes::VIEW_X) + ", " +
           semantic(VSOutputAttributes::VIEW_Y) + ", " + semantic(VSOutputAttributes::VIEW_Z) +
           ");\n\n";

    out += "    texcoord2 = vec2(" + semantic(VSOutputAttributes::TEXCOORD2_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD2_V) + ");\n\n";

    out += "    EmitVertex();\n";
    out += "}\n";

    out += R"(
bool AreQuaternionsOpposite(vec4 qa, vec4 qb) {
    return (dot(qa, qb) < 0.0);
}

void EmitPrim(Vertex vtx0, Vertex vtx1, Vertex vtx2) {
    EmitVtx(vtx0, false);
    EmitVtx(vtx1, AreQuaternionsOpposite(GetVertexQuaternion(vtx0), GetVertexQuaternion(vtx1)));
    EmitVtx(vtx2, AreQuaternionsOpposite(GetVertexQuaternion(vtx0), GetVertexQuaternion(vtx2)));
    EndPrimitive();
}
)";

    return out;
};

ShaderDecompiler::ProgramResult GenerateFixedGeometryShader(const PicaFixedGSConfig& config,
                                                            bool separable_shader) {
    std::string out = "";
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n\n";
    }

    out += R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

)";

    out += GetGSCommonSource(config.state, separable_shader);

    out += R"(
void main() {
    Vertex prim_buffer[3];
)";
    for (u32 vtx = 0; vtx < 3; ++vtx) {
        out += fmt::format("    prim_buffer[{}].attributes = vec4[{}](", vtx,
                           config.state.gs_output_attributes);
        for (u32 i = 0; i < config.state.vs_output_attributes; ++i) {
            out += fmt::format("{}vs_out_attr{}[{}]", i == 0 ? "" : ", ", i, vtx);
        }
        out += ");\n";
    }
    out += "    EmitPrim(prim_buffer[0], prim_buffer[1], prim_buffer[2]);\n";
    out += "}\n";

    return {std::move(out)};
}
} // namespace OpenGL
