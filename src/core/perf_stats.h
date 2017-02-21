// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <mutex>
#include "common/common_types.h"

namespace Core {

/**
 * Class to manage and query performance/timing statistics. All public functions of this class are
 * thread-safe unless stated otherwise.
 */
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

    /**
     * Gets the ratio between walltime and the emulated time of the previous system frame. This is
     * useful for scaling inputs or outputs moving between the two time domains.
     */
    double GetLastFrameTimeScale();

private:
    std::mutex object_mutex;

    Clock::time_point reset_point = Clock::now();

    Clock::time_point frame_begin = reset_point;
    Clock::time_point previous_frame_end = reset_point;
    Clock::duration accumulated_frametime = Clock::duration::zero();
    Clock::duration previous_frame_length = Clock::duration::zero();
    u64 reset_point_system_us = 0;
    u32 system_frames = 0;
    u32 game_frames = 0;
};

class FrameLimiter {
public:
    using Clock = std::chrono::high_resolution_clock;

    void DoFrameLimiting(u64 current_system_time_us);

private:
    /// Emulated system time (in microseconds) at the last limiter invocation
    u64 previous_system_time_us = 0;
    /// Walltime at the last limiter invocation
    Clock::time_point previous_walltime = Clock::now();

    /// Accumulated difference between walltime and emulated time
    std::chrono::microseconds frame_limiting_delta_err{0};
};

} // namespace Core
