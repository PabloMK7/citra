// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include "common/common_types.h"

namespace DSP {
namespace HLE {

constexpr int num_sources = 24;
constexpr int samples_per_frame = 160; ///< Samples per audio frame at native sample rate

/// The final output to the speakers is stereo. Preprocessing output in Source is also stereo.
using StereoFrame16 = std::array<std::array<s16, 2>, samples_per_frame>;

/// The DSP is quadraphonic internally.
using QuadFrame32 = std::array<std::array<s32, 4>, samples_per_frame>;

/**
 * This performs the filter operation defined by FilterT::ProcessSample on the frame in-place.
 * FilterT::ProcessSample is called sequentially on the samples.
 */
template <typename FrameT, typename FilterT>
void FilterFrame(FrameT& frame, FilterT& filter) {
    std::transform(frame.begin(), frame.end(), frame.begin(),
                   [&filter](const auto& sample) { return filter.ProcessSample(sample); });
}

} // namespace HLE
} // namespace DSP
