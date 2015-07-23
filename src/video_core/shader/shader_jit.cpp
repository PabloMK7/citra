// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/pica.h"

#include "shader.h"
#include "shader_jit.h"

namespace Pica {

namespace Shader {

JitShader::JitShader() : jitted(nullptr) {
}

void JitShader::DoJit(JitCompiler& jit) {
    jitted = jit.Compile();
}

void JitShader::Run(UnitState& state) {
    if (jitted)
        jitted(&state);
}

JitCompiler::JitCompiler() {
    AllocCodeSpace(1024 * 1024 * 4);
}

void JitCompiler::Clear() {
    ClearCodeSpace();
}

} // namespace Shader

} // namespace Pica
