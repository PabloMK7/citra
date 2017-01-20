// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common_types.h"
#include "core/hle/service/cam/cam.h"

namespace Settings {

enum class LayoutOption {
    Default,
    SingleScreen,
    LargeScreen,
    Custom,
};

namespace NativeInput {

enum Values {
    // directly mapped keys
    A,
    B,
    X,
    Y,
    L,
    R,
    ZL,
    ZR,
    START,
    SELECT,
    HOME,
    DUP,
    DDOWN,
    DLEFT,
    DRIGHT,
    CUP,
    CDOWN,
    CLEFT,
    CRIGHT,

    // indirectly mapped keys
    CIRCLE_UP,
    CIRCLE_DOWN,
    CIRCLE_LEFT,
    CIRCLE_RIGHT,
    CIRCLE_MODIFIER,

    NUM_INPUTS
};

static const std::array<const char*, NUM_INPUTS> Mapping = {{
    // directly mapped keys
    "pad_a", "pad_b", "pad_x", "pad_y", "pad_l", "pad_r", "pad_zl", "pad_zr", "pad_start",
    "pad_select", "pad_home", "pad_dup", "pad_ddown", "pad_dleft", "pad_dright", "pad_cup",
    "pad_cdown", "pad_cleft", "pad_cright",

    // indirectly mapped keys
    "pad_circle_up", "pad_circle_down", "pad_circle_left", "pad_circle_right",
    "pad_circle_modifier",
}};
static const std::array<Values, NUM_INPUTS> All = {{
    A,     B,      X,      Y,         L,           R,           ZL,           ZR,
    START, SELECT, HOME,   DUP,       DDOWN,       DLEFT,       DRIGHT,       CUP,
    CDOWN, CLEFT,  CRIGHT, CIRCLE_UP, CIRCLE_DOWN, CIRCLE_LEFT, CIRCLE_RIGHT, CIRCLE_MODIFIER,
}};
}

namespace NativeButton {
enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,

    ZL,
    ZR,

    Home,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_IR_BEGIN = ZL;
constexpr int BUTTON_NS_BEGIN = Home;

constexpr int BUTTON_HID_END = BUTTON_IR_BEGIN;
constexpr int BUTTON_IR_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_IR = BUTTON_IR_END - BUTTON_IR_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

static const std::array<const char*, NumButtons> mapping = {{
    "button_a", "button_b", "button_x", "button_y", "button_up", "button_down", "button_left",
    "button_right", "button_l", "button_r", "button_start", "button_select", "button_zl",
    "button_zr", "button_home",
}};
} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    CirclePad,
    CStick,

    NumAnalogs,
};

static const std::array<const char*, NumAnalogs> mapping = {{
    "circle_pad", "c_stick",
}};
} // namespace NumAnalog

struct Values {
    // CheckNew3DS
    bool is_new_3ds;

    // Controls
    std::array<int, NativeInput::NUM_INPUTS> input_mappings;
    float pad_circle_modifier_scale;

    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;

    // Core
    bool use_cpu_jit;

    // Data Storage
    bool use_virtual_sd;

    // System Region
    int region_value;

    // Renderer
    bool use_hw_renderer;
    bool use_shader_jit;
    float resolution_factor;
    bool use_vsync;
    bool toggle_framelimit;

    LayoutOption layout_option;
    bool swap_screen;

    float bg_red;
    float bg_green;
    float bg_blue;

    std::string log_filter;

    // Audio
    std::string sink_id;
    bool enable_audio_stretching;
    std::string audio_device_id;

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;

    // Debugging
    bool use_gdbstub;
    u16 gdbstub_port;
} extern values;

// a special value for Values::region_value indicating that citra will automatically select a region
// value to fit the region lockout info of the game
static constexpr int REGION_VALUE_AUTO_SELECT = -1;

void Apply();
}
