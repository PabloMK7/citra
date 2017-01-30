// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/regs_framebuffer.h"

namespace Pica {
namespace Rasterizer {

void DrawPixel(int x, int y, const Math::Vec4<u8>& color);
const Math::Vec4<u8> GetPixel(int x, int y);
u32 GetDepth(int x, int y);
u8 GetStencil(int x, int y);
void SetDepth(int x, int y, u32 value);
void SetStencil(int x, int y, u8 value);
u8 PerformStencilAction(FramebufferRegs::StencilAction action, u8 old_stencil, u8 ref);

} // namespace Rasterizer
} // namespace Pica
