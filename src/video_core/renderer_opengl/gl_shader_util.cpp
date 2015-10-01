// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.


#include "gl_shader_util.h"
#include "gl_rasterizer.h"
#include "common/logging/log.h"

#include "video_core/pica.h"

#include <algorithm>
#include <vector>

#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace ShaderUtil {

GLuint LoadShaders(const char* vertex_shader, const char* fragment_shader) {

    // Create the shaders
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    GLint result = GL_FALSE;
    int info_log_length;

    // Compile Vertex Shader
    LOG_DEBUG(Render_OpenGL, "Compiling vertex shader...");

    glShaderSource(vertex_shader_id, 1, &vertex_shader, nullptr);
    glCompileShader(vertex_shader_id);

    // Check Vertex Shader
    glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> vertex_shader_error(info_log_length);
        glGetShaderInfoLog(vertex_shader_id, info_log_length, nullptr, &vertex_shader_error[0]);
        if (result) {
            LOG_DEBUG(Render_OpenGL, "%s", &vertex_shader_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error compiling vertex shader:\n%s", &vertex_shader_error[0]);
        }
    }

    // Compile Fragment Shader
    LOG_DEBUG(Render_OpenGL, "Compiling fragment shader...");

    glShaderSource(fragment_shader_id, 1, &fragment_shader, nullptr);
    glCompileShader(fragment_shader_id);

    // Check Fragment Shader
    glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> fragment_shader_error(info_log_length);
        glGetShaderInfoLog(fragment_shader_id, info_log_length, nullptr, &fragment_shader_error[0]);
        if (result) {
            LOG_DEBUG(Render_OpenGL, "%s", &fragment_shader_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error compiling fragment shader:\n%s", &fragment_shader_error[0]);
        }
    }

    // Link the program
    LOG_DEBUG(Render_OpenGL, "Linking program...");

    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    glBindAttribLocation(program_id, Attributes::ATTRIBUTE_POSITION, "vert_position");
    glBindAttribLocation(program_id, Attributes::ATTRIBUTE_COLOR, "vert_color");
    glBindAttribLocation(program_id, Attributes::ATTRIBUTE_TEXCOORDS + 0, "vert_texcoords0");
    glBindAttribLocation(program_id, Attributes::ATTRIBUTE_TEXCOORDS + 1, "vert_texcoords1");
    glBindAttribLocation(program_id, Attributes::ATTRIBUTE_TEXCOORDS + 2, "vert_texcoords2");

    glLinkProgram(program_id);

    // Check the program
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> program_error(info_log_length);
        glGetProgramInfoLog(program_id, info_log_length, nullptr, &program_error[0]);
        if (result) {
            LOG_DEBUG(Render_OpenGL, "%s", &program_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error linking shader:\n%s", &program_error[0]);
        }
    }

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}

}

namespace ShaderCache
{

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

void AppendSource(std::string& shader, Pica::Regs::TevStageConfig::Source source, const std::string& index_name) {
    using Source = Pica::Regs::TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        shader += "o[2]";
        break;
    case Source::PrimaryFragmentColor:
        // HACK: Until we implement fragment lighting, use primary_color
        shader += "o[2]";
        break;
    case Source::SecondaryFragmentColor:
        // HACK: Until we implement fragment lighting, use zero
        shader += "vec4(0.0, 0.0, 0.0, 0.0)";
        break;
    case Source::Texture0:
        shader += "texture(tex[0], o[3].xy)";
        break;
    case Source::Texture1:
        shader += "texture(tex[1], o[3].zw)";
        break;
    case Source::Texture2: // TODO: Unverified
        shader += "texture(tex[2], o[5].zw)";
        break;
    case Source::PreviousBuffer:
        shader += "g_combiner_buffer";
        break;
    case Source::Constant:
        shader += "const_color[" + index_name + "]";
        break;
    case Source::Previous:
        shader += "g_last_tex_env_out";
        break;
    default:
        shader += "vec4(0.0)";
        LOG_CRITICAL(Render_OpenGL, "Unknown source op %u", source);
        break;
    }
}

void AppendColorModifier(std::string& shader, Pica::Regs::TevStageConfig::ColorModifier modifier, Pica::Regs::TevStageConfig::Source source, const std::string& index_name) {
    using ColorModifier = Pica::Regs::TevStageConfig::ColorModifier;
    switch (modifier) {
        case ColorModifier::SourceColor:
            AppendSource(shader, source, index_name);
            shader += ".rgb";
            break;
        case ColorModifier::OneMinusSourceColor:
            shader += "vec3(1.0) - ";
            AppendSource(shader, source, index_name);
            shader += ".rgb";
            break;
        case ColorModifier::SourceAlpha:
            AppendSource(shader, source, index_name);
            shader += ".aaa";
            break;
        case ColorModifier::OneMinusSourceAlpha:
            shader += "vec3(1.0) - ";
            AppendSource(shader, source, index_name);
            shader += ".aaa";
            break;
        case ColorModifier::SourceRed:
            AppendSource(shader, source, index_name);
            shader += ".rrr";
            break;
        case ColorModifier::OneMinusSourceRed:
            shader += "vec3(1.0) - ";
            AppendSource(shader, source, index_name);
            shader += ".rrr";
            break;
        case ColorModifier::SourceGreen:
            AppendSource(shader, source, index_name);
            shader += ".ggg";
            break;
        case ColorModifier::OneMinusSourceGreen:
            shader += "vec3(1.0) - ";
            AppendSource(shader, source, index_name);
            shader += ".ggg";
            break;
        case ColorModifier::SourceBlue:
            AppendSource(shader, source, index_name);
            shader += ".bbb";
            break;
        case ColorModifier::OneMinusSourceBlue:
            shader += "vec3(1.0) - ";
            AppendSource(shader, source, index_name);
            shader += ".bbb";
            break;
        default:
            shader += "vec3(0.0)";
            LOG_CRITICAL(Render_OpenGL, "Unknown color modifier op %u", modifier);
            break;
    }
}

void AppendAlphaModifier(std::string& shader, Pica::Regs::TevStageConfig::AlphaModifier modifier, Pica::Regs::TevStageConfig::Source source, const std::string& index_name) {
    using AlphaModifier = Pica::Regs::TevStageConfig::AlphaModifier;
    switch (modifier) {
        case AlphaModifier::SourceAlpha:
            AppendSource(shader, source, index_name);
            shader += ".a";
            break;
        case AlphaModifier::OneMinusSourceAlpha:
            shader += "1.0 - ";
            AppendSource(shader, source, index_name);
            shader += ".a";
            break;
        case AlphaModifier::SourceRed:
            AppendSource(shader, source, index_name);
            shader += ".r";
            break;
        case AlphaModifier::OneMinusSourceRed:
            shader += "1.0 - ";
            AppendSource(shader, source, index_name);
            shader += ".r";
            break;
        case AlphaModifier::SourceGreen:
            AppendSource(shader, source, index_name);
            shader += ".g";
            break;
        case AlphaModifier::OneMinusSourceGreen:
            shader += "1.0 - ";
            AppendSource(shader, source, index_name);
            shader += ".g";
            break;
        case AlphaModifier::SourceBlue:
            AppendSource(shader, source, index_name);
            shader += ".b";
            break;
        case AlphaModifier::OneMinusSourceBlue:
            shader += "1.0 - ";
            AppendSource(shader, source, index_name);
            shader += ".b";
            break;
        default:
            shader += "vec3(0.0)";
            LOG_CRITICAL(Render_OpenGL, "Unknown alpha modifier op %u", modifier);
            break;
    }
}

void AppendColorCombiner(std::string& shader, Pica::Regs::TevStageConfig::Operation operation, const std::string& variable_name) {
    using Operation = Pica::Regs::TevStageConfig::Operation;

    switch (operation) {
        case Operation::Replace:
            shader += variable_name + "[0]";
            break;
        case Operation::Modulate:
            shader += variable_name + "[0] * " + variable_name + "[1]";
            break;
        case Operation::Add:
            shader += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0)";
            break;
        case Operation::AddSigned:
            shader += "clamp(" + variable_name + "[0] + " + variable_name + "[1] - vec3(0.5), 0.0, 1.0)";
            break;
        case Operation::Lerp:
            shader += variable_name + "[0] * " + variable_name + "[2] + " + variable_name + "[1] * (vec3(1.0) - " + variable_name + "[2])";
            break;
        case Operation::Subtract:
            shader += "max(" + variable_name + "[0] - " + variable_name + "[1], 0.0)";
            break;
        case Operation::MultiplyThenAdd:
            shader += "min(" + variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2], 1.0)";
            break;
        case Operation::AddThenMultiply:
            shader += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0) * " + variable_name + "[2]";
            break;
        default:
            shader += "0.0";
            LOG_CRITICAL(Render_OpenGL, "Unknown color comb op %u", operation);
            break;
    }
}

void AppendAlphaCombiner(std::string& shader, Pica::Regs::TevStageConfig::Operation operation, const std::string& variable_name) {
    using Operation = Pica::Regs::TevStageConfig::Operation;
    switch (operation) {
        case Operation::Replace:
            shader += variable_name + "[0]";
            break;
        case Operation::Modulate:
            shader += variable_name + "[0] * " + variable_name + "[1]";
            break;
        case Operation::Add:
            shader += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0)";
            break;
        case Operation::AddSigned:
            shader += "clamp(" + variable_name + "[0] + " + variable_name + "[1] - 0.5, 0.0, 1.0)";
            break;
        case Operation::Lerp:
            shader += variable_name + "[0] * " + variable_name + "[2] + " + variable_name + "[1] * (1.0 - " + variable_name + "[2])";
            break;
        case Operation::Subtract:
            shader += "max(" + variable_name + "[0] - " + variable_name + "[1], 0.0)";
            break;
        case Operation::MultiplyThenAdd:
            shader += "min(" + variable_name + "[0] * " + variable_name + "[1] + " + variable_name + "[2], 1.0)";
            break;
        case Operation::AddThenMultiply:
            shader += "min(" + variable_name + "[0] + " + variable_name + "[1], 1.0) * " + variable_name + "[2]";
            break;
        default:
            shader += "0.0";
            LOG_CRITICAL(Render_OpenGL, "Unknown alpha combiner op %u", operation);
            break;
    }
}

void AppendAlphaTestCondition(std::string& shader, Pica::Regs::CompareFunc func) {
    using CompareFunc = Pica::Regs::CompareFunc;
    switch (func) {
        case CompareFunc::Never:
            shader += "true";
            break;
        case CompareFunc::Always:
            shader += "false";
            break;
        case CompareFunc::Equal:
            shader += "int(g_last_tex_env_out.a * 255.0f) != alphatest_ref";
            break;
        case CompareFunc::NotEqual:
            shader += "int(g_last_tex_env_out.a * 255.0f) == alphatest_ref";
            break;
        case CompareFunc::LessThan:
            shader += "int(g_last_tex_env_out.a * 255.0f) >= alphatest_ref";
            break;
        case CompareFunc::LessThanOrEqual:
            shader += "int(g_last_tex_env_out.a * 255.0f) > alphatest_ref";
            break;
        case CompareFunc::GreaterThan:
            shader += "int(g_last_tex_env_out.a * 255.0f) <= alphatest_ref";
            break;
        case CompareFunc::GreaterThanOrEqual:
            shader += "int(g_last_tex_env_out.a * 255.0f) < alphatest_ref";
            break;
        default:
            shader += "false";
            LOG_CRITICAL(Render_OpenGL, "Unknown alpha test condition %u", func);
            break;
    }
}

std::string GenerateFragmentShader(const ShaderCacheKey& config) {
    std::string shader = R"(
#version 150 core

#define NUM_VTX_ATTR 7
#define NUM_TEV_STAGES 6

in vec4 o[NUM_VTX_ATTR];
out vec4 color;

uniform int alphatest_ref;
uniform vec4 const_color[NUM_TEV_STAGES];
uniform sampler2D tex[3];

uniform vec4 tev_combiner_buffer_color;

void main(void) {
    vec4 g_combiner_buffer = tev_combiner_buffer_color;
    vec4 g_last_tex_env_out = vec4(0.0, 0.0, 0.0, 0.0);
)";

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (config.alpha_test_func == Pica::Regs::CompareFunc::Never) {
        shader += "discard;";
        return shader;
    }

    auto& tev_stages = config.tev_stages;
    for (unsigned tev_stage_index = 0; tev_stage_index < tev_stages.size(); ++tev_stage_index) {
        auto& tev_stage = tev_stages[tev_stage_index];
        if (!IsPassThroughTevStage(tev_stage)) {
            std::string index_name = std::to_string(tev_stage_index);

            shader += "vec3 color_results_" + index_name + "[3] = vec3[3](";
            AppendColorModifier(shader, tev_stage.color_modifier1, tev_stage.color_source1, index_name);
            shader += ", ";
            AppendColorModifier(shader, tev_stage.color_modifier2, tev_stage.color_source2, index_name);
            shader += ", ";
            AppendColorModifier(shader, tev_stage.color_modifier3, tev_stage.color_source3, index_name);
            shader += ");\n";

            shader += "vec3 color_output_" + index_name + " = ";
            AppendColorCombiner(shader, tev_stage.color_op, "color_results_" + index_name);
            shader += ";\n";

            shader += "float alpha_results_" + index_name + "[3] = float[3](";
            AppendAlphaModifier(shader, tev_stage.alpha_modifier1, tev_stage.alpha_source1, index_name);
            shader += ", ";
            AppendAlphaModifier(shader, tev_stage.alpha_modifier2, tev_stage.alpha_source2, index_name);
            shader += ", ";
            AppendAlphaModifier(shader, tev_stage.alpha_modifier3, tev_stage.alpha_source3, index_name);
            shader += ");\n";

            shader += "float alpha_output_" + index_name + " = ";
            AppendAlphaCombiner(shader, tev_stage.alpha_op, "alpha_results_" + index_name);
            shader += ";\n";

            shader += "g_last_tex_env_out = vec4(min(color_output_" + index_name + " * " + std::to_string(tev_stage.GetColorMultiplier()) + ".0, 1.0), min(alpha_output_" + index_name + " * " + std::to_string(tev_stage.GetAlphaMultiplier()) + ".0, 1.0));\n";
        }

        if (config.TevStageUpdatesCombinerBufferColor(tev_stage_index))
            shader += "g_combiner_buffer.rgb = g_last_tex_env_out.rgb;\n";

        if (config.TevStageUpdatesCombinerBufferAlpha(tev_stage_index))
            shader += "g_combiner_buffer.a = g_last_tex_env_out.a;\n";
    }

    if (config.alpha_test_func != Pica::Regs::CompareFunc::Always) {
        shader += "if (";
        AppendAlphaTestCondition(shader, config.alpha_test_func);
        shader += ") {\n discard;\n }\n";
    }

    shader += "color = g_last_tex_env_out;\n}";
    return shader;
}
}
