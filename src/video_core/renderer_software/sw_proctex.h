// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"

namespace Pica::Rasterizer {

/// Generates procedural texture color for the given coordinates
Common::Vec4<u8> ProcTex(float u, float v, const TexturingRegs& regs, const State::ProcTex& state);

} // namespace Pica::Rasterizer
