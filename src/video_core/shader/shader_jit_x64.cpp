// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hash.h"
#include "common/microprofile.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_jit_x64.h"
#include "video_core/shader/shader_jit_x64_compiler.h"

namespace Pica {
namespace Shader {

JitX64Engine::JitX64Engine() = default;
JitX64Engine::~JitX64Engine() = default;

void JitX64Engine::SetupBatch(const ShaderSetup* setup_) {
    cached_shader = nullptr;
    setup = setup_;
    if (setup == nullptr)
        return;

    u64 code_hash = Common::ComputeHash64(&setup->program_code, sizeof(setup->program_code));
    u64 swizzle_hash = Common::ComputeHash64(&setup->swizzle_data, sizeof(setup->swizzle_data));

    u64 cache_key = code_hash ^ swizzle_hash;
    auto iter = cache.find(cache_key);
    if (iter != cache.end()) {
        cached_shader = iter->second.get();
    } else {
        auto shader = std::make_unique<JitShader>();
        shader->Compile(&setup->program_code, &setup->swizzle_data);
        cached_shader = shader.get();
        cache.emplace_hint(iter, cache_key, std::move(shader));
    }
}

MICROPROFILE_DECLARE(GPU_Shader);

void JitX64Engine::Run(UnitState& state, unsigned int entry_point) const {
    ASSERT(setup != nullptr);
    ASSERT(cached_shader != nullptr);
    ASSERT(entry_point < 1024);

    MICROPROFILE_SCOPE(GPU_Shader);

    cached_shader->Run(*setup, state, entry_point);
}

} // namespace Shader
} // namespace Pica
