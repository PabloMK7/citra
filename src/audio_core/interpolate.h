// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/common_types.h"

namespace AudioInterp {

/// A variable length buffer of signed PCM16 stereo samples.
using StereoBuffer16 = std::vector<std::array<s16, 2>>;

struct State {
    // Two historical samples.
    std::array<s16, 2> xn1 = {}; ///< x[n-1]
    std::array<s16, 2> xn2 = {}; ///< x[n-2]
};

/**
 * No interpolation. This is equivalent to a zero-order hold. There is a two-sample predelay.
 * @param input Input buffer.
 * @param rate_multiplier Stretch factor. Must be a positive non-zero value.
 *                        rate_multiplier > 1.0 performs decimation and rate_multipler < 1.0
 *                        performs upsampling.
 * @return The resampled audio buffer.
 */
StereoBuffer16 None(State& state, const StereoBuffer16& input, float rate_multiplier);

/**
 * Linear interpolation. This is equivalent to a first-order hold. There is a two-sample predelay.
 * @param input Input buffer.
 * @param rate_multiplier Stretch factor. Must be a positive non-zero value.
 *                        rate_multiplier > 1.0 performs decimation and rate_multipler < 1.0
 *                        performs upsampling.
 * @return The resampled audio buffer.
 */
StereoBuffer16 Linear(State& state, const StereoBuffer16& input, float rate_multiplier);

} // namespace AudioInterp
