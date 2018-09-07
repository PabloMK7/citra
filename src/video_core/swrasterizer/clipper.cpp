// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <boost/container/static_vector.hpp>
#include <boost/container/vector.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/shader/shader.h"
#include "video_core/swrasterizer/clipper.h"
#include "video_core/swrasterizer/rasterizer.h"

using Pica::Rasterizer::Vertex;

namespace Pica {
namespace Clipper {

struct ClippingEdge {
public:
    ClippingEdge(Math::Vec4<float24> coeffs, Math::Vec4<float24> bias = Math::Vec4<float24>(
                                                 float24::FromFloat32(0), float24::FromFloat32(0),
                                                 float24::FromFloat32(0), float24::FromFloat32(0)))
        : coeffs(coeffs), bias(bias) {}

    bool IsInside(const Vertex& vertex) const {
        return Math::Dot(vertex.pos + bias, coeffs) >= float24::FromFloat32(0);
    }

    bool IsOutSide(const Vertex& vertex) const {
        return !IsInside(vertex);
    }

    Vertex GetIntersection(const Vertex& v0, const Vertex& v1) const {
        float24 dp = Math::Dot(v0.pos + bias, coeffs);
        float24 dp_prev = Math::Dot(v1.pos + bias, coeffs);
        float24 factor = dp_prev / (dp_prev - dp);

        return Vertex::Lerp(factor, v0, v1);
    }

private:
    float24 pos;
    Math::Vec4<float24> coeffs;
    Math::Vec4<float24> bias;
};

static void InitScreenCoordinates(Vertex& vtx) {
    struct {
        float24 halfsize_x;
        float24 offset_x;
        float24 halfsize_y;
        float24 offset_y;
        float24 zscale;
        float24 offset_z;
    } viewport;

    const auto& regs = g_state.regs;
    viewport.halfsize_x = float24::FromRaw(regs.rasterizer.viewport_size_x);
    viewport.halfsize_y = float24::FromRaw(regs.rasterizer.viewport_size_y);
    viewport.offset_x = float24::FromFloat32(static_cast<float>(regs.rasterizer.viewport_corner.x));
    viewport.offset_y = float24::FromFloat32(static_cast<float>(regs.rasterizer.viewport_corner.y));

    float24 inv_w = float24::FromFloat32(1.f) / vtx.pos.w;
    vtx.pos.w = inv_w;
    vtx.quat *= inv_w;
    vtx.color *= inv_w;
    vtx.tc0 *= inv_w;
    vtx.tc1 *= inv_w;
    vtx.tc0_w *= inv_w;
    vtx.view *= inv_w;
    vtx.tc2 *= inv_w;

    vtx.screenpos[0] =
        (vtx.pos.x * inv_w + float24::FromFloat32(1.0)) * viewport.halfsize_x + viewport.offset_x;
    vtx.screenpos[1] =
        (vtx.pos.y * inv_w + float24::FromFloat32(1.0)) * viewport.halfsize_y + viewport.offset_y;
    vtx.screenpos[2] = vtx.pos.z * inv_w;
}

void ProcessTriangle(const OutputVertex& v0, const OutputVertex& v1, const OutputVertex& v2) {
    using boost::container::static_vector;

    // Clipping a planar n-gon against a plane will remove at least 1 vertex and introduces 2 at
    // the new edge (or less in degenerate cases). As such, we can say that each clipping plane
    // introduces at most 1 new vertex to the polygon. Since we start with a triangle and have a
    // fixed 6 clipping planes, the maximum number of vertices of the clipped polygon is 3 + 6 = 9.
    static const std::size_t MAX_VERTICES = 9;
    static_vector<Vertex, MAX_VERTICES> buffer_a = {v0, v1, v2};
    static_vector<Vertex, MAX_VERTICES> buffer_b;

    auto FlipQuaternionIfOpposite = [](auto& a, const auto& b) {
        if (Math::Dot(a, b) < float24::Zero())
            a = a * float24::FromFloat32(-1.0f);
    };

    // Flip the quaternions if they are opposite to prevent interpolating them over the wrong
    // direction.
    FlipQuaternionIfOpposite(buffer_a[1].quat, buffer_a[0].quat);
    FlipQuaternionIfOpposite(buffer_a[2].quat, buffer_a[0].quat);

    auto* output_list = &buffer_a;
    auto* input_list = &buffer_b;

    // NOTE: We clip against a w=epsilon plane to guarantee that the output has a positive w value.
    // TODO: Not sure if this is a valid approach. Also should probably instead use the smallest
    //       epsilon possible within float24 accuracy.
    static const float24 EPSILON = float24::FromFloat32(0.00001f);
    static const float24 f0 = float24::FromFloat32(0.0);
    static const float24 f1 = float24::FromFloat32(1.0);
    static const std::array<ClippingEdge, 7> clipping_edges = {{
        {Math::MakeVec(-f1, f0, f0, f1)},                                          // x = +w
        {Math::MakeVec(f1, f0, f0, f1)},                                           // x = -w
        {Math::MakeVec(f0, -f1, f0, f1)},                                          // y = +w
        {Math::MakeVec(f0, f1, f0, f1)},                                           // y = -w
        {Math::MakeVec(f0, f0, -f1, f0)},                                          // z =  0
        {Math::MakeVec(f0, f0, f1, f1)},                                           // z = -w
        {Math::MakeVec(f0, f0, f0, f1), Math::Vec4<float24>(f0, f0, f0, EPSILON)}, // w = EPSILON
    }};

    // Simple implementation of the Sutherland-Hodgman clipping algorithm.
    // TODO: Make this less inefficient (currently lots of useless buffering overhead happens here)
    auto Clip = [&](const ClippingEdge& edge) {
        std::swap(input_list, output_list);
        output_list->clear();

        const Vertex* reference_vertex = &input_list->back();

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
    };

    for (auto edge : clipping_edges) {
        Clip(edge);

        // Need to have at least a full triangle to continue...
        if (output_list->size() < 3)
            return;
    }

    if (g_state.regs.rasterizer.clip_enable) {
        ClippingEdge custom_edge{g_state.regs.rasterizer.GetClipCoef()};
        Clip(custom_edge);

        if (output_list->size() < 3)
            return;
    }

    InitScreenCoordinates((*output_list)[0]);
    InitScreenCoordinates((*output_list)[1]);

    for (std::size_t i = 0; i < output_list->size() - 2; i++) {
        Vertex& vtx0 = (*output_list)[0];
        Vertex& vtx1 = (*output_list)[i + 1];
        Vertex& vtx2 = (*output_list)[i + 2];

        InitScreenCoordinates(vtx2);

        LOG_TRACE(
            Render_Software,
            "Triangle {}/{} at position ({:.3}, {:.3}, {:.3}, {:.3f}), "
            "({:.3}, {:.3}, {:.3}, {:.3}), ({:.3}, {:.3}, {:.3}, {:.3}) and "
            "screen position ({:.2}, {:.2}, {:.2}), ({:.2}, {:.2}, {:.2}), ({:.2}, {:.2}, {:.2})",
            i + 1, output_list->size() - 2, vtx0.pos.x.ToFloat32(), vtx0.pos.y.ToFloat32(),
            vtx0.pos.z.ToFloat32(), vtx0.pos.w.ToFloat32(), vtx1.pos.x.ToFloat32(),
            vtx1.pos.y.ToFloat32(), vtx1.pos.z.ToFloat32(), vtx1.pos.w.ToFloat32(),
            vtx2.pos.x.ToFloat32(), vtx2.pos.y.ToFloat32(), vtx2.pos.z.ToFloat32(),
            vtx2.pos.w.ToFloat32(), vtx0.screenpos.x.ToFloat32(), vtx0.screenpos.y.ToFloat32(),
            vtx0.screenpos.z.ToFloat32(), vtx1.screenpos.x.ToFloat32(),
            vtx1.screenpos.y.ToFloat32(), vtx1.screenpos.z.ToFloat32(),
            vtx2.screenpos.x.ToFloat32(), vtx2.screenpos.y.ToFloat32(),
            vtx2.screenpos.z.ToFloat32());

        Rasterizer::ProcessTriangle(vtx0, vtx1, vtx2);
    }
}

} // namespace Clipper
} // namespace Pica
