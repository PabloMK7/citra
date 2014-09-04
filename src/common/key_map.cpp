// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "key_map.h"
#include <map>


namespace KeyMap {

std::map<CitraKey, HID_User::PADState> g_key_map;

void SetKeyMapping(CitraKey key, HID_User::PADState padState) {
    g_key_map[key].hex = padState.hex;
}

HID_User::PADState Get3DSKey(CitraKey key) {
    return g_key_map[key];
}

}
