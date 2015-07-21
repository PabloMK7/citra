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

void ProcessTriangle(OutputVertex& v0, OutputVertex& v1, OutputVertex& v2);

} // namespace

} // namespace
