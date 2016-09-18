// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace Shader {
struct OutputVertex;
}

namespace Clipper {

using Shader::OutputVertex;

void ProcessTriangle(const OutputVertex& v0, const OutputVertex& v1, const OutputVertex& v2);

} // namespace

} // namespace
