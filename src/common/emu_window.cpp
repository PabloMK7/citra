// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "emu_window.h"

void EmuWindow::KeyPressed(KeyMap::HostDeviceKey key) {
    HID_User::PadState mapped_key = KeyMap::GetPadKey(key);

    HID_User::PadButtonPress(mapped_key);
}

void EmuWindow::KeyReleased(KeyMap::HostDeviceKey key) {
    HID_User::PadState mapped_key = KeyMap::GetPadKey(key);

    HID_User::PadButtonRelease(mapped_key);
}
