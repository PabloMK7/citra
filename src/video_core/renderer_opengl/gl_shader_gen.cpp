// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/pica.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"

using Pica::Regs;
using TevStageConfig = Regs::TevStageConfig;

namespace GLShader {

/// Detects if a TEV stage is configured to be skipped (to avoid generating unnecessary code)
static bool IsPassThroughTevStage(const TevStageConfig& stage) {
    return (stage.color_op             == TevStageConfig::Operation::Replace &&
            stage.alpha_op             == TevStageConfig::Operation::Replace &&
            stage.color_source1        == TevStageConfig::Source::Previous &&
            stage.alpha_source1        == TevStageConfig::Source::Previous &&
            stage.color_modifier1      == TevStageConfig::ColorModifier::SourceColor &&
            stage.alpha_modifier1      == TevStageConfig::AlphaModifier::SourceAlpha &&
            stage.GetColorMultiplier() == 1 &&
            stage.GetAlphaMultiplier() == 1);
}

/// Writes the specified TEV stage source component(s)
static void AppendSource(std::string& out, TevStageConfig::Source source,
        const std::string& index_name) {
    using Source = TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        out += "primary_color";
        break;
    case Source::PrimaryFragmentColor:
        // HACK: Until we implement fragment lighting, use primary_color
        out += "primary_color";
        break;
    case Source::SecondaryFragmentColor:
        // HACK: Until we implement fragment lighting, use zero
        out += "vec4(0.0)";
        break;
    case Source::Texture0:
        out += "texture(tex[0], texcoord[0])";
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
static void AppendColorModifier(std::string& out, TevStageConfig::ColorModifier modifier,
        TevStageConfig::Source source, const std::string& index_name) {
    using ColorModifier = TevStageConfig::ColorModifier;
    switch (modifier) {
    case ColorModifier::SourceColor:
        AppendSource(out, source, index_name);
        out += ".rgb";
        break;
    case ColorModifier::OneMinusSourceColor:
        out += "vec3(1.0) - ";
        AppendSource(out, source, index_name);
        out += ".rgb";
        break;
    case ColorModifier::SourceAlpha:
        AppendSource(out, source, index_name);
        out += ".aaa";
        break;
    case ColorModifier::OneMinusSourceAlpha:
        out += "vec3(1.0) - ";
        AppendSource(out, source, index_name);
        out += ".aaa";
        break;
    case ColorModifier::SourceRed:
        AppendSource(out, source, index_name);
        out += ".rrr";
        break;
    case ColorModifier::OneMinusSourceRed:
        out += "vec3(1.0) - ";
        AppendSource(out, source, index_name);
        out += ".rrr";
        break;
    case ColorModifier::SourceGreen:
        AppendSource(out, source, index_name);
        out += ".ggg";
        break;
    case ColorModifier::OneMinusSourceGreen:
        out += "vec3(1.0) - ";
        AppendSource(out, source, index_name);
        out += ".ggg";
        break;
    case ColorModifier::SourceBlue:
        AppendSource(out, source, index_name);
        out += ".bbb";
        break;
    case ColorModifier::OneMinusSourceBlue:
        out += "vec3(1.0) - ";
        AppendSource(out, source, index_name);
        out += ".bbb";
        break;
    default:
        out += "vec3(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown color modifier op %u", modifier);
        break;
    }
}

/// Writes the alpha component to use for the specified TEV stage alpha modifier
static void AppendAlphaModifier(std::string& out, TevStageConfig::AlphaModifier modifier,
        TevStageConfig::Source source, const std::string& index_name) {
    using AlphaModifier = TevStageConfig::AlphaModifier;
    switch (modifier) {
    case AlphaModifier::SourceAlpha:
        AppendSource(out, source, index_name);
        out += ".a";
        break;
    case AlphaModifier::OneMinusSourceAlpha:
        out += "1.0 - ";
        AppendSource(out, source, index_name);
        out += ".a";
        break;
    case AlphaModifier::SourceRed:
        AppendSource(out, source, index_name);
        out += ".r";
        break;
    case AlphaModifier::OneMinusSourceRed:
        out += "1.0 - ";
        AppendSource(out, source, index_name);
        out += ".r";
        break;
    case AlphaModifier::SourceGreen:
        AppendSource(out, source, index_name);
        out += ".g";
        break;
    case AlphaModifier::OneMinusSourceGreen:
        out += "1.0 - ";
        AppendSource(out, source, index_name);
        out += ".g";
        break;
    case AlphaModifier::SourceBlue:
        AppendSource(out, source, index_name);
        out += ".b";
        break;
    case AlphaModifier::OneMinusSourceBlue:
        out += "1.0 - ";
        AppendSource(out, source, index_name);
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
        // TODO(bunnei): Verify if HW actually does this per-component, otherwise we can just use builtin lerp
        out += variable_name + "[0] * " + variable_name + "[2] + " + variable_name + "[1] * (vec3(1.0) - " + variable_name + "[2])";
        break;
    case Operation::Subtract:
        out += variable_name + "[0] - " + variable_name + "[1]";
        break;
    case Operation::MultiplyThenAdd:
        out += variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2]";
        break;
    case Operation::AddThenMultiply:
        out += "min(" + variable_name + "[0] + " + variable_name + "[1], vec3(1.0)) * " + variable_name + "[2]";
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
        out += variable_name + "[0] * " + variable_name + "[2] + " + variable_name + "[1] * (1.0 - " + variable_name + "[2])";
        break;
    case Operation::Subtract:
        out += variable_name + "[0] - " + variable_name + "[1]";
        break;
    case Operation::MultiplyThenAdd:
        out += variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2]";
        break;
    case Operation::AddThenMultiply:
        out += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0) * " + variable_name + "[2]";
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
    case CompareFunc::GreaterThanOrEqual:
    {
        static const char* op[] = { "!=", "==", ">=", ">", "<=", "<", };
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
    auto& stage = config.tev_stages[index];
    if (!IsPassThroughTevStage(stage)) {
        std::string index_name = std::to_string(index);

        out += "vec3 color_results_" + index_name + "[3] = vec3[3](";
        AppendColorModifier(out, stage.color_modifier1, stage.color_source1, index_name);
        out += ", ";
        AppendColorModifier(out, stage.color_modifier2, stage.color_source2, index_name);
        out += ", ";
        AppendColorModifier(out, stage.color_modifier3, stage.color_source3, index_name);
        out += ");\n";

        out += "vec3 color_output_" + index_name + " = ";
        AppendColorCombiner(out, stage.color_op, "color_results_" + index_name);
        out += ";\n";

        out += "float alpha_results_" + index_name + "[3] = float[3](";
        AppendAlphaModifier(out, stage.alpha_modifier1, stage.alpha_source1, index_name);
        out += ", ";
        AppendAlphaModifier(out, stage.alpha_modifier2, stage.alpha_source2, index_name);
        out += ", ";
        AppendAlphaModifier(out, stage.alpha_modifier3, stage.alpha_source3, index_name);
        out += ");\n";

        out += "float alpha_output_" + index_name + " = ";
        AppendAlphaCombiner(out, stage.alpha_op, "alpha_results_" + index_name);
        out += ";\n";

        out += "last_tex_env_out = vec4("
            "clamp(color_output_" + index_name + " * " + std::to_string(stage.GetColorMultiplier()) + ".0, vec3(0.0), vec3(1.0)),"
            "clamp(alpha_output_" + index_name + " * " + std::to_string(stage.GetAlphaMultiplier()) + ".0, 0.0, 1.0));\n";
    }

    if (config.TevStageUpdatesCombinerBufferColor(index))
        out += "combiner_buffer.rgb = last_tex_env_out.rgb;\n";

    if (config.TevStageUpdatesCombinerBufferAlpha(index))
        out += "combiner_buffer.a = last_tex_env_out.a;\n";
}

std::string GenerateFragmentShader(const PicaShaderConfig& config) {
    std::string out = R"(
#version 330
#extension GL_ARB_explicit_uniform_location : require

#define NUM_TEV_STAGES 6

in vec4 primary_color;
in vec2 texcoord[3];

out vec4 color;

layout (std140) uniform shader_data {
    vec4 const_color[NUM_TEV_STAGES];
    vec4 tev_combiner_buffer_color;
    int alphatest_ref;
};

)";

    using Uniform = RasterizerOpenGL::PicaShader::Uniform;
    out += "layout(location = " + std::to_string((int)Uniform::Texture0) + ") uniform sampler2D tex[3];\n";

    out += "void main() {\n";
    out += "vec4 combiner_buffer = tev_combiner_buffer_color;\n";
    out += "vec4 last_tex_env_out = vec4(0.0);\n";

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (config.alpha_test_func == Regs::CompareFunc::Never) {
        out += "discard; }";
        return out;
    }

    for (size_t index = 0; index < config.tev_stages.size(); ++index)
        WriteTevStage(out, config, (unsigned)index);

    if (config.alpha_test_func != Regs::CompareFunc::Always) {
        out += "if (";
        AppendAlphaTestCondition(out, config.alpha_test_func);
        out += ") discard;\n";
    }

    out += "color = last_tex_env_out;\n}";

    return out;
}

std::string GenerateVertexShader() {
    std::string out = "#version 330\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_POSITION)  + ") in vec4 vert_position;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_COLOR)     + ") in vec4 vert_color;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD0) + ") in vec2 vert_texcoord0;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD1) + ") in vec2 vert_texcoord1;\n";
    out += "layout(location = " + std::to_string((int)ATTRIBUTE_TEXCOORD2) + ") in vec2 vert_texcoord2;\n";

    out += R"(
out vec4 primary_color;
out vec2 texcoord[3];

void main() {
    primary_color = vert_color;
    texcoord[0] = vert_texcoord0;
    texcoord[1] = vert_texcoord1;
    texcoord[2] = vert_texcoord2;
    gl_Position = vec4(vert_position.x, -vert_position.y, -vert_position.z, vert_position.w);
}
)";

    return out;
}

} // namespace GLShader
