// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include "common/common_types.h"
#include "core/hle/service/cam/cam.h"

namespace Settings {

enum class LayoutOption {
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
};

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
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_up",
    "button_down",
    "button_left",
    "button_right",
    "button_l",
    "button_r",
    "button_start",
    "button_select",
    "button_zl",
    "button_zr",
    "button_home",
}};
} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    CirclePad,
    CStick,

    NumAnalogs,
};

static const std::array<const char*, NumAnalogs> mapping = {{
    "circle_pad",
    "c_stick",
}};
} // namespace NativeAnalog

struct Values {
    // CheckNew3DS
    bool is_new_3ds;

    // Controls
    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;
    std::string motion_device;
    std::string touch_device;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;

    // Core
    bool use_cpu_jit;

    // Data Storage
    bool use_virtual_sd;

    // System Region
    int region_value;

    // Renderer
    bool use_hw_renderer;
    bool use_hw_shader;
    bool shaders_accurate_gs;
    bool shaders_accurate_mul;
    bool use_shader_jit;
    u16 resolution_factor;
    bool use_vsync;
    bool use_frame_limit;
    u16 frame_limit;

    LayoutOption layout_option;
    bool swap_screen;
    bool custom_layout;
    u16 custom_top_left;
    u16 custom_top_top;
    u16 custom_top_right;
    u16 custom_top_bottom;
    u16 custom_bottom_left;
    u16 custom_bottom_top;
    u16 custom_bottom_right;
    u16 custom_bottom_bottom;

    float bg_red;
    float bg_green;
    float bg_blue;

    bool toggle_3d;
    u8 factor_3d;

    // Audio
    std::string sink_id;
    bool enable_audio_stretching;
    std::string audio_device_id;
    float volume;

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Debugging
    bool use_gdbstub;
    u16 gdbstub_port;
    std::string log_filter;
    std::unordered_map<std::string, bool> lle_modules;

    // Movie
    std::string movie_play;
    std::string movie_record;

    // WebService
    bool enable_telemetry;
    std::string telemetry_endpoint_url;
    std::string verify_endpoint_url;
    std::string announce_multiplayer_room_endpoint_url;
    std::string citra_username;
    std::string citra_token;
} extern values;

// a special value for Values::region_value indicating that citra will automatically select a region
// value to fit the region lockout info of the game
static constexpr int REGION_VALUE_AUTO_SELECT = -1;

void Apply();
void LogSettings();
} // namespace Settings
