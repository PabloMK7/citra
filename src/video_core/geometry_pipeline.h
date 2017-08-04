// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "video_core/shader/shader.h"

namespace Pica {

struct State;

class GeometryPipelineBackend;

/// A pipeline receiving from vertex shader and sending to geometry shader and primitive assembler
class GeometryPipeline {
public:
    explicit GeometryPipeline(State& state);
    ~GeometryPipeline();

    /// Sets the handler for receiving vertex outputs from vertex shader
    void SetVertexHandler(Shader::VertexHandler vertex_handler);

    /**
     * Setup the geometry shader unit if it is in use
     * @param shader_engine the shader engine for the geometry shader to run
     */
    void Setup(Shader::ShaderEngine* shader_engine);

    /// Reconfigures the pipeline according to current register settings
    void Reconfigure();

    /// Checks if the pipeline needs a direct input from index buffer
    bool NeedIndexInput() const;

    /// Submits an index from index buffer. Call this only when NeedIndexInput returns true
    void SubmitIndex(unsigned int val);

    /// Submits vertex attributes output from vertex shader
    void SubmitVertex(const Shader::AttributeBuffer& input);

private:
    Shader::VertexHandler vertex_handler;
    Shader::ShaderEngine* shader_engine;
    std::unique_ptr<GeometryPipelineBackend> backend;
    State& state;
};
} // namespace Pica
