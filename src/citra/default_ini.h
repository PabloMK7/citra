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
# The applied frameskip amount. Must be a power of two.
# 0 (default): No frameskip, 1: x2 frameskip, 2: x4 frameskip, 3: x8 frameskip, etc.
frame_skip =

[Renderer]
# Whether to use software or hardware rendering.
# 0 (default): Software, 1: Hardware
use_hw_renderer =

# Whether to use the Just-In-Time (JIT) compiler for shader emulation
# 0 : Interpreter (slow), 1 (default): JIT (fast)
use_shader_jit =

# Whether to use native 3DS screen resolution or to scale rendering resolution to the displayed screen size.
# 0 (default): Native, 1: Scaled
use_scaled_resolution =

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0.0-1.0. Defaults to 1.0 for all.
bg_red =
bg_blue =
bg_green =

[Audio]
# Which audio output engine to use.
# auto (default): Auto-select, null: No audio output, sdl2: SDL2 (if available)
output_engine =

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
