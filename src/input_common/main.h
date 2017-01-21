// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace InputCommon {

/// Initializes and registers all built-in input device factories.
void Init();

/// Unresisters all build-in input device factories and shut them down.
void Shutdown();

class Keyboard;

/// Gets the keyboard button device factory.
Keyboard* GetKeyboard();

/// Generates a serialized param package for creating a keyboard button device
std::string GenerateKeyboardParam(int key_code);

} // namespace InputCommon
