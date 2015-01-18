// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"

namespace KeyMap {

/**
 * Represents a key for a specific host device.
 */
struct HostDeviceKey {
    int key_code;
    int device_id; ///< Uniquely identifies a host device

    bool operator < (const HostDeviceKey &other) const {
        if (device_id == other.device_id) {
            return key_code < other.key_code;
        }
        return device_id < other.device_id;
    }

    bool operator == (const HostDeviceKey &other) const {
        return device_id == other.device_id && key_code == other.key_code;
    }
};

/**
 * Generates a new device id, which uniquely identifies a host device within KeyMap.
 */
int NewDeviceId();

/**
 * Maps a device-specific key to a PadState.
 */
void SetKeyMapping(HostDeviceKey key, Service::HID::PadState padState);

/**
 * Gets the PadState that's mapped to the provided device-specific key.
 */
Service::HID::PadState GetPadKey(HostDeviceKey key);

}
