// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "video_core/primitive_assembly.h"
#include "video_core/regs_pipeline.h"
#include "video_core/shader/shader.h"

namespace Pica {

template <typename VertexType>
PrimitiveAssembler<VertexType>::PrimitiveAssembler(PipelineRegs::TriangleTopology topology)
    : topology(topology) {}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::SubmitVertex(const VertexType& vtx,
                                                  const TriangleHandler& triangle_handler) {
    switch (topology) {
    case PipelineRegs::TriangleTopology::List:
    case PipelineRegs::TriangleTopology::Shader:
        if (buffer_index < 2) {
            buffer[buffer_index++] = vtx;
        } else {
            buffer_index = 0;
            if (topology == PipelineRegs::TriangleTopology::Shader && winding) {
                triangle_handler(buffer[1], buffer[0], vtx);
                winding = false;
            } else {
                triangle_handler(buffer[0], buffer[1], vtx);
            }
        }
        break;

    case PipelineRegs::TriangleTopology::Strip:
    case PipelineRegs::TriangleTopology::Fan:
        if (strip_ready)
            triangle_handler(buffer[0], buffer[1], vtx);

        buffer[buffer_index] = vtx;

        strip_ready |= (buffer_index == 1);

        if (topology == PipelineRegs::TriangleTopology::Strip)
            buffer_index = !buffer_index;
        else if (topology == PipelineRegs::TriangleTopology::Fan)
            buffer_index = 1;
        break;

    default:
        LOG_ERROR(HW_GPU, "Unknown triangle topology {:x}:", (int)topology);
        break;
    }
}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::SetWinding() {
    winding = true;
}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::Reset() {
    buffer_index = 0;
    strip_ready = false;
    winding = false;
}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::Reconfigure(PipelineRegs::TriangleTopology topology) {
    Reset();
    this->topology = topology;
}

template <typename VertexType>
bool PrimitiveAssembler<VertexType>::IsEmpty() const {
    return buffer_index == 0 && strip_ready == false;
}

template <typename VertexType>
PipelineRegs::TriangleTopology PrimitiveAssembler<VertexType>::GetTopology() const {
    return topology;
}

// explicitly instantiate use cases
template struct PrimitiveAssembler<Shader::OutputVertex>;

} // namespace Pica
