// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <array>

namespace Settings {

namespace NativeInput {
enum Values {
    A, B, X, Y,
    L, R, ZL, ZR,
    START, SELECT, HOME,
    DUP, DDOWN, DLEFT, DRIGHT,
    SUP, SDOWN, SLEFT, SRIGHT,
    CUP, CDOWN, CLEFT, CRIGHT,
    NUM_INPUTS
};
static const std::array<const char*, NUM_INPUTS> Mapping = {
    "pad_a", "pad_b", "pad_x", "pad_y",
    "pad_l", "pad_r", "pad_zl", "pad_zr",
    "pad_start", "pad_select", "pad_home",
    "pad_dup", "pad_ddown", "pad_dleft", "pad_dright",
    "pad_sup", "pad_sdown", "pad_sleft", "pad_sright",
    "pad_cup", "pad_cdown", "pad_cleft", "pad_cright"
};
static const std::array<Values, NUM_INPUTS> All = {
    A, B, X, Y,
    L, R, ZL, ZR,
    START, SELECT, HOME,
    DUP, DDOWN, DLEFT, DRIGHT,
    SUP, SDOWN, SLEFT, SRIGHT,
    CUP, CDOWN, CLEFT, CRIGHT
};
}


struct Values {
    // Controls
    std::array<int, NativeInput::NUM_INPUTS> input_mappings;

    // Core
    int frame_skip;

    // Data Storage
    bool use_virtual_sd;

    // System Region
    int region_value;

    // Renderer
    bool use_hw_renderer;

    float bg_red;
    float bg_green;
    float bg_blue;

    std::string log_filter;
} extern values;

}
