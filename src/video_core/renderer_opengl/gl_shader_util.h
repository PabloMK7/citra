// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>

namespace GLShader {

/**
 * Utility function to create and compile an OpenGL GLSL shader program (vertex + fragment shader)
 * @param vertex_shader String of the GLSL vertex shader program
 * @param fragment_shader String of the GLSL fragment shader program
 * @returns Handle of the newly created OpenGL shader object
 */
GLuint LoadProgram(const char* vertex_shader, const char* fragment_shader);

} // namespace GLShader
