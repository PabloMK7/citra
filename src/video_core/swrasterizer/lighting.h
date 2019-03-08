// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include "common/quaternion.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"

namespace Pica {

std::tuple<Common::Vec4<u8>, Common::Vec4<u8>> ComputeFragmentsColors(
    const Pica::LightingRegs& lighting, const Pica::State::Lighting& lighting_state,
    const Common::Quaternion<float>& normquat, const Common::Vec3<float>& view,
    const Common::Vec4<u8> (&texture_color)[4]);

} // namespace Pica
