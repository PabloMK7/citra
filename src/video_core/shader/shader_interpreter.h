// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/shader/debug_data.h"
#include "video_core/shader/shader.h"

namespace Pica {

namespace Shader {

class InterpreterEngine final : public ShaderEngine {
public:
    void SetupBatch(const ShaderSetup* setup) override;
    void Run(UnitState& state, unsigned int entry_point) const override;

    /**
     * Produce debug information based on the given shader and input vertex
     * @param input Input vertex into the shader
     * @param num_attributes The number of vertex shader attributes
     * @param config Configuration object for the shader pipeline
     * @return Debug information for this shader with regards to the given vertex
     */
    DebugData<true> ProduceDebugInfo(const InputVertex& input, int num_attributes,
                                     unsigned int entry_point) const;

private:
    const ShaderSetup* setup = nullptr;
};

} // namespace

} // namespace
