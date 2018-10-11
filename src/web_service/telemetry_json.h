// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <string>
#include "common/announce_multiplayer_room.h"
#include "common/telemetry.h"

namespace WebService {

/**
 * Implementation of VisitorInterface that serialized telemetry into JSON, and submits it to the
 * Citra web service
 */
class TelemetryJson : public Telemetry::VisitorInterface {
public:
    TelemetryJson(std::string host, std::string username, std::string token);
    ~TelemetryJson() override;

    void Visit(const Telemetry::Field<bool>& field) override;
    void Visit(const Telemetry::Field<double>& field) override;
    void Visit(const Telemetry::Field<float>& field) override;
    void Visit(const Telemetry::Field<u8>& field) override;
    void Visit(const Telemetry::Field<u16>& field) override;
    void Visit(const Telemetry::Field<u32>& field) override;
    void Visit(const Telemetry::Field<u64>& field) override;
    void Visit(const Telemetry::Field<s8>& field) override;
    void Visit(const Telemetry::Field<s16>& field) override;
    void Visit(const Telemetry::Field<s32>& field) override;
    void Visit(const Telemetry::Field<s64>& field) override;
    void Visit(const Telemetry::Field<std::string>& field) override;
    void Visit(const Telemetry::Field<const char*>& field) override;
    void Visit(const Telemetry::Field<std::chrono::microseconds>& field) override;

    void Complete() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace WebService
