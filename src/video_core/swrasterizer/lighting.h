// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include "common/quaternion.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"

namespace Pica {

std::tuple<Math::Vec4<u8>, Math::Vec4<u8>> ComputeFragmentsColors(
    const Pica::LightingRegs& lighting, const Pica::State::Lighting& lighting_state,
    const Math::Quaternion<float>& normquat, const Math::Vec3<float>& view,
    const Math::Vec4<u8> (&texture_color)[4]);

} // namespace Pica
