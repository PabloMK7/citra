// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "video_core/pica.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

using Pica::Regs;
using TevStageConfig = Regs::TevStageConfig;

namespace GLShader {

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

/// Writes the specified TEV stage source component(s)
static void AppendSource(std::string& out, const PicaShaderConfig& config,
                         TevStageConfig::Source source, const std::string& index_name) {
    const auto& state = config.state;
    using Source = TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        out += "primary_color";
        break;
    case Source::PrimaryFragmentColor:
        out += "primary_fragment_color";
        break;
    case Source::SecondaryFragmentColor:
        out += "secondary_fragment_color";
        break;
    case Source::Texture0:
        // Only unit 0 respects the texturing type (according to 3DBrew)
        switch (state.texture0_type) {
        case Pica::Regs::TextureConfig::Texture2D:
            out += "texture(tex[0], texcoord[0])";
            break;
        case Pica::Regs::TextureConfig::Projection2D:
            out += "textureProj(tex[0], vec3(texcoord[0], texcoord0_w))";
            break;
        default:
            out += "texture(tex[0], texcoord[0])";
            LOG_CRITICAL(HW_GPU, "Unhandled texture type %x",
                         static_cast<int>(state.texture0_type));
            UNIMPLEMENTED();
            break;
        }
        break;
    case Source::Texture1:
        out += "texture(tex[1], texcoord[1])";
        break;
    case Source::Texture2:
        out += "texture(tex[2], texcoord[2])";
        break;
    case Source::PreviousBuffer:
        out += "combiner_buffer";
        break;
    case Source::Constant:
        ((out += "const_color[") += index_name) += ']';
        break;
    case Source::Previous:
        out += "last_tex_env_out";
        break;
    default:
        out += "vec4(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown source op %u", source);
        break;
    }
}

/// Writes the color components to use for the specified TEV stage color modifier
static void AppendColorModifier(std::string& out, const PicaShaderConfig& config,
                                TevStageConfig::ColorModifier modifier,
                                TevStageConfig::Source source, const std::string& index_name) {
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
        LOG_CRITICAL(Render_OpenGL, "Unknown color modifier op %u", modifier);
        break;
    }
}

/// Writes the alpha component to use for the specified TEV stage alpha modifier
static void AppendAlphaModifier(std::string& out, const PicaShaderConfig& config,
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
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha modifier op %u", modifier);
        break;
    }
}

/// Writes the combiner function for the color components for the specified TEV stage operation
static void AppendColorCombiner(std::string& out, TevStageConfig::Operation operation,
                                const std::string& variable_name) {
    out += "clamp(";
    using Operation = TevStageConfig::Operation;
    switch (operation) {
    case Operation::Replace:
        out += variable_name + "[0]";
        break;
    case Operation::Modulate:
        out += variable_name + "[0] * " + variable_name + "[1]";
        break;
    case Operation::Add:
        out += variable_name + "[0] + " + variable_name + "[1]";
        break;
    case Operation::AddSigned:
        out += variable_name + "[0] + " + variable_name + "[1] - vec3(0.5)";
        break;
    case Operation::Lerp:
        // TODO(bunnei): Verify if HW actually does this per-component, otherwise we can just use
        // builtin lerp
        out += variable_name + "[0] * " + variable_name + "[2] + " + variable_name +
               "[1] * (vec3(1.0) - " + variable_name + "[2])";
        break;
    case Operation::Subtract:
        out += variable_name + "[0] - " + variable_name + "[1]";
        break;
    case Operation::MultiplyThenAdd:
        out += variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2]";
        break;
    case Operation::AddThenMultiply:
        out += "min(" + variable_name + "[0] + " + variable_name + "[1], vec3(1.0)) * " +
               variable_name + "[2]";
        break;
    case Operation::Dot3_RGB:
        out += "vec3(dot(" + variable_name + "[0] - vec3(0.5), " + variable_name +
               "[1] - vec3(0.5)) * 4.0)";
        break;
    default:
        out += "vec3(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown color combiner operation: %u", operation);
        break;
    }
    out += ", vec3(0.0), vec3(1.0))"; // Clamp result to 0.0, 1.0
}

/// Writes the combiner function for the alpha component for the specified TEV stage operation
static void AppendAlphaCombiner(std::string& out, TevStageConfig::Operation operation,
                                const std::string& variable_name) {
    out += "clamp(";
    using Operation = TevStageConfig::Operation;
    switch (operation) {
    case Operation::Replace:
        out += variable_name + "[0]";
        break;
    case Operation::Modulate:
        out += variable_name + "[0] * " + variable_name + "[1]";
        break;
    case Operation::Add:
        out += variable_name + "[0] + " + variable_name + "[1]";
        break;
    case Operation::AddSigned:
        out += variable_name + "[0] + " + variable_name + "[1] - 0.5";
        break;
    case Operation::Lerp:
        out += variable_name + "[0] * " + variable_name + "[2] + " + variable_name +
               "[1] * (1.0 - " + variable_name + "[2])";
        break;
    case Operation::Subtract:
        out += variable_name + "[0] - " + variable_name + "[1]";
        break;
    case Operation::MultiplyThenAdd:
        out += variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2]";
        break;
    case Operation::AddThenMultiply:
        out += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0) * " + variable_name +
               "[2]";
        break;
    default:
        out += "0.0";
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha combiner operation: %u", operation);
        break;
    }
    out += ", 0.0, 1.0)";
}

/// Writes the if-statement condition used to evaluate alpha testing
static void AppendAlphaTestCondition(std::string& out, Regs::CompareFunc func) {
    using CompareFunc = Regs::CompareFunc;
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
        static const char* op[] = {"!=", "==", ">=", ">", "<=", "<"};
        unsigned index = (unsigned)func - (unsigned)CompareFunc::Equal;
        out += "int(last_tex_env_out.a * 255.0f) " + std::string(op[index]) + " alphatest_ref";
        break;
    }

    default:
        out += "false";
        LOG_CRITICAL(Render_OpenGL, "Unknown alpha test condition %u", func);
        break;
    }
}

/// Writes the code to emulate the specified TEV stage
static void WriteTevStage(std::string& out, const PicaShaderConfig& config, unsigned index) {
    const auto stage =
        static_cast<const Pica::Regs::TevStageConfig>(config.state.tev_stages[index]);
    if (!IsPassThroughTevStage(stage)) {
        std::string index_name = std::to_string(index);

        out += "vec3 color_results_" + index_name + "[3] = vec3[3](";
        AppendColorModifier(out, config, stage.color_modifier1, stage.color_source1, index_name);
        out += ", ";
        AppendColorModifier(out, config, stage.color_modifier2, stage.color_source2, index_name);
        out += ", ";
        AppendColorModifier(out, config, stage.color_modifier3, stage.color_source3, index_name);
        out += ");\n";

        out += "vec3 color_output_" + index_name + " = ";
        AppendColorCombiner(out, stage.color_op, "color_results_" + index_name);
        out += ";\n";

        out += "float alpha_results_" + index_name + "[3] = float[3](";
        AppendAlphaModifier(out, config, stage.alpha_modifier1, stage.alpha_source1, index_name);
        out += ", ";
        AppendAlphaModifier(out, config, stage.alpha_modifier2, stage.alpha_source2, index_name);
        out += ", ";
        AppendAlphaModifier(out, config, stage.alpha_modifier3, stage.alpha_source3, index_name);
        out += ");\n";

        out += "float alpha_output_" + index_name + " = ";
        AppendAlphaCombiner(out, stage.alpha_op, "alpha_results_" + index_name);
        out += ";\n";

        out += "last_tex_env_out = vec4("
               "clamp(color_output_" +
               index_name + " * " + std::to_string(stage.GetColorMultiplier()) +
               ".0, vec3(0.0), vec3(1.0)),"
               "clamp(alpha_output_" +
               index_name + " * " + std::to_string(stage.GetAlphaMultiplier()) +
               ".0, 0.0, 1.0));\n";
    }

    out += "combiner_buffer = next_combiner_buffer;\n";

    if (config.TevStageUpdatesCombinerBufferColor(index))
        out += "next_combiner_buffer.rgb = last_tex_env_out.rgb;\n";

    if (config.TevStageUpdatesCombinerBufferAlpha(index))
        out += "next_combiner_buffer.a = last_tex_env_out.a;\n";
}

/// Writes the code to emulate fragment lighting
static void WriteLighting(std::string& out, const PicaShaderConfig& config) {
    const auto& lighting = config.state.lighting;

    // Define lighting globals
    out += "vec4 diffuse_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec4 specular_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec3 light_vector = vec3(0.0);\n"
           "vec3 refl_value = vec3(0.0);\n";

    // Compute fragment normals
    if (lighting.bump_mode == Pica::Regs::LightingBumpMode::NormalMap) {
        // Bump mapping is enabled using a normal map, read perturbation vector from the selected
        // texture
        std::string bump_selector = std::to_string(lighting.bump_selector);
        out += "vec3 surface_normal = 2.0 * texture(tex[" + bump_selector + "], texcoord[" +
               bump_selector + "]).rgb - 1.0;\n";

        // Recompute Z-component of perturbation if 'renorm' is enabled, this provides a higher
        // precision result
        if (lighting.bump_renorm) {
            std::string val =
                "(1.0 - (surface_normal.x*surface_normal.x + surface_normal.y*surface_normal.y))";
            out += "surface_normal.z = sqrt(max(" + val + ", 0.0));\n";
        }
    } else if (lighting.bump_mode == Pica::Regs::LightingBumpMode::TangentMap) {
        // Bump mapping is enabled using a tangent map
        LOG_CRITICAL(HW_GPU, "unimplemented bump mapping mode (tangent mapping)");
        UNIMPLEMENTED();
    } else {
        // No bump mapping - surface local normal is just a unit normal
        out += "vec3 surface_normal = vec3(0.0, 0.0, 1.0);\n";
    }

    // Rotate the surface-local normal by the interpolated normal quaternion to convert it to
    // eyespace
    out += "vec3 normal = normalize(quaternion_rotate(normquat, surface_normal));\n";

    // Gets the index into the specified lookup table for specular lighting
    auto GetLutIndex = [&lighting](unsigned light_num, Regs::LightingLutInput input, bool abs) {
        const std::string half_angle = "normalize(normalize(view) + light_vector)";
        std::string index;
        switch (input) {
        case Regs::LightingLutInput::NH:
            index = "dot(normal, " + half_angle + ")";
            break;

        case Regs::LightingLutInput::VH:
            index = std::string("dot(normalize(view), " + half_angle + ")");
            break;

        case Regs::LightingLutInput::NV:
            index = std::string("dot(normal, normalize(view))");
            break;

        case Regs::LightingLutInput::LN:
            index = std::string("dot(light_vector, normal)");
            break;

        default:
            LOG_CRITICAL(HW_GPU, "Unknown lighting LUT input %d\n", (int)input);
            UNIMPLEMENTED();
            index = "0.0";
            break;
        }

        if (abs) {
            // LUT index is in the range of (0.0, 1.0)
            index = lighting.light[light_num].two_sided_diffuse ? "abs(" + index + ")"
                                                                : "max(" + index + ", 0.f)";
            return "(FLOAT_255 * clamp(" + index + ", 0.0, 1.0))";
        } else {
            // LUT index is in the range of (-1.0, 1.0)
            index = "clamp(" + index + ", -1.0, 1.0)";
            return "(FLOAT_255 * ((" + index + " < 0) ? " + index + " + 2.0 : " + index +
                   ") / 2.0)";
        }

        return std::string();
    };

    // Gets the lighting lookup table value given the specified sampler and index
    auto GetLutValue = [](Regs::LightingSampler sampler, std::string lut_index) {
        return std::string("texture(lut[" + std::to_string((unsigned)sampler / 4) + "], " +
                           lut_index + ")[" + std::to_string((unsigned)sampler & 3) + "]");
    };

    // Write the code to emulate each enabled light
    for (unsigned light_index = 0; light_index < lighting.src_num; ++light_index) {
        const auto& light_config = lighting.light[light_index];
        std::string light_src = "light_src[" + std::to_string(light_config.num) + "]";

        // Compute light vector (directional or positional)
        if (light_config.directional)
            out += "light_vector = normalize(" + light_src + ".position);\n";
        else
            out += "light_vector = normalize(" + light_src + ".position + view);\n";

        // Compute dot product of light_vector and normal, adjust if lighting is one-sided or
        // two-sided
        std::string dot_product = light_config.two_sided_diffuse
                                      ? "abs(dot(light_vector, normal))"
                                      : "max(dot(light_vector, normal), 0.0)";

        // If enabled, compute distance attenuation value
        std::string dist_atten = "1.0";
        if (light_config.dist_atten_enable) {
            std::string index = "(" + light_src + ".dist_atten_scale * length(-view - " +
                                light_src + ".position) + " + light_src + ".dist_atten_bias)";
            index = "((clamp(" + index + ", 0.0, FLOAT_255)))";
            const unsigned lut_num =
                ((unsigned)Regs::LightingSampler::DistanceAttenuation + light_config.num);
            dist_atten = GetLutValue((Regs::LightingSampler)lut_num, index);
        }

        // If enabled, clamp specular component if lighting result is negative
        std::string clamp_highlights =
            lighting.clamp_highlights ? "(dot(light_vector, normal) <= 0.0 ? 0.0 : 1.0)" : "1.0";

        // Specular 0 component
        std::string d0_lut_value = "1.0";
        if (lighting.lut_d0.enable &&
            Pica::Regs::IsLightingSamplerSupported(lighting.config,
                                                   Pica::Regs::LightingSampler::Distribution0)) {
            // Lookup specular "distribution 0" LUT value
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_d0.type, lighting.lut_d0.abs_input);
            d0_lut_value = "(" + std::to_string(lighting.lut_d0.scale) + " * " +
                           GetLutValue(Regs::LightingSampler::Distribution0, index) + ")";
        }
        std::string specular_0 = "(" + d0_lut_value + " * " + light_src + ".specular_0)";

        // If enabled, lookup ReflectRed value, otherwise, 1.0 is used
        if (lighting.lut_rr.enable &&
            Pica::Regs::IsLightingSamplerSupported(lighting.config,
                                                   Pica::Regs::LightingSampler::ReflectRed)) {
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_rr.type, lighting.lut_rr.abs_input);
            std::string value = "(" + std::to_string(lighting.lut_rr.scale) + " * " +
                                GetLutValue(Regs::LightingSampler::ReflectRed, index) + ")";
            out += "refl_value.r = " + value + ";\n";
        } else {
            out += "refl_value.r = 1.0;\n";
        }

        // If enabled, lookup ReflectGreen value, otherwise, ReflectRed value is used
        if (lighting.lut_rg.enable &&
            Pica::Regs::IsLightingSamplerSupported(lighting.config,
                                                   Pica::Regs::LightingSampler::ReflectGreen)) {
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_rg.type, lighting.lut_rg.abs_input);
            std::string value = "(" + std::to_string(lighting.lut_rg.scale) + " * " +
                                GetLutValue(Regs::LightingSampler::ReflectGreen, index) + ")";
            out += "refl_value.g = " + value + ";\n";
        } else {
            out += "refl_value.g = refl_value.r;\n";
        }

        // If enabled, lookup ReflectBlue value, otherwise, ReflectRed value is used
        if (lighting.lut_rb.enable &&
            Pica::Regs::IsLightingSamplerSupported(lighting.config,
                                                   Pica::Regs::LightingSampler::ReflectBlue)) {
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_rb.type, lighting.lut_rb.abs_input);
            std::string value = "(" + std::to_string(lighting.lut_rb.scale) + " * " +
                                GetLutValue(Regs::LightingSampler::ReflectBlue, index) + ")";
            out += "refl_value.b = " + value + ";\n";
        } else {
            out += "refl_value.b = refl_value.r;\n";
        }

        // Specular 1 component
        std::string d1_lut_value = "1.0";
        if (lighting.lut_d1.enable &&
            Pica::Regs::IsLightingSamplerSupported(lighting.config,
                                                   Pica::Regs::LightingSampler::Distribution1)) {
            // Lookup specular "distribution 1" LUT value
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_d1.type, lighting.lut_d1.abs_input);
            d1_lut_value = "(" + std::to_string(lighting.lut_d1.scale) + " * " +
                           GetLutValue(Regs::LightingSampler::Distribution1, index) + ")";
        }
        std::string specular_1 =
            "(" + d1_lut_value + " * refl_value * " + light_src + ".specular_1)";

        // Fresnel
        if (lighting.lut_fr.enable && Pica::Regs::IsLightingSamplerSupported(
                                          lighting.config, Pica::Regs::LightingSampler::Fresnel)) {
            // Lookup fresnel LUT value
            std::string index =
                GetLutIndex(light_config.num, lighting.lut_fr.type, lighting.lut_fr.abs_input);
            std::string value = "(" + std::to_string(lighting.lut_fr.scale) + " * " +
                                GetLutValue(Regs::LightingSampler::Fresnel, index) + ")";

            // Enabled for difffuse lighting alpha component
            if (lighting.fresnel_selector == Pica::Regs::LightingFresnelSelector::PrimaryAlpha ||
                lighting.fresnel_selector == Pica::Regs::LightingFresnelSelector::Both)
                out += "diffuse_sum.a  *= " + value + ";\n";

            // Enabled for the specular lighting alpha component
            if (lighting.fresnel_selector == Pica::Regs::LightingFresnelSelector::SecondaryAlpha ||
                lighting.fresnel_selector == Pica::Regs::LightingFresnelSelector::Both)
                out += "specular_sum.a *= " + value + ";\n";
        }

        // Compute primary fragment color (diffuse lighting) function
        out += "diffuse_sum.rgb += ((" + light_src + ".diffuse * " + dot_product + ") + " +
               light_src + ".ambient) * " + dist_atten + ";\n";

        // Compute secondary fragment color (specular lighting) function
        out += "specular_sum.rgb += (" + specular_0 + " + " + specular_1 + ") * " +
               clamp_highlights + " * " + dist_atten + ";\n";
    }

    // Sum final lighting result
    out += "diffuse_sum.rgb += lighting_global_ambient;\n";
    out += "primary_fragment_color = clamp(diffuse_sum, vec4(0.0), vec4(1.0));\n";
    out += "secondary_fragment_color = clamp(specular_sum, vec4(0.0), vec4(1.0));\n";
}

std::string GenerateFragmentShader(const PicaShaderConfig& config) {
    const auto& state = config.state;

    std::string out = R"(
#version 330 core
#define NUM_TEV_STAGES 6
#define NUM_LIGHTS 8
#define LIGHTING_LUT_SIZE 256
#define FLOAT_255 (255.0 / 256.0)

in vec4 primary_color;
in vec2 texcoord[3];
in float texcoord0_w;
in vec4 normquat;
in vec3 view;

in vec4 gl_FragCoord;

out vec4 color;

struct LightSrc {
    vec3 specular_0;
    vec3 specular_1;
    vec3 diffuse;
    vec3 ambient;
    vec3 position;
    float dist_atten_bias;
    float dist_atten_scale;
};

layout (std140) uniform shader_data {
    vec2 framebuffer_scale;
    int alphatest_ref;
    float depth_scale;
    float depth_offset;
    int scissor_x1;
    int scissor_y1;
    int scissor_x2;
    int scissor_y2;
    vec3 fog_color;
    vec3 lighting_global_ambient;
    LightSrc light_src[NUM_LIGHTS];
    vec4 const_color[NUM_TEV_STAGES];
    vec4 tev_combiner_buffer_color;
};

uniform sampler2D tex[3];
uniform sampler1D lut[6];
uniform usampler1D fog_lut;

// Rotate the vector v by the quaternion q
vec3 quaternion_rotate(vec4 q, vec3 v) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main() {
vec4 primary_fragment_color = vec4(0.0);
vec4 secondary_fragment_color = vec4(0.0);
)";

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (state.alpha_test_func == Regs::CompareFunc::Never) {
        out += "discard; }";
        return out;
    }

    // Append the scissor test
    if (state.scissor_test_mode != Regs::ScissorMode::Disabled) {
        out += "if (";
        // Negate the condition if we have to keep only the pixels outside the scissor box
        if (state.scissor_test_mode == Regs::ScissorMode::Include)
            out += "!";
        out += "(gl_FragCoord.x >= scissor_x1 && "
               "gl_FragCoord.y >= scissor_y1 && "
               "gl_FragCoord.x < scissor_x2 && "
               "gl_FragCoord.y < scissor_y2)) discard;\n";
    }

    out += "float z_over_w = 1.0 - gl_FragCoord.z * 2.0;\n";
    out += "float depth = z_over_w * depth_scale + depth_offset;\n";
    if (state.depthmap_enable == Pica::Regs::DepthBuffering::WBuffering) {
        out += "depth /= gl_FragCoord.w;\n";
    }

    if (state.lighting.enable)
        WriteLighting(out, config);

    out += "vec4 combiner_buffer = vec4(0.0);\n";
    out += "vec4 next_combiner_buffer = tev_combiner_buffer_color;\n";
    out += "vec4 last_tex_env_out = vec4(0.0);\n";

    for (size_t index = 0; index < state.tev_stages.size(); ++index)
        WriteTevStage(out, config, (unsigned)index);

    if (state.alpha_test_func != Regs::CompareFunc::Always) {
        out += "if (";
        AppendAlphaTestCondition(out, state.alpha_test_func);
        out += ") discard;\n";
    }

    // Append fog combiner
    if (state.fog_mode == Regs::FogMode::Fog) {
        // Get index into fog LUT
        if (state.fog_flip) {
            out += "float fog_index = (1.0 - depth) * 128.0;\n";
        } else {
            out += "float fog_index = depth * 128.0;\n";
        }

        // Generate clamped fog factor from LUT for given fog index
        out += "float fog_i = clamp(floor(fog_index), 0.0, 127.0);\n";
        out += "float fog_f = fog_index - fog_i;\n";
        out += "uint fog_lut_entry = texelFetch(fog_lut, int(fog_i), 0).r;\n";
        out += "float fog_lut_entry_difference = float(int((fog_lut_entry & 0x1FFFU) << 19U) >> "
               "19);\n"; // Extract signed difference
        out += "float fog_lut_entry_value = float((fog_lut_entry >> 13U) & 0x7FFU);\n";
        out += "float fog_factor = (fog_lut_entry_value + fog_lut_entry_difference * fog_f) / "
               "2047.0;\n";
        out += "fog_factor = clamp(fog_factor, 0.0, 1.0);\n";

        // Blend the fog
        out += "last_tex_env_out.rgb = mix(fog_color.rgb, last_tex_env_out.rgb, fog_factor);\n";
    }

    out += "gl_FragDepth = depth;\n";
    out += "color = last_tex_env_out;\n";

    out += "}";

    return out;
}

std::string GenerateVertexShader() {
    std::string out = "#version 330 core\n";

    out += "layout(location = " + std::to_string((int)ATTRIBUTE_POSITION) +
           ") in vec4 vert_position;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_COLOR) + ") in vec4 vert_color;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD0) +
           ") in vec2 vert_texcoord0;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD1) +
           ") in vec2 vert_texcoord1;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD2) +
           ") in vec2 vert_texcoord2;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD0_W) +
           ") in float vert_texcoord0_w;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_NORMQUAT) +
           ") in vec4 vert_normquat;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_VIEW) + ") in vec3 vert_view;\n";

    out += R"(
out vec4 primary_color;
out vec2 texcoord[3];
out float texcoord0_w;
out vec4 normquat;
out vec3 view;

void main() {
    primary_color = vert_color;
    texcoord[0] = vert_texcoord0;
    texcoord[1] = vert_texcoord1;
    texcoord[2] = vert_texcoord2;
    texcoord0_w = vert_texcoord0_w;
    normquat = vert_normquat;
    view = vert_view;
    gl_Position = vec4(vert_position.x, vert_position.y, -vert_position.z, vert_position.w);
}
)";

    return out;
}

} // namespace GLShader
