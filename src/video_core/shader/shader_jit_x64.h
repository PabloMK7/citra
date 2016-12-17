// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica {
namespace Shader {

class JitShader;

class JitX64Engine final : public ShaderEngine {
public:
    JitX64Engine();
    ~JitX64Engine() override;

    void SetupBatch(const ShaderSetup* setup) override;
    void Run(UnitState& state, unsigned int entry_point) const override;

private:
    const ShaderSetup* setup = nullptr;

    std::unordered_map<u64, std::unique_ptr<JitShader>> cache;
    const JitShader* cached_shader = nullptr;
};

} // namespace Shader
} // namespace Pica
