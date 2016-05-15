// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "common/emu_window.h"
#include "common/key_map.h"

namespace KeyMap {

// TODO (wwylele): currently we treat c-stick as four direction buttons
//     and map it directly to EmuWindow::ButtonPressed.
//     It should go the analog input way like circle pad does.
const std::array<KeyTarget, Settings::NativeInput::NUM_INPUTS> mapping_targets = {{
    Service::HID::PAD_A, Service::HID::PAD_B, Service::HID::PAD_X, Service::HID::PAD_Y,
    Service::HID::PAD_L, Service::HID::PAD_R, Service::HID::PAD_ZL, Service::HID::PAD_ZR,
    Service::HID::PAD_START, Service::HID::PAD_SELECT, Service::HID::PAD_NONE,
    Service::HID::PAD_UP, Service::HID::PAD_DOWN, Service::HID::PAD_LEFT, Service::HID::PAD_RIGHT,
    Service::HID::PAD_C_UP, Service::HID::PAD_C_DOWN, Service::HID::PAD_C_LEFT, Service::HID::PAD_C_RIGHT,

    IndirectTarget::CirclePadUp,
    IndirectTarget::CirclePadDown,
    IndirectTarget::CirclePadLeft,
    IndirectTarget::CirclePadRight,
    IndirectTarget::CirclePadModifier,
}};

static std::map<HostDeviceKey, KeyTarget> key_map;
static int next_device_id = 0;

static bool circle_pad_up = false;
static bool circle_pad_down = false;
static bool circle_pad_left = false;
static bool circle_pad_right = false;
static bool circle_pad_modifier = false;

static void UpdateCirclePad(EmuWindow& emu_window) {
    constexpr float SQRT_HALF = 0.707106781;
    int x = 0, y = 0;

    if (circle_pad_right)
        ++x;
    if (circle_pad_left)
        --x;
    if (circle_pad_up)
        ++y;
    if (circle_pad_down)
        --y;

    float modifier = circle_pad_modifier ? Settings::values.pad_circle_modifier_scale : 1.0;
    emu_window.CirclePadUpdated(x * modifier * (y == 0 ? 1.0 : SQRT_HALF), y * modifier * (x == 0 ? 1.0 : SQRT_HALF));
}

int NewDeviceId() {
    return next_device_id++;
}

void SetKeyMapping(HostDeviceKey key, KeyTarget target) {
    key_map[key] = target;
}

void ClearKeyMapping(int device_id) {
    auto iter = key_map.begin();
    while (iter != key_map.end()) {
        if (iter->first.device_id == device_id)
            key_map.erase(iter++);
        else
            ++iter;
    }
}

void PressKey(EmuWindow& emu_window, HostDeviceKey key) {
    auto target = key_map.find(key);
    if (target == key_map.end())
        return;

    if (target->second.direct) {
        emu_window.ButtonPressed({{target->second.target.direct_target_hex}});
    } else {
        switch (target->second.target.indirect_target) {
        case IndirectTarget::CirclePadUp:
            circle_pad_up = true;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadDown:
            circle_pad_down = true;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadLeft:
            circle_pad_left = true;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadRight:
            circle_pad_right = true;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadModifier:
            circle_pad_modifier = true;
            UpdateCirclePad(emu_window);
            break;
        }
    }
}

void ReleaseKey(EmuWindow& emu_window,HostDeviceKey key) {
    auto target = key_map.find(key);
    if (target == key_map.end())
        return;

    if (target->second.direct) {
        emu_window.ButtonReleased({{target->second.target.direct_target_hex}});
    } else {
        switch (target->second.target.indirect_target) {
        case IndirectTarget::CirclePadUp:
            circle_pad_up = false;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadDown:
            circle_pad_down = false;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadLeft:
            circle_pad_left = false;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadRight:
            circle_pad_right = false;
            UpdateCirclePad(emu_window);
            break;
        case IndirectTarget::CirclePadModifier:
            circle_pad_modifier = false;
            UpdateCirclePad(emu_window);
            break;
        }
    }
}

}
