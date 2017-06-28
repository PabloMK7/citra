// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cpr/cpr.h>
#include <stdlib.h>
#include "common/logging/log.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char ENV_VAR_USERNAME[]{"CITRA_WEB_SERVICES_USERNAME"};
static constexpr char ENV_VAR_TOKEN[]{"CITRA_WEB_SERVICES_TOKEN"};

static std::string GetEnvironmentVariable(const char* name) {
    const char* value{getenv(name)};
    if (value) {
        return value;
    }
    return {};
}

const std::string& GetUsername() {
    static const std::string username{GetEnvironmentVariable(ENV_VAR_USERNAME)};
    return username;
}

const std::string& GetToken() {
    static const std::string token{GetEnvironmentVariable(ENV_VAR_TOKEN)};
    return token;
}

void PostJson(const std::string& url, const std::string& data) {
    if (url.empty()) {
        LOG_ERROR(WebService, "URL is invalid");
        return;
    }

    if (GetUsername().empty() || GetToken().empty()) {
        LOG_ERROR(WebService, "Environment variables %s and %s must be set to POST JSON",
                  ENV_VAR_USERNAME, ENV_VAR_TOKEN);
        return;
    }

    cpr::PostAsync(cpr::Url{url}, cpr::Body{data}, cpr::Header{{"Content-Type", "application/json"},
                                                               {"x-username", GetUsername()},
                                                               {"x-token", GetToken()}});
}

} // namespace WebService
