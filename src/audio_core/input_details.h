// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

class Input;

enum class InputType : u32 {
    Auto = 0,
    Null = 1,
    Static = 2,
    Cubeb = 3,
    OpenAL = 4,

    NumInputTypes,
};

/// Gets the name of a input type.
std::string_view GetInputName(InputType input_type);

/// Gets the list of devices for a particular input identified by the given ID.
std::vector<std::string> GetDeviceListForInput(InputType input_type);

/// Creates an audio input identified by the given device ID.
std::unique_ptr<Input> CreateInputFromID(InputType input_type, std::string_view device_id);

} // namespace AudioCore
