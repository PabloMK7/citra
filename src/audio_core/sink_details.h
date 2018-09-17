// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AudioCore {

class Sink;

struct SinkDetails {
    using FactoryFn = std::function<std::unique_ptr<Sink>(std::string)>;
    using ListDevicesFn = std::function<std::vector<std::string>()>;

    SinkDetails(const char* id_, FactoryFn factory_, ListDevicesFn list_devices_)
        : id(id_), factory(std::move(factory_)), list_devices(std::move(list_devices_)) {}

    /// Name for this sink.
    const char* id;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn factory;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
};

extern const std::vector<SinkDetails> g_sink_details;

const SinkDetails& GetSinkDetails(std::string_view sink_id);

} // namespace AudioCore
