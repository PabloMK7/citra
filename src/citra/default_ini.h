// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace DefaultINI {

const char* glfw_config_file = R"(
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
pad_sup =
pad_sdown =
pad_sleft =
pad_sright =
pad_cup =
pad_cdown =
pad_cleft =
pad_cright =

[Core]
# The applied frameskip amount. Must be a power of two.
# 0 (default): No frameskip, 1: x2 frameskip, 2: x4 frameskip, 3: x8 frameskip, etc.
frame_skip =

[Renderer]
# Whether to use software or hardware rendering.
# 0 (default): Software, 1: Hardware
use_hw_renderer =

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0.0-1.0. Defaults to 1.0 for all.
bg_red =
bg_blue =
bg_green =

[Data Storage]
# Whether to create a virtual SD card.
# 1 (default): Yes, 0: No
use_virtual_sd =

[System Region]
# The system region that Citra will use during emulation
# 0: Japan, 1: USA (default), 2: Europe, 3: Australia, 4: China, 5: Korea, 6: Taiwan
region_value =

[Miscellaneous]
# A filter which removes logs below a certain logging level.
# Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
log_filter = *:Info
)";

}
