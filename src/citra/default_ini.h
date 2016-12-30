// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace DefaultINI {

const char* sdl2_config_file = R"(
[Controls]
pad_start =
pad_select =
pad_home =
pad_dup =
pad_ddown =
pad_dleft =
pad_dright =
pad_a =
pad_b =
pad_x =
pad_y =
pad_l =
pad_r =
pad_zl =
pad_zr =
pad_cup =
pad_cdown =
pad_cleft =
pad_cright =
pad_circle_up =
pad_circle_down =
pad_circle_left =
pad_circle_right =
pad_circle_modifier =

# The applied modifier scale to circle pad.
# Must be in range of 0.0-1.0. Defaults to 0.5
pad_circle_modifier_scale =

[Core]
# Whether to use the Just-In-Time (JIT) compiler for CPU emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
use_cpu_jit =

[Renderer]
# Whether to use software or hardware rendering.
# 0: Software, 1 (default): Hardware
use_hw_renderer =

# Whether to use the Just-In-Time (JIT) compiler for shader emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
use_shader_jit =

# Whether to use native 3DS screen resolution or to scale rendering resolution to the displayed screen size.
# 0 (default): Native, 1: Scaled
use_scaled_resolution =

# Whether to enable V-Sync (caps the framerate at 60FPS) or not.
# 0 (default): Off, 1: On
use_vsync =

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0.0-1.0. Defaults to 1.0 for all.
bg_red =
bg_blue =
bg_green =

[Layout]
# Layout for the screen inside the render window.
# 0 (default): Default Top Bottom Screen, 1: Single Screen Only, 2: Large Screen Small Screen
layout_option =

#Whether to toggle frame limiter on or off.
# 0: Off , 1  (default): On
toggle_framelimit =

# Swaps the prominent screen with the other screen.
# For example, if Single Screen is chosen, setting this to 1 will display the bottom screen instead of the top screen.
# 0 (default): Top Screen is prominent, 1: Bottom Screen is prominent
swap_screen =

[Audio]
# Which audio output engine to use.
# auto (default): Auto-select, null: No audio output, sdl2: SDL2 (if available)
output_engine =

# Whether or not to enable the audio-stretching post-processing effect.
# This effect adjusts audio speed to match emulation speed and helps prevent audio stutter,
# at the cost of increasing audio latency.
# 0: No, 1 (default): Yes
enable_audio_stretching =

[Data Storage]
# Whether to create a virtual SD card.
# 1 (default): Yes, 0: No
use_virtual_sd =

[System]
# The system model that Citra will try to emulate
# 0: Old 3DS (default), 1: New 3DS
is_new_3ds =

# The system region that Citra will use during emulation
# 0: Japan, 1: USA (default), 2: Europe, 3: Australia, 4: China, 5: Korea, 6: Taiwan
region_value =

[Miscellaneous]
# A filter which removes logs below a certain logging level.
# Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
log_filter = *:Info

[Debugging]
# Port for listening to GDB connections.
use_gdbstub=false
gdbstub_port=24689
)";
}
