// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include "common/common_types.h"

namespace Core {

class PerfStats {
public:
    using Clock = std::chrono::high_resolution_clock;

    struct Results {
        /// System FPS (LCD VBlanks) in Hz
        double system_fps;
        /// Game FPS (GSP frame submissions) in Hz
        double game_fps;
        /// Walltime per system frame, in seconds, excluding any waits
        double frametime;
        /// Ratio of walltime / emulated time elapsed
        double emulation_speed;
    };

    void BeginSystemFrame();
    void EndSystemFrame();
    void EndGameFrame();

    Results GetAndResetStats(u64 current_system_time_us);

private:
    Clock::time_point reset_point = Clock::now();

    Clock::time_point frame_begin;
    Clock::duration accumulated_frametime = Clock::duration::zero();
    u64 reset_point_system_us = 0;
    u32 system_frames = 0;
    u32 game_frames = 0;
};

} // namespace Core
