// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace AudioCore {

class Sink;

struct SinkDetails {
    SinkDetails(const char* id_, std::function<std::unique_ptr<Sink>()> factory_)
        : id(id_), factory(factory_) {}

    /// Name for this sink.
    const char* id;
    /// A method to call to construct an instance of this type of sink.
    std::function<std::unique_ptr<Sink>()> factory;
};

extern const std::vector<SinkDetails> g_sink_details;

} // namespace AudioCore
