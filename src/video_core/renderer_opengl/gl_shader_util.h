// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <glad/glad.h>

namespace OpenGL {

// High precision may or may not supported in GLES3. If it isn't, use medium precision instead.
static constexpr char fragment_shader_precision_OES[] = R"(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp int;
precision highp float;
precision highp samplerBuffer;
precision highp uimage2D;
#else
precision mediump int;
precision mediump float;
precision mediump samplerBuffer;
precision mediump uimage2D;
#endif // GL_FRAGMENT_PRECISION_HIGH
)";

/**
 * Utility function to create and compile an OpenGL GLSL shader
 * @param source String of the GLSL shader program
 * @param type Type of the shader (GL_VERTEX_SHADER, GL_GEOMETRY_SHADER or GL_FRAGMENT_SHADER)
 */
GLuint LoadShader(const char* source, GLenum type);

/**
 * Utility function to create and link an OpenGL GLSL shader program
 * @param separable_program whether to create a separable program
 * @param shaders ID of shaders to attach to the program
 * @returns Handle of the newly created OpenGL program object
 */
GLuint LoadProgram(bool separable_program, const std::vector<GLuint>& shaders);

} // namespace OpenGL
