// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace GLShader {

GLuint LoadShader(const char* source, GLenum type) {
    const char* debug_type;
    switch (type) {
    case GL_VERTEX_SHADER:
        debug_type = "vertex";
        break;
    case GL_GEOMETRY_SHADER:
        debug_type = "geometry";
        break;
    case GL_FRAGMENT_SHADER:
        debug_type = "fragment";
        break;
    default:
        UNREACHABLE();
    }

    GLuint shader_id = glCreateShader(type);
    glShaderSource(shader_id, 1, &source, nullptr);
    LOG_DEBUG(Render_OpenGL, "Compiling {} shader...", debug_type);
    glCompileShader(shader_id);

    GLint result = GL_FALSE;
    GLint info_log_length;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> shader_error(info_log_length);
        glGetShaderInfoLog(shader_id, info_log_length, nullptr, &shader_error[0]);
        if (result == GL_TRUE) {
            LOG_DEBUG(Render_OpenGL, "{}", &shader_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error compiling {} shader:\n{}", debug_type,
                        &shader_error[0]);
            LOG_ERROR(Render_OpenGL, "Shader source code:\n{}", source);
        }
    }
    return shader_id;
}

GLuint LoadProgram(bool separable_program, const std::vector<GLuint>& shaders) {
    // Link the program
    LOG_DEBUG(Render_OpenGL, "Linking program...");

    GLuint program_id = glCreateProgram();

    for (GLuint shader : shaders) {
        if (shader != 0) {
            glAttachShader(program_id, shader);
        }
    }

    if (separable_program) {
        glProgramParameteri(program_id, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }

    glLinkProgram(program_id);

    // Check the program
    GLint result = GL_FALSE;
    GLint info_log_length;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> program_error(info_log_length);
        glGetProgramInfoLog(program_id, info_log_length, nullptr, &program_error[0]);
        if (result == GL_TRUE) {
            LOG_DEBUG(Render_OpenGL, "{}", &program_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error linking shader:\n{}", &program_error[0]);
        }
    }

    ASSERT_MSG(result == GL_TRUE, "Shader not linked");

    for (GLuint shader : shaders) {
        if (shader != 0) {
            glDetachShader(program_id, shader);
        }
    }

    return program_id;
}

} // namespace GLShader
