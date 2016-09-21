// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/clipper.h"
#include "video_core/swrasterizer.h"

namespace VideoCore {

void SWRasterizer::AddTriangle(const Pica::Shader::OutputVertex& v0,
                               const Pica::Shader::OutputVertex& v1,
                               const Pica::Shader::OutputVertex& v2) {
    Pica::Clipper::ProcessTriangle(v0, v1, v2);
}
}
