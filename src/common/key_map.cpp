// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "key_map.h"
#include <map>

namespace KeyMap {

static std::map<HostDeviceKey, Service::HID::PadState> key_map;
static int next_device_id = 0;

int NewDeviceId() {
    return next_device_id++;
}

void SetKeyMapping(HostDeviceKey key, Service::HID::PadState padState) {
    key_map[key].hex = padState.hex;
}

Service::HID::PadState GetPadKey(HostDeviceKey key) {
    return key_map[key];
}

}
