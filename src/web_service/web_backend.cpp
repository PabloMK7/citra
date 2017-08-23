// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cpr/cpr.h>
#include <stdlib.h>
#include "common/logging/log.h"
#include "core/settings.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char API_VERSION[]{"1"};

void PostJson(const std::string& url, const std::string& data) {
    if (!Settings::values.enable_telemetry) {
        // Telemetry disabled by user configuration
        return;
    }

    if (url.empty()) {
        LOG_ERROR(WebService, "URL is invalid");
        return;
    }

    if (Settings::values.citra_token.empty() || Settings::values.citra_username.empty()) {
        // Anonymous request if citra token or username are empty
        cpr::PostAsync(
            cpr::Url{url}, cpr::Body{data},
            cpr::Header{{"Content-Type", "application/json"}, {"api-version", API_VERSION}});
    } else {
        // We have both, do an authenticated request
        cpr::PostAsync(cpr::Url{url}, cpr::Body{data},
                       cpr::Header{{"Content-Type", "application/json"},
                                   {"x-username", Settings::values.citra_username},
                                   {"x-token", Settings::values.citra_token},
                                   {"api-version", API_VERSION}});
    }
}

} // namespace WebService
