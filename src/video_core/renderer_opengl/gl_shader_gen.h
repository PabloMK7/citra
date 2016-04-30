// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

struct PicaShaderConfig;

namespace GLShader {

/**
 * Generates the GLSL vertex shader program source code for the current Pica state
 * @returns String of the shader source code
 */
std::string GenerateVertexShader();

/**
 * Generates the GLSL fragment shader program source code for the current Pica state
 * @param config ShaderCacheKey object generated for the current Pica state, used for the shader
 *               configuration (NOTE: Use state in this struct only, not the Pica registers!)
 * @returns String of the shader source code
 */
std::string GenerateFragmentShader(const PicaShaderConfig& config);

} // namespace GLShader
