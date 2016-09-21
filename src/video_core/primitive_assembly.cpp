// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "video_core/pica.h"
#include "video_core/primitive_assembly.h"
#include "video_core/shader/shader.h"

namespace Pica {

template <typename VertexType>
PrimitiveAssembler<VertexType>::PrimitiveAssembler(Regs::TriangleTopology topology)
    : topology(topology), buffer_index(0) {}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::SubmitVertex(VertexType& vtx,
                                                  TriangleHandler triangle_handler) {
    switch (topology) {
    // TODO: Figure out what's different with TriangleTopology::Shader.
    case Regs::TriangleTopology::List:
    case Regs::TriangleTopology::Shader:
        if (buffer_index < 2) {
            buffer[buffer_index++] = vtx;
        } else {
            buffer_index = 0;

            triangle_handler(buffer[0], buffer[1], vtx);
        }
        break;

    case Regs::TriangleTopology::Strip:
    case Regs::TriangleTopology::Fan:
        if (strip_ready)
            triangle_handler(buffer[0], buffer[1], vtx);

        buffer[buffer_index] = vtx;

        strip_ready |= (buffer_index == 1);

        if (topology == Regs::TriangleTopology::Strip)
            buffer_index = !buffer_index;
        else if (topology == Regs::TriangleTopology::Fan)
            buffer_index = 1;
        break;

    default:
        LOG_ERROR(HW_GPU, "Unknown triangle topology %x:", (int)topology);
        break;
    }
}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::Reset() {
    buffer_index = 0;
    strip_ready = false;
}

template <typename VertexType>
void PrimitiveAssembler<VertexType>::Reconfigure(Regs::TriangleTopology topology) {
    Reset();
    this->topology = topology;
}

// explicitly instantiate use cases
template struct PrimitiveAssembler<Shader::OutputVertex>;

} // namespace
