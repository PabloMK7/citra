// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scm_rev.h"
#include "core/telemetry_session.h"

namespace Core {

TelemetrySession::TelemetrySession() {
    // TODO(bunnei): Replace with a backend that logs to our web service
    backend = std::make_unique<Telemetry::NullVisitor>();
}

TelemetrySession::~TelemetrySession() {
    // Complete the session, submitting to web service if necessary
    // This is just a placeholder to wrap up the session once the core completes and this is
    // destroyed. This will be moved elsewhere once we are actually doing real I/O with the service.
    field_collection.Accept(*backend);
    backend->Complete();
    backend = nullptr;
}

} // namespace Core
