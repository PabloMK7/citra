// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/telemetry.h"

namespace Core {

/**
 * Instruments telemetry for this emulation session. Creates a new set of telemetry fields on each
 * session, logging any one-time fields. Interfaces with the telemetry backend used for submitting
 * data to the web service. Submits session data on close.
 */
class TelemetrySession : NonCopyable {
public:
    TelemetrySession();
    ~TelemetrySession();

    /**
     * Wrapper around the Telemetry::FieldCollection::AddField method.
     * @param type Type of the field to add.
     * @param name Name of the field to add.
     * @param value Value for the field to add.
     */
    template <typename T>
    void AddField(Telemetry::FieldType type, const char* name, T value) {
        field_collection.AddField(type, name, std::move(value));
    }

private:
    Telemetry::FieldCollection field_collection; ///< Tracks all added fields for the session
    std::unique_ptr<Telemetry::VisitorInterface> backend; ///< Backend interface that logs fields
};

/**
 * Gets TelemetryId, a unique identifier used for the user's telemetry sessions.
 * @returns The current TelemetryId for the session.
 */
u64 GetTelemetryId();

/**
 * Regenerates TelemetryId, a unique identifier used for the user's telemetry sessions.
 * @returns The new TelemetryId that was generated.
 */
u64 RegenerateTelemetryId();

/**
 * Verifies the username and token.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 * @returns Future with bool indicating whether the verification succeeded
 */
bool VerifyLogin(const std::string& username, const std::string& token);

} // namespace Core
