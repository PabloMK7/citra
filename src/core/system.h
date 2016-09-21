// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

class EmuWindow;

namespace System {

enum class Result {
    Success,            ///< Everything is fine
    Error,              ///< Something went wrong (no module specified)
    ErrorInitCore,      ///< Something went wrong during core init
    ErrorInitVideoCore, ///< Something went wrong during video core init
};

Result Init(EmuWindow* emu_window);
bool IsPoweredOn();
void Shutdown();
}
