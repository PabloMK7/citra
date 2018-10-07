// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <SoundTouch.h>
#include "audio_core/audio_types.h"
#include "audio_core/time_stretch.h"
#include "common/logging/log.h"

namespace AudioCore {

TimeStretcher::TimeStretcher()
    : sample_rate(native_sample_rate), sound_touch(std::make_unique<soundtouch::SoundTouch>()) {
    sound_touch->setChannels(2);
    sound_touch->setSampleRate(native_sample_rate);
    sound_touch->setPitch(1.0);
    sound_touch->setTempo(1.0);
}

TimeStretcher::~TimeStretcher() = default;

void TimeStretcher::SetOutputSampleRate(unsigned int sample_rate) {
    sound_touch->setSampleRate(sample_rate);
    sample_rate = native_sample_rate;
}

std::size_t TimeStretcher::Process(const s16* in, std::size_t num_in, s16* out,
                                   std::size_t num_out) {
    const double time_delta = static_cast<double>(num_out) / sample_rate; // seconds
    double current_ratio = static_cast<double>(num_in) / static_cast<double>(num_out);

    const double max_latency = 0.25; // seconds
    const double max_backlog = sample_rate * max_latency;
    const double backlog_fullness = sound_touch->numSamples() / max_backlog;
    if (backlog_fullness > 4.0) {
        // Too many samples in backlog: Don't push anymore on
        num_in = 0;
    }

    // We ideally want the backlog to be about 50% full.
    // This gives some headroom both ways to prevent underflow and overflow.
    // We tweak current_ratio to encourage this.
    constexpr double tweak_time_scale = 0.050; // seconds
    const double tweak_correction = (backlog_fullness - 0.5) * (time_delta / tweak_time_scale);
    current_ratio *= std::pow(1.0 + 2.0 * tweak_correction, tweak_correction < 0 ? 3.0 : 1.0);

    // This low-pass filter smoothes out variance in the calculated stretch ratio.
    // The time-scale determines how responsive this filter is.
    constexpr double lpf_time_scale = 0.712; // seconds
    const double lpf_gain = 1.0 - std::exp(-time_delta / lpf_time_scale);
    stretch_ratio += lpf_gain * (current_ratio - stretch_ratio);

    // Place a lower limit of 5% speed.  When a game boots up, there will be
    // many silence samples.  These do not need to be timestretched.
    stretch_ratio = std::max(stretch_ratio, 0.05);
    sound_touch->setTempo(stretch_ratio);

    LOG_TRACE(Audio, "{:5}/{:5} ratio:{:0.6f} backlog:{:0.6f}", num_in, num_out, stretch_ratio,
              backlog_fullness);

    sound_touch->putSamples(in, static_cast<u32>(num_in));
    return sound_touch->receiveSamples(out, static_cast<u32>(num_out));
}

void TimeStretcher::Clear() {
    sound_touch->clear();
}

void TimeStretcher::Flush() {
    sound_touch->flush();
}

} // namespace AudioCore
