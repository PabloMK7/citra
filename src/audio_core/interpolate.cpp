// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/interpolate.h"
#include "common/assert.h"
#include "common/math_util.h"

namespace AudioInterp {

// Calculations are done in fixed point with 24 fractional bits.
// (This is not verified. This was chosen for minimal error.)
constexpr u64 scale_factor = 1 << 24;
constexpr u64 scale_mask = scale_factor - 1;

/// Here we step over the input in steps of rate_multiplier, until we consume all of the input.
/// Three adjacent samples are passed to fn each step.
template <typename Function>
static StereoBuffer16 StepOverSamples(State& state, const StereoBuffer16& input,
                                      float rate_multiplier, Function fn) {
    ASSERT(rate_multiplier > 0);

    if (input.size() < 2)
        return {};

    StereoBuffer16 output;
    output.reserve(static_cast<size_t>(input.size() / rate_multiplier));

    u64 step_size = static_cast<u64>(rate_multiplier * scale_factor);

    u64 fposition = 0;
    const u64 max_fposition = input.size() * scale_factor;

    while (fposition < 1 * scale_factor) {
        u64 fraction = fposition & scale_mask;

        output.push_back(fn(fraction, state.xn2, state.xn1, input[0]));

        fposition += step_size;
    }

    while (fposition < 2 * scale_factor) {
        u64 fraction = fposition & scale_mask;

        output.push_back(fn(fraction, state.xn1, input[0], input[1]));

        fposition += step_size;
    }

    while (fposition < max_fposition) {
        u64 fraction = fposition & scale_mask;

        size_t index = static_cast<size_t>(fposition / scale_factor);
        output.push_back(fn(fraction, input[index - 2], input[index - 1], input[index]));

        fposition += step_size;
    }

    state.xn2 = input[input.size() - 2];
    state.xn1 = input[input.size() - 1];

    return output;
}

StereoBuffer16 None(State& state, const StereoBuffer16& input, float rate_multiplier) {
    return StepOverSamples(
        state, input, rate_multiplier,
        [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) { return x0; });
}

StereoBuffer16 Linear(State& state, const StereoBuffer16& input, float rate_multiplier) {
    // Note on accuracy: Some values that this produces are +/- 1 from the actual firmware.
    return StepOverSamples(state, input, rate_multiplier,
                           [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) {
                               // This is a saturated subtraction. (Verified by black-box fuzzing.)
                               s64 delta0 = MathUtil::Clamp<s64>(x1[0] - x0[0], -32768, 32767);
                               s64 delta1 = MathUtil::Clamp<s64>(x1[1] - x0[1], -32768, 32767);

                               return std::array<s16, 2>{
                                   static_cast<s16>(x0[0] + fraction * delta0 / scale_factor),
                                   static_cast<s16>(x0[1] + fraction * delta1 / scale_factor),
                               };
                           });
}

} // namespace AudioInterp
