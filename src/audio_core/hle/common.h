// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>

#include "audio_core/audio_core.h"

#include "common/common_types.h"

namespace DSP {
namespace HLE {

/// The final output to the speakers is stereo. Preprocessing output in Source is also stereo.
using StereoFrame16 = std::array<std::array<s16, 2>, AudioCore::samples_per_frame>;

/// The DSP is quadraphonic internally.
using QuadFrame32   = std::array<std::array<s32, 4>, AudioCore::samples_per_frame>;

/**
 * This performs the filter operation defined by FilterT::ProcessSample on the frame in-place.
 * FilterT::ProcessSample is called sequentially on the samples.
 */
template<typename FrameT, typename FilterT>
void FilterFrame(FrameT& frame, FilterT& filter) {
    std::transform(frame.begin(), frame.end(), frame.begin(), [&filter](const typename FrameT::value_type& sample) {
        return filter.ProcessSample(sample);
    });
}

} // namespace HLE
} // namespace DSP
