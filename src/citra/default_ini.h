// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
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
cpu_core = ## 0: Interpreter (default), 1: FastInterpreter (experimental)
gpu_refresh_rate = ## 60 (default)

[Data Storage]
use_virtual_sd =
)";

}
