// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace Shader {
struct OutputVertex;
}

namespace Rasterizer {

void ProcessTriangle(const Shader::OutputVertex& v0, const Shader::OutputVertex& v1,
                     const Shader::OutputVertex& v2);

} // namespace Rasterizer

} // namespace Pica
