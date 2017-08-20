// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace InputCommon {

/// Initializes and registers all built-in input device factories.
void Init();

/// Deregisters all built-in input device factories and shuts them down.
void Shutdown();

class Keyboard;

/// Gets the keyboard button device factory.
Keyboard* GetKeyboard();

class MotionEmu;

/// Gets the motion emulation factory.
MotionEmu* GetMotionEmu();

/// Generates a serialized param package for creating a keyboard button device
std::string GenerateKeyboardParam(int key_code);

/// Generates a serialized param package for creating an analog device taking input from keyboard
std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale);

} // namespace InputCommon
