// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/regs_texturing.h"

namespace Pica {
namespace Rasterizer {

int GetWrappedTexCoord(TexturingRegs::TextureConfig::WrapMode mode, int val, unsigned size);

Math::Vec3<u8> GetColorModifier(TexturingRegs::TevStageConfig::ColorModifier factor,
                                const Math::Vec4<u8>& values);

u8 GetAlphaModifier(TexturingRegs::TevStageConfig::AlphaModifier factor,
                    const Math::Vec4<u8>& values);

Math::Vec3<u8> ColorCombine(TexturingRegs::TevStageConfig::Operation op,
                            const Math::Vec3<u8> input[3]);

u8 AlphaCombine(TexturingRegs::TevStageConfig::Operation op, const std::array<u8, 3>& input);

} // namespace Rasterizer
} // namespace Pica
