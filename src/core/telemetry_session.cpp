// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/scm_rev.h"
#include "core/telemetry_session.h"
#include "web_services/telemetry_json.h"

namespace Core {

TelemetrySession::TelemetrySession() {
    backend = std::make_unique<WebService::TelemetryJson>();

    // Log one-time session start information
    const auto duration{std::chrono::steady_clock::now().time_since_epoch()};
    const auto start_time{std::chrono::duration_cast<std::chrono::microseconds>(duration).count()};
    AddField(Telemetry::FieldType::Session, "StartTime", start_time);

    // Log one-time application information
    const bool is_git_dirty{std::strstr(Common::g_scm_desc, "dirty") != nullptr};
    AddField(Telemetry::FieldType::App, "GitIsDirty", is_git_dirty);
    AddField(Telemetry::FieldType::App, "GitBranch", Common::g_scm_branch);
    AddField(Telemetry::FieldType::App, "GitRevision", Common::g_scm_rev);
}

TelemetrySession::~TelemetrySession() {
    // Log one-time session end information
    const auto duration{std::chrono::steady_clock::now().time_since_epoch()};
    const auto end_time{std::chrono::duration_cast<std::chrono::microseconds>(duration).count()};
    AddField(Telemetry::FieldType::Session, "EndTime", end_time);

    // Complete the session, submitting to web service if necessary
    // This is just a placeholder to wrap up the session once the core completes and this is
    // destroyed. This will be moved elsewhere once we are actually doing real I/O with the service.
    field_collection.Accept(*backend);
    backend->Complete();
    backend = nullptr;
}

} // namespace Core
