// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"

namespace Pica {
namespace Rasterizer {

/// Generates procedural texture color for the given coordinates
Math::Vec4<u8> ProcTex(float u, float v, TexturingRegs regs, State::ProcTex state);

} // namespace Rasterizer
} // namespace Pica
