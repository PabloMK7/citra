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
pad_r =
pad_l =
pad_sup =
pad_sdown =
pad_sleft =
pad_sright =

[Core]
gpu_refresh_rate = ## 30 (default)
frame_skip = ## 0: No frameskip (default), 1 : 2x frameskip, 2 : 4x frameskip, etc.

[Data Storage]
use_virtual_sd =

[Miscellaneous]
log_filter = *:Info  ## Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
)";

}
