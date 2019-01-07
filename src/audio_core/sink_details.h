// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace AudioCore {

class Sink;

/// Retrieves the IDs for all available audio sinks.
std::vector<const char*> GetSinkIDs();

/// Gets the list of devices for a particular sink identified by the given ID.
std::vector<std::string> GetDeviceListForSink(std::string_view sink_id);

/// Creates an audio sink identified by the given device ID.
std::unique_ptr<Sink> CreateSinkFromID(std::string_view sink_id, std::string_view device_id);

} // namespace AudioCore
