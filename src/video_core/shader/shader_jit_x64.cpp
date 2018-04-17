// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_jit_x64.h"
#include "video_core/shader/shader_jit_x64_compiler.h"

namespace Pica {
namespace Shader {

JitX64Engine::JitX64Engine() = default;
JitX64Engine::~JitX64Engine() = default;

void JitX64Engine::SetupBatch(ShaderSetup& setup, unsigned int entry_point) {
    ASSERT(entry_point < MAX_PROGRAM_CODE_LENGTH);
    setup.engine_data.entry_point = entry_point;

    u64 code_hash = setup.GetProgramCodeHash();
    u64 swizzle_hash = setup.GetSwizzleDataHash();

    u64 cache_key = code_hash ^ swizzle_hash;
    auto iter = cache.find(cache_key);
    if (iter != cache.end()) {
        setup.engine_data.cached_shader = iter->second.get();
    } else {
        auto shader = std::make_unique<JitShader>();
        shader->Compile(&setup.program_code, &setup.swizzle_data);
        setup.engine_data.cached_shader = shader.get();
        cache.emplace_hint(iter, cache_key, std::move(shader));
    }
}

MICROPROFILE_DECLARE(GPU_Shader);

void JitX64Engine::Run(const ShaderSetup& setup, UnitState& state) const {
    ASSERT(setup.engine_data.cached_shader != nullptr);

    MICROPROFILE_SCOPE(GPU_Shader);

    const JitShader* shader = static_cast<const JitShader*>(setup.engine_data.cached_shader);
    shader->Run(setup, state, setup.engine_data.entry_point);
}

} // namespace Shader
} // namespace Pica
