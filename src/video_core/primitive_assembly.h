// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include "video_core/pica.h"

namespace Pica {

/*
 * Utility class to build triangles from a series of vertices,
 * according to a given triangle topology.
 */
template <typename VertexType>
struct PrimitiveAssembler {
    using TriangleHandler = std::function<void(VertexType& v0, VertexType& v1, VertexType& v2)>;

    PrimitiveAssembler(Regs::TriangleTopology topology = Regs::TriangleTopology::List);

    /*
     * Queues a vertex, builds primitives from the vertex queue according to the given
     * triangle topology, and calls triangle_handler for each generated primitive.
     * NOTE: We could specify the triangle handler in the constructor, but this way we can
     * keep event and handler code next to each other.
     */
    void SubmitVertex(VertexType& vtx, TriangleHandler triangle_handler);

    /**
     * Resets the internal state of the PrimitiveAssembler.
     */
    void Reset();

    /**
     * Reconfigures the PrimitiveAssembler to use a different triangle topology.
     */
    void Reconfigure(Regs::TriangleTopology topology);

private:
    Regs::TriangleTopology topology;

    int buffer_index;
    VertexType buffer[2];
    bool strip_ready = false;
};

} // namespace
