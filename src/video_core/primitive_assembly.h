// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace VertexShader {
    struct OutputVertex;
}

namespace PrimitiveAssembly {

using VertexShader::OutputVertex;

void SubmitVertex(OutputVertex& vtx);

} // namespace

} // namespace
