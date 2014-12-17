// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "pica.h"
#include "primitive_assembly.h"
#include "vertex_shader.h"

#include "video_core/debug_utils/debug_utils.h"

namespace Pica {

template<typename VertexType>
PrimitiveAssembler<VertexType>::PrimitiveAssembler(Regs::TriangleTopology topology)
    : topology(topology), buffer_index(0) {
}

template<typename VertexType>
void PrimitiveAssembler<VertexType>::SubmitVertex(VertexType& vtx, TriangleHandler triangle_handler)
{
    switch (topology) {
        case Regs::TriangleTopology::List:
        case Regs::TriangleTopology::ListIndexed:
            if (buffer_index < 2) {
                buffer[buffer_index++] = vtx;
            } else {
                buffer_index = 0;

                triangle_handler(buffer[0], buffer[1], vtx);
            }
            break;

        case Regs::TriangleTopology::Fan:
            if (buffer_index == 2) {
                buffer_index = 0;

                triangle_handler(buffer[0], buffer[1], vtx);

                buffer[1] = vtx;
            } else {
                buffer[buffer_index++] = vtx;
            }
            break;

        default:
            LOG_ERROR(Render_Software, "Unknown triangle topology %x:", (int)topology);
            break;
    }
}

// explicitly instantiate use cases
template
struct PrimitiveAssembler<VertexShader::OutputVertex>;
template
struct PrimitiveAssembler<DebugUtils::GeometryDumper::Vertex>;

} // namespace
