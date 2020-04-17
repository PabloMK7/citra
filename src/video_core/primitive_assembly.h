// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <boost/serialization/access.hpp>
#include "video_core/regs_pipeline.h"

namespace Pica {

/*
 * Utility class to build triangles from a series of vertices,
 * according to a given triangle topology.
 */
template <typename VertexType>
struct PrimitiveAssembler {
    using TriangleHandler =
        std::function<void(const VertexType& v0, const VertexType& v1, const VertexType& v2)>;

    PrimitiveAssembler(
        PipelineRegs::TriangleTopology topology = PipelineRegs::TriangleTopology::List);

    /*
     * Queues a vertex, builds primitives from the vertex queue according to the given
     * triangle topology, and calls triangle_handler for each generated primitive.
     * NOTE: We could specify the triangle handler in the constructor, but this way we can
     * keep event and handler code next to each other.
     */
    void SubmitVertex(const VertexType& vtx, TriangleHandler triangle_handler);

    /**
     * Invert the vertex order of the next triangle. Called by geometry shader emitter.
     * This only takes effect for TriangleTopology::Shader.
     */
    void SetWinding();

    /**
     * Resets the internal state of the PrimitiveAssembler.
     */
    void Reset();

    /**
     * Reconfigures the PrimitiveAssembler to use a different triangle topology.
     */
    void Reconfigure(PipelineRegs::TriangleTopology topology);

    /**
     * Returns whether the PrimitiveAssembler has an empty internal buffer.
     */
    bool IsEmpty() const;

    /**
     * Returns the current topology.
     */
    PipelineRegs::TriangleTopology GetTopology() const;

private:
    PipelineRegs::TriangleTopology topology;

    int buffer_index;
    VertexType buffer[2];
    bool strip_ready = false;
    bool winding = false;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar& topology;
        ar& buffer_index;
        ar& buffer;
        ar& strip_ready;
        ar& winding;
    }
    friend class boost::serialization::access;
};

} // namespace Pica
