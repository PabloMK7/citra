// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if CITRA_ARCH(arm64)

#include <memory>
#include <unordered_map>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader {

class JitShader;

class JitA64Engine final : public ShaderEngine {
public:
    JitA64Engine();
    ~JitA64Engine() override;

    void SetupBatch(ShaderSetup& setup, unsigned int entry_point) override;
    void Run(const ShaderSetup& setup, UnitState& state) const override;

private:
    std::unordered_map<u64, std::unique_ptr<JitShader>> cache;
};

} // namespace Pica::Shader

#endif // CITRA_ARCH(arm64)
