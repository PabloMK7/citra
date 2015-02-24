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
gpu_refresh_rate = ## 30 (default)
frame_skip = ## 0: No frameskip (default), 1 : 2x frameskip, 2 : 4x frameskip, etc.

[Data Storage]
use_virtual_sd =

[System Region]
region_value = ## 0 : Japan, 1 : Usa (default), 2 : Europe, 3 : Australia, 4 : China, 5 : Korea, 6 : Taiwan.

[Miscellaneous]
log_filter = *:Info  ## Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
)";

}
