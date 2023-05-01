// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

class Sink;

enum class SinkType : u32 {
    Auto = 0,
    Null = 1,
    Cubeb = 2,
    OpenAL = 3,
    SDL2 = 4,

    NumSinkTypes,
};

/// Gets the name of a sink type.
std::string_view GetSinkName(SinkType sink_type);

/// Gets the list of devices for a particular sink identified by the given ID.
std::vector<std::string> GetDeviceListForSink(SinkType sink_type);

/// Creates an audio sink identified by the given device ID.
std::unique_ptr<Sink> CreateSinkFromID(SinkType sink_type, std::string_view device_id);

} // namespace AudioCore
