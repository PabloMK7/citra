// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <array>

#include "common/common_types.h"

namespace Settings {

namespace NativeInput {
enum Values {
    // directly mapped keys
    A, B, X, Y,
    L, R, ZL, ZR,
    START, SELECT, HOME,
    DUP, DDOWN, DLEFT, DRIGHT,
    CUP, CDOWN, CLEFT, CRIGHT,

    // indirectly mapped keys
    CIRCLE_UP, CIRCLE_DOWN, CIRCLE_LEFT, CIRCLE_RIGHT,
    CIRCLE_MODIFIER,

    NUM_INPUTS
};

static const std::array<const char*, NUM_INPUTS> Mapping = {{
    // directly mapped keys
    "pad_a", "pad_b", "pad_x", "pad_y",
    "pad_l", "pad_r", "pad_zl", "pad_zr",
    "pad_start", "pad_select", "pad_home",
    "pad_dup", "pad_ddown", "pad_dleft", "pad_dright",
    "pad_cup", "pad_cdown", "pad_cleft", "pad_cright",

    // indirectly mapped keys
    "pad_circle_up", "pad_circle_down", "pad_circle_left", "pad_circle_right",
    "pad_circle_modifier",
}};
static const std::array<Values, NUM_INPUTS> All = {{
    A, B, X, Y,
    L, R, ZL, ZR,
    START, SELECT, HOME,
    DUP, DDOWN, DLEFT, DRIGHT,
    CUP, CDOWN, CLEFT, CRIGHT,
    CIRCLE_UP, CIRCLE_DOWN, CIRCLE_LEFT, CIRCLE_RIGHT,
    CIRCLE_MODIFIER,
}};
}


struct Values {
    // CheckNew3DS
    bool is_new_3ds;

    // Controls
    std::array<int, NativeInput::NUM_INPUTS> input_mappings;
    float pad_circle_modifier_scale;

    // Core
    int frame_skip;

    // Data Storage
    bool use_virtual_sd;

    // System Region
    int region_value;

    // Renderer
    bool use_hw_renderer;
    bool use_shader_jit;
    bool use_scaled_resolution;

    float bg_red;
    float bg_green;
    float bg_blue;

    std::string log_filter;

    // Audio
    std::string sink_id;

    // Debugging
    bool use_gdbstub;
    u16 gdbstub_port;
} extern values;

void Apply();

}
