// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/settings.h"

namespace GameSettings {

void LoadOverrides(u64 program_id) {
    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_8ms;
    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_2ms;
    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Synch;
    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Asynch;
    Settings::values.gpu_timing_mode_invalidate = Settings::GpuTimingMode::Synch;

    switch (program_id) {
        // JAP / Dragon Quest VII: Fragments of the Forgotten Past
    case 0x0004000000065E00:
        // USA / Dragon Quest VII: Fragments of the Forgotten Past
    case 0x000400000018EF00:
        // EUR / Dragon Quest VII: Fragments of the Forgotten Past
    case 0x000400000018F000:
        // This game is currently broken with asynchronous GPU
        Settings::values.use_asynchronous_gpu_emulation = false;
        break;

        // JAP / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033400:
        // USA / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033500:
        // EUR / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033600:
        // KOR / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F800:
        // CHI / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F900:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        break;

        // JAP / Super Mario 3D Land
    case 0x0004000000054100:
        // USA / Super Mario 3D Land
    case 0x0004000000054000:
        // EUR / Super Mario 3D Land
    case 0x0004000000053F00:
        // KOR / Super Mario 3D Land
    case 0x0004000000089D00:
        // This game has very sensitive timings with asynchronous GPU
        Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Synch;
        break;

        // USA / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x00040000001B8F00:
        // EUR / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x00040000001B9000:
        // JAP / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x0004000000194B00:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        break;

        // USA / Mario & Luigi: Bowsers Inside Story + Bowser Jrs Journey
    case 0x00040000001D1400:
        // EUR / Mario & Luigi: Bowsers Inside Story + Bowser Jrs Journey
    case 0x00040000001D1500:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        break;
    }
}

} // namespace GameSettings
