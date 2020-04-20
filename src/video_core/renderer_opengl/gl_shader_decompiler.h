// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace OpenGL::ShaderDecompiler {

using RegGetter = std::function<std::string(u32)>;

struct ProgramResult {
    std::string code;
};

std::string GetCommonDeclarations();

std::optional<ProgramResult> DecompileProgram(const Pica::Shader::ProgramCode& program_code,
                                              const Pica::Shader::SwizzleData& swizzle_data,
                                              u32 main_offset, const RegGetter& inputreg_getter,
                                              const RegGetter& outputreg_getter, bool sanitize_mul);

} // namespace OpenGL::ShaderDecompiler
