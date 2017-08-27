// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <json.hpp>
#include "common/telemetry.h"

namespace WebService {

/**
 * Implementation of VisitorInterface that serialized telemetry into JSON, and submits it to the
 * Citra web service
 */
class TelemetryJson : public Telemetry::VisitorInterface {
public:
    TelemetryJson(const std::string& endpoint_url, const std::string& username,
                  const std::string& token)
        : endpoint_url(endpoint_url), username(username), token(token) {}
    ~TelemetryJson() = default;

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
    nlohmann::json& TopSection() {
        return sections[static_cast<u8>(Telemetry::FieldType::None)];
    }

    template <class T>
    void Serialize(Telemetry::FieldType type, const std::string& name, T value);

    void SerializeSection(Telemetry::FieldType type, const std::string& name);

    nlohmann::json output;
    std::array<nlohmann::json, 7> sections;
    std::string endpoint_url;
    std::string username;
    std::string token;
};

} // namespace WebService
