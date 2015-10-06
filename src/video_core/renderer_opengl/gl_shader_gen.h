// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace GLShader {

std::string GenerateVertexShader();

std::string GenerateFragmentShader(const ShaderCacheKey& config);

} // namespace GLShader
