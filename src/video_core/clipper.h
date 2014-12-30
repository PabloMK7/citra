// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace VertexShader {
    struct OutputVertex;
}

namespace Clipper {

using VertexShader::OutputVertex;

void ProcessTriangle(OutputVertex& v0, OutputVertex& v1, OutputVertex& v2);

} // namespace

} // namespace
