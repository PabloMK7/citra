// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/shader/shader.h"

namespace Pica {

namespace Rasterizer {

struct Vertex : Shader::OutputVertex {
    Vertex(const OutputVertex& v) : OutputVertex(v) {}

    // Attributes used to store intermediate results
    // position after perspective divide
    Math::Vec3<float24> screenpos;

    // Linear interpolation
    // factor: 0=this, 1=vtx
    // Note: This function cannot be called after perspective divide
    void Lerp(float24 factor, const Vertex& vtx) {
        pos = pos * factor + vtx.pos * (float24::FromFloat32(1) - factor);
        quat = quat * factor + vtx.quat * (float24::FromFloat32(1) - factor);
        color = color * factor + vtx.color * (float24::FromFloat32(1) - factor);
        tc0 = tc0 * factor + vtx.tc0 * (float24::FromFloat32(1) - factor);
        tc1 = tc1 * factor + vtx.tc1 * (float24::FromFloat32(1) - factor);
        tc0_w = tc0_w * factor + vtx.tc0_w * (float24::FromFloat32(1) - factor);
        view = view * factor + vtx.view * (float24::FromFloat32(1) - factor);
        tc2 = tc2 * factor + vtx.tc2 * (float24::FromFloat32(1) - factor);
    }

    // Linear interpolation
    // factor: 0=v0, 1=v1
    // Note: This function cannot be called after perspective divide
    static Vertex Lerp(float24 factor, const Vertex& v0, const Vertex& v1) {
        Vertex ret = v0;
        ret.Lerp(factor, v1);
        return ret;
    }
};

void ProcessTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

} // namespace Rasterizer

} // namespace Pica
