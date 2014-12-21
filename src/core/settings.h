// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Settings {

struct Values {
    // Controls
    int pad_a_key;
    int pad_b_key;
    int pad_x_key;
    int pad_y_key;
    int pad_l_key;
    int pad_r_key;
    int pad_start_key;
    int pad_select_key;
    int pad_home_key;
    int pad_dup_key;
    int pad_ddown_key;
    int pad_dleft_key;
    int pad_dright_key;
    int pad_sup_key;
    int pad_sdown_key;
    int pad_sleft_key;
    int pad_sright_key;

    // Core
    int cpu_core;
    int gpu_refresh_rate;

    // Data Storage
    bool use_virtual_sd;

    std::string log_filter;
} extern values;

}
