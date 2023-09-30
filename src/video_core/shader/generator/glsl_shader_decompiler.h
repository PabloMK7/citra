// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader::Generator::GLSL {

using RegGetter = std::function<std::string(u32)>;

std::string DecompileProgram(const Pica::Shader::ProgramCode& program_code,
                             const Pica::Shader::SwizzleData& swizzle_data, u32 main_offset,
                             const RegGetter& inputreg_getter, const RegGetter& outputreg_getter,
                             bool sanitize_mul);

} // namespace Pica::Shader::Generator::GLSL
