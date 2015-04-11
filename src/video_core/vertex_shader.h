// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>

#include <common/common_types.h>

#include "math.h"
#include "pica.h"

namespace Pica {

namespace VertexShader {

struct InputVertex {
    Math::Vec4<float24> attr[16];
};

struct OutputVertex {
    OutputVertex() = default;

    // VS output attributes
    Math::Vec4<float24> pos;
    Math::Vec4<float24> dummy; // quaternions (not implemented, yet)
    Math::Vec4<float24> color;
    Math::Vec2<float24> tc0;
    Math::Vec2<float24> tc1;
    float24 pad[6];
    Math::Vec2<float24> tc2;

    // Padding for optimal alignment
    float24 pad2[4];

    // Attributes used to store intermediate results

    // position after perspective divide
    Math::Vec3<float24> screenpos;
    float24 pad3;

    // Linear interpolation
    // factor: 0=this, 1=vtx
    void Lerp(float24 factor, const OutputVertex& vtx) {
        pos = pos * factor + vtx.pos * (float24::FromFloat32(1) - factor);

        // TODO: Should perform perspective correct interpolation here...
        tc0 = tc0 * factor + vtx.tc0 * (float24::FromFloat32(1) - factor);
        tc1 = tc1 * factor + vtx.tc1 * (float24::FromFloat32(1) - factor);
        tc2 = tc2 * factor + vtx.tc2 * (float24::FromFloat32(1) - factor);

        screenpos = screenpos * factor + vtx.screenpos * (float24::FromFloat32(1) - factor);

        color = color * factor + vtx.color * (float24::FromFloat32(1) - factor);
    }

    // Linear interpolation
    // factor: 0=v0, 1=v1
    static OutputVertex Lerp(float24 factor, const OutputVertex& v0, const OutputVertex& v1) {
        OutputVertex ret = v0;
        ret.Lerp(factor, v1);
        return ret;
    }
};
static_assert(std::is_pod<OutputVertex>::value, "Structure is not POD");
static_assert(sizeof(OutputVertex) == 32 * sizeof(float), "OutputVertex has invalid size");

void SubmitShaderMemoryChange(u32 addr, u32 value);
void SubmitSwizzleDataChange(u32 addr, u32 value);

OutputVertex RunShader(const InputVertex& input, int num_attributes);

Math::Vec4<float24>& GetFloatUniform(u32 index);
bool& GetBoolUniform(u32 index);
Math::Vec4<u8>& GetIntUniform(u32 index);
Math::Vec4<float24>& GetDefaultAttribute(u32 index);

const std::array<u32, 1024>& GetShaderBinary();
const std::array<u32, 1024>& GetSwizzlePatterns();

} // namespace

} // namespace

