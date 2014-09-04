// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid.h"

namespace KeyMap {

class CitraKey {
public:
    CitraKey() : keyCode(0) {}
    CitraKey(int code) : keyCode(code) {}

    int keyCode;

    bool operator < (const CitraKey &other) const {
        return keyCode < other.keyCode;
    }

    bool operator == (const CitraKey &other) const {
        return keyCode == other.keyCode;
    }
};

struct DefaultKeyMapping {
    KeyMap::CitraKey key;
    HID_User::PADState state;
};

void SetKeyMapping(CitraKey key, HID_User::PADState padState);
HID_User::PADState Get3DSKey(CitraKey key);

}
