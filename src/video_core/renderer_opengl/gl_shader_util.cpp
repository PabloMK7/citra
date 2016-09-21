// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include <glad/glad.h>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace GLShader {

GLuint LoadProgram(const char* vertex_shader, const char* fragment_shader) {

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
            LOG_ERROR(Render_OpenGL, "Error compiling fragment shader:\n%s",
                      &fragment_shader_error[0]);
        }
    }

    // Link the program
    LOG_DEBUG(Render_OpenGL, "Linking program...");

    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

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

} // namespace GLShader
