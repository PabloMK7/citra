// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "video_core/geometry_pipeline.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Pica {

State g_state;

void Init() {
    g_state.Reset();
}

void Shutdown() {
    Shader::Shutdown();
}

template <typename T>
void Zero(T& o) {
    memset(&o, 0, sizeof(o));
}

State::State() : geometry_pipeline(*this) {
    auto SubmitVertex = [this](const Shader::AttributeBuffer& vertex) {
        using Pica::Shader::OutputVertex;
        auto AddTriangle = [this](const OutputVertex& v0, const OutputVertex& v1,
                                  const OutputVertex& v2) {
            VideoCore::g_renderer->Rasterizer()->AddTriangle(v0, v1, v2);
        };
        primitive_assembler.SubmitVertex(
            Shader::OutputVertex::FromAttributeBuffer(regs.rasterizer, vertex), AddTriangle);
    };

    auto SetWinding = [this]() { primitive_assembler.SetWinding(); };

    g_state.gs_unit.SetVertexHandler(SubmitVertex, SetWinding);
    g_state.geometry_pipeline.SetVertexHandler(SubmitVertex);
}

void State::Reset() {
    Zero(regs);
    Zero(vs);
    Zero(gs);
    Zero(cmd_list);
    Zero(immediate);
    primitive_assembler.Reconfigure(PipelineRegs::TriangleTopology::List);
    vs_float_regs_counter = 0;
    Zero(vs_uniform_write_buffer);
    gs_float_regs_counter = 0;
    Zero(gs_uniform_write_buffer);
    default_attr_counter = 0;
    Zero(default_attr_write_buffer);
}
} // namespace Pica
