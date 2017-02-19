// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include "core/hw/gpu.h"
#include "core/perf_stats.h"

namespace Core {

void PerfStats::BeginSystemFrame() {
    frame_begin = Clock::now();
}

void PerfStats::EndSystemFrame() {
    auto frame_end = Clock::now();
    accumulated_frametime += frame_end - frame_begin;
    system_frames += 1;
}

void PerfStats::EndGameFrame() {
    game_frames += 1;
}

PerfStats::Results PerfStats::GetAndResetStats(u64 current_system_time_us) {
    using DoubleSecs = std::chrono::duration<double, std::chrono::seconds::period>;
    using std::chrono::duration_cast;

    auto now = Clock::now();
    // Walltime elapsed since stats were reset
    auto interval = duration_cast<DoubleSecs>(now - reset_point).count();

    auto system_us_per_second =
        static_cast<double>(current_system_time_us - reset_point_system_us) / interval;

    Results results{};
    results.system_fps = static_cast<double>(system_frames) / interval;
    results.game_fps = static_cast<double>(game_frames) / interval;
    results.frametime = duration_cast<DoubleSecs>(accumulated_frametime).count() /
                        static_cast<double>(system_frames);
    results.emulation_speed = system_us_per_second / 1'000'000.0;

    // Reset counters
    reset_point = now;
    reset_point_system_us = current_system_time_us;
    accumulated_frametime = Clock::duration::zero();
    system_frames = 0;
    game_frames = 0;

    return results;
}

} // namespace Core
