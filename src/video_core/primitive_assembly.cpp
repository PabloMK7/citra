// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "pica.h"
#include "primitive_assembly.h"
#include "vertex_shader.h"

namespace Pica {

namespace PrimitiveAssembly {

static OutputVertex buffer[2];
static int buffer_index = 0; // TODO: reset this on emulation restart

void SubmitVertex(OutputVertex& vtx)
{
    switch (registers.triangle_topology) {
        case Regs::TriangleTopology::List:
        case Regs::TriangleTopology::ListIndexed:
            if (buffer_index < 2) {
                buffer[buffer_index++] = vtx;
            } else {
                buffer_index = 0;

                // TODO
                // Clipper::ProcessTriangle(buffer[0], buffer[1], vtx);
            }
            break;

        case Regs::TriangleTopology::Fan:
            if (buffer_index == 2) {
                buffer_index = 0;

                // TODO
                // Clipper::ProcessTriangle(buffer[0], buffer[1], vtx);

                buffer[1] = vtx;
            } else {
                buffer[buffer_index++] = vtx;
            }
            break;

        default:
            ERROR_LOG(GPU, "Unknown triangle mode %x:", (int)registers.triangle_topology.Value());
            break;
    }
}

} // namespace

} // namespace
