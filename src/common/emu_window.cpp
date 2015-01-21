// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "emu_window.h"

void EmuWindow::KeyPressed(KeyMap::HostDeviceKey key) {
    Service::HID::PadState mapped_key = KeyMap::GetPadKey(key);

    Service::HID::PadButtonPress(mapped_key);
}

void EmuWindow::KeyReleased(KeyMap::HostDeviceKey key) {
    Service::HID::PadState mapped_key = KeyMap::GetPadKey(key);

    Service::HID::PadButtonRelease(mapped_key);
}
