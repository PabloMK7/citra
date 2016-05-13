// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <tuple>
#include "core/hle/service/hid/hid.h"

class EmuWindow;

namespace KeyMap {

enum class IndirectTarget {
    CIRCLE_PAD_UP,
    CIRCLE_PAD_DOWN,
    CIRCLE_PAD_LEFT,
    CIRCLE_PAD_RIGHT,
    CIRCLE_PAD_MODIFIER,
};

/**
 * Represents a key mapping target. It can be a PadState that represents 3DS real buttons,
 * or an IndirectTarget.
 */
struct KeyTarget {
    bool direct;
    union {
        u32 direct_target_hex;
        IndirectTarget indirect_target;
    } target;

    KeyTarget() : direct(true) {
        target.direct_target_hex = 0;
    }

    KeyTarget(Service::HID::PadState pad) : direct(true) {
        target.direct_target_hex = pad.hex;
    }

    KeyTarget(IndirectTarget i) : direct(false) {
        target.indirect_target = i;
    }
};

/**
 * Represents a key for a specific host device.
 */
struct HostDeviceKey {
    int key_code;
    int device_id; ///< Uniquely identifies a host device

    bool operator<(const HostDeviceKey &other) const {
        return std::tie(key_code, device_id) <
               std::tie(other.key_code, other.device_id);
    }

    bool operator==(const HostDeviceKey &other) const {
        return std::tie(key_code, device_id) ==
               std::tie(other.key_code, other.device_id);
    }
};

extern const std::array<KeyTarget, Settings::NativeInput::NUM_INPUTS> mapping_targets;

/**
 * Generates a new device id, which uniquely identifies a host device within KeyMap.
 */
int NewDeviceId();

/**
 * Maps a device-specific key to a target (a PadState or an IndirectTarget).
 */
void SetKeyMapping(HostDeviceKey key, KeyTarget target);

/**
 * Clears all key mappings belonging to one device.
 */
void ClearKeyMapping(int device_id);

/**
 * Maps a key press actions and call the corresponding function in EmuWindow
 */
void PressKey(EmuWindow& emu_window, HostDeviceKey key);

/**
 * Maps a key release actions and call the corresponding function in EmuWindow
 */
void ReleaseKey(EmuWindow& emu_window, HostDeviceKey key);

}
