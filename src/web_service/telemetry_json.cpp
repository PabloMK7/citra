// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/detached_tasks.h"
#include "web_service/telemetry_json.h"
#include "web_service/web_backend.h"

namespace WebService {

TelemetryJson::TelemetryJson(const std::string& host, const std::string& username,
                             const std::string& token)
    : host(std::move(host)), username(std::move(username)), token(std::move(token)) {}
TelemetryJson::~TelemetryJson() = default;

template <class T>
void TelemetryJson::Serialize(Telemetry::FieldType type, const std::string& name, T value) {
    sections[static_cast<u8>(type)][name] = value;
}

void TelemetryJson::SerializeSection(Telemetry::FieldType type, const std::string& name) {
    TopSection()[name] = sections[static_cast<unsigned>(type)];
}

void TelemetryJson::Visit(const Telemetry::Field<bool>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<double>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<float>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u8>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u16>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u32>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u64>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s8>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s16>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s32>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s64>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<std::string>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<const char*>& field) {
    Serialize(field.GetType(), field.GetName(), std::string(field.GetValue()));
}

void TelemetryJson::Visit(const Telemetry::Field<std::chrono::microseconds>& field) {
    Serialize(field.GetType(), field.GetName(), field.GetValue().count());
}

void TelemetryJson::Complete() {
    SerializeSection(Telemetry::FieldType::App, "App");
    SerializeSection(Telemetry::FieldType::Session, "Session");
    SerializeSection(Telemetry::FieldType::Performance, "Performance");
    SerializeSection(Telemetry::FieldType::UserFeedback, "UserFeedback");
    SerializeSection(Telemetry::FieldType::UserConfig, "UserConfig");
    SerializeSection(Telemetry::FieldType::UserSystem, "UserSystem");

    auto content = TopSection().dump();
    // Send the telemetry async but don't handle the errors since they were written to the log
    Common::DetachedTasks::AddTask(
        [host{this->host}, username{this->username}, token{this->token}, content]() {
            Client{host, username, token}.PostJson("/telemetry", content, true);
        });
}

} // namespace WebService
