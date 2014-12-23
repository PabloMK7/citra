// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/static_vector.hpp>

#include "clipper.h"
#include "pica.h"
#include "rasterizer.h"
#include "vertex_shader.h"

namespace Pica {

namespace Clipper {

struct ClippingEdge {
public:
    enum Type {
        POS_X = 0,
        NEG_X = 1,
        POS_Y = 2,
        NEG_Y = 3,
        POS_Z = 4,
        NEG_Z = 5,
    };

    ClippingEdge(Type type, float24 position) : type(type), pos(position) {}

    bool IsInside(const OutputVertex& vertex) const {
        switch (type) {
        case POS_X: return vertex.pos.x <= pos * vertex.pos.w;
        case NEG_X: return vertex.pos.x >= pos * vertex.pos.w;
        case POS_Y: return vertex.pos.y <= pos * vertex.pos.w;
        case NEG_Y: return vertex.pos.y >= pos * vertex.pos.w;

        // TODO: Check z compares ... should be 0..1 instead?
        case POS_Z: return vertex.pos.z <= pos * vertex.pos.w;

        default:
        case NEG_Z: return vertex.pos.z >= pos * vertex.pos.w;
        }
    }

    bool IsOutSide(const OutputVertex& vertex) const {
        return !IsInside(vertex);
    }

    OutputVertex GetIntersection(const OutputVertex& v0, const OutputVertex& v1) const {
        auto dotpr = [this](const OutputVertex& vtx) {
            switch (type) {
            case POS_X: return vtx.pos.x - vtx.pos.w;
            case NEG_X: return -vtx.pos.x - vtx.pos.w;
            case POS_Y: return vtx.pos.y - vtx.pos.w;
            case NEG_Y: return -vtx.pos.y - vtx.pos.w;

            // TODO: Verify z clipping
            case POS_Z: return vtx.pos.z - vtx.pos.w;

            default:
            case NEG_Z: return -vtx.pos.w;
            }
        };

        float24 dp = dotpr(v0);
        float24 dp_prev = dotpr(v1);
        float24 factor = dp_prev / (dp_prev - dp);

        return OutputVertex::Lerp(factor, v0, v1);
    }

private:
    Type type;
    float24 pos;
};

static void InitScreenCoordinates(OutputVertex& vtx)
{
    struct {
        float24 halfsize_x;
        float24 offset_x;
        float24 halfsize_y;
        float24 offset_y;
        float24 zscale;
        float24 offset_z;
    } viewport;

    viewport.halfsize_x = float24::FromRawFloat24(registers.viewport_size_x);
    viewport.halfsize_y = float24::FromRawFloat24(registers.viewport_size_y);
    viewport.offset_x   = float24::FromFloat32(static_cast<float>(registers.viewport_corner.x));
    viewport.offset_y   = float24::FromFloat32(static_cast<float>(registers.viewport_corner.y));
    viewport.zscale     = float24::FromRawFloat24(registers.viewport_depth_range);
    viewport.offset_z   = float24::FromRawFloat24(registers.viewport_depth_far_plane);

    float24 inv_w = float24::FromFloat32(1.f) / vtx.pos.w;
    vtx.color *= inv_w;
    vtx.tc0 *= inv_w;
    vtx.tc1 *= inv_w;
    vtx.tc2 *= inv_w;
    vtx.pos.w = inv_w;

    // TODO: Not sure why the viewport width needs to be divided by 2 but the viewport height does not
    vtx.screenpos[0] = (vtx.pos.x * inv_w + float24::FromFloat32(1.0)) * viewport.halfsize_x + viewport.offset_x;
    vtx.screenpos[1] = (vtx.pos.y * inv_w + float24::FromFloat32(1.0)) * viewport.halfsize_y + viewport.offset_y;
    vtx.screenpos[2] = viewport.offset_z - vtx.pos.z * inv_w * viewport.zscale;
}

void ProcessTriangle(OutputVertex &v0, OutputVertex &v1, OutputVertex &v2) {
    using boost::container::static_vector;

    // Clipping a planar n-gon against a plane will remove at least 1 vertex and introduces 2 at
    // the new edge (or less in degenerate cases). As such, we can say that each clipping plane
    // introduces at most 1 new vertex to the polygon. Since we start with a triangle and have a
    // fixed 6 clipping planes, the maximum number of vertices of the clipped polygon is 3 + 6 = 9.
    static const size_t MAX_VERTICES = 9;
    static_vector<OutputVertex, MAX_VERTICES> buffer_a = { v0, v1, v2 };
    static_vector<OutputVertex, MAX_VERTICES> buffer_b;
    auto* output_list = &buffer_a;
    auto* input_list  = &buffer_b;

    // Simple implementation of the Sutherland-Hodgman clipping algorithm.
    // TODO: Make this less inefficient (currently lots of useless buffering overhead happens here)
    for (auto edge : { ClippingEdge(ClippingEdge::POS_X, float24::FromFloat32(+1.0)),
                       ClippingEdge(ClippingEdge::NEG_X, float24::FromFloat32(-1.0)),
                       ClippingEdge(ClippingEdge::POS_Y, float24::FromFloat32(+1.0)),
                       ClippingEdge(ClippingEdge::NEG_Y, float24::FromFloat32(-1.0)),
                       ClippingEdge(ClippingEdge::POS_Z, float24::FromFloat32(+1.0)),
                       ClippingEdge(ClippingEdge::NEG_Z, float24::FromFloat32(-1.0)) }) {

        std::swap(input_list, output_list);
        output_list->clear();

        const OutputVertex* reference_vertex = &input_list->back();

        for (const auto& vertex : *input_list) {
            // NOTE: This algorithm changes vertex order in some cases!
            if (edge.IsInside(vertex)) {
                if (edge.IsOutSide(*reference_vertex)) {
                    output_list->push_back(edge.GetIntersection(vertex, *reference_vertex));
                }

                output_list->push_back(vertex);
            } else if (edge.IsInside(*reference_vertex)) {
                output_list->push_back(edge.GetIntersection(vertex, *reference_vertex));
            }
            reference_vertex = &vertex;
        }

        // Need to have at least a full triangle to continue...
        if (output_list->size() < 3)
            return;
    }

    InitScreenCoordinates((*output_list)[0]);
    InitScreenCoordinates((*output_list)[1]);

    for (size_t i = 0; i < output_list->size() - 2; i ++) {
        OutputVertex& vtx0 = (*output_list)[0];
        OutputVertex& vtx1 = (*output_list)[i+1];
        OutputVertex& vtx2 = (*output_list)[i+2];

        InitScreenCoordinates(vtx2);

        LOG_TRACE(Render_Software,
                  "Triangle %lu/%lu at position (%.3f, %.3f, %.3f, %.3f), "
                  "(%.3f, %.3f, %.3f, %.3f), (%.3f, %.3f, %.3f, %.3f) and "
                  "screen position (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)",
                  i, output_list->size(),
                  vtx0.pos.x.ToFloat32(), vtx0.pos.y.ToFloat32(), vtx0.pos.z.ToFloat32(), vtx0.pos.w.ToFloat32(),
                  vtx1.pos.x.ToFloat32(), vtx1.pos.y.ToFloat32(), vtx1.pos.z.ToFloat32(), vtx1.pos.w.ToFloat32(),
                  vtx2.pos.x.ToFloat32(), vtx2.pos.y.ToFloat32(), vtx2.pos.z.ToFloat32(), vtx2.pos.w.ToFloat32(),
                  vtx0.screenpos.x.ToFloat32(), vtx0.screenpos.y.ToFloat32(), vtx0.screenpos.z.ToFloat32(),
                  vtx1.screenpos.x.ToFloat32(), vtx1.screenpos.y.ToFloat32(), vtx1.screenpos.z.ToFloat32(),
                  vtx2.screenpos.x.ToFloat32(), vtx2.screenpos.y.ToFloat32(), vtx2.screenpos.z.ToFloat32());

        Rasterizer::ProcessTriangle(vtx0, vtx1, vtx2);
    }
}


} // namespace

} // namespace
