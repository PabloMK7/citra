// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "video_core/pica/shader_unit.h"

namespace Pica {

struct RegsInternal;
struct GeometryShaderUnit;
struct ShaderSetup;
class ShaderEngine;

class GeometryPipelineBackend;
class GeometryPipeline_Point;
class GeometryPipeline_VariablePrimitive;
class GeometryPipeline_FixedPrimitive;

/// A pipeline receiving from vertex shader and sending to geometry shader and primitive assembler
class GeometryPipeline {
public:
    explicit GeometryPipeline(RegsInternal& regs, GeometryShaderUnit& gs_unit, ShaderSetup& gs);
    ~GeometryPipeline();

    /// Sets the handler for receiving vertex outputs from vertex shader
    void SetVertexHandler(VertexHandler vertex_handler);

    /// Setup the geometry shader unit if it is in use
    void Setup(ShaderEngine* shader_engine);

    /// Reconfigures the pipeline according to current register settings
    void Reconfigure();

    /// Checks if the pipeline needs a direct input from index buffer
    bool NeedIndexInput() const;

    /// Submits an index from index buffer. Call this only when NeedIndexInput returns true
    void SubmitIndex(unsigned int val);

    /// Submits vertex attributes output from vertex shader
    void SubmitVertex(const AttributeBuffer& input);

private:
    VertexHandler vertex_handler;
    ShaderEngine* shader_engine;
    std::unique_ptr<GeometryPipelineBackend> backend;
    RegsInternal& regs;
    GeometryShaderUnit& gs_unit;
    ShaderSetup& gs;
};
} // namespace Pica
