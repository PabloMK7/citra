// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <functional>
#include <optional>
#include <string>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace OpenGL::ShaderDecompiler {

using ProgramCode = std::array<u32, Pica::Shader::MAX_PROGRAM_CODE_LENGTH>;
using SwizzleData = std::array<u32, Pica::Shader::MAX_SWIZZLE_DATA_LENGTH>;
using RegGetter = std::function<std::string(u32)>;

std::string GetCommonDeclarations();

std::optional<std::string> DecompileProgram(const ProgramCode& program_code,
                                            const SwizzleData& swizzle_data, u32 main_offset,
                                            const RegGetter& inputreg_getter,
                                            const RegGetter& outputreg_getter, bool sanitize_mul);

} // namespace OpenGL::ShaderDecompiler
