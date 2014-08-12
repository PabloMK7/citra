// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace VertexShader {
    struct OutputVertex;
}

namespace Rasterizer {

void ProcessTriangle(const VertexShader::OutputVertex& v0,
                     const VertexShader::OutputVertex& v1,
                     const VertexShader::OutputVertex& v2);

} // namespace Rasterizer

} // namespace Pica
