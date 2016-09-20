// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"

namespace Pica {
namespace Shader {
struct OutputVertex;
}
}

namespace VideoCore {

class SWRasterizer : public RasterizerInterface {
    void AddTriangle(const Pica::Shader::OutputVertex& v0, const Pica::Shader::OutputVertex& v1,
                     const Pica::Shader::OutputVertex& v2) override;
    void DrawTriangles() override {}
    void NotifyPicaRegisterChanged(u32 id) override {}
    void FlushAll() override {}
    void FlushRegion(PAddr addr, u32 size) override {}
    void FlushAndInvalidateRegion(PAddr addr, u32 size) override {}
};
}
