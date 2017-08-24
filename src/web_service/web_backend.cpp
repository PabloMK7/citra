// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cpr/cpr.h>
#include <stdlib.h>
#include "common/logging/log.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char API_VERSION[]{"1"};

void PostJson(const std::string& url, const std::string& data, bool allow_anonymous,
              const std::string& username, const std::string& token) {
    if (url.empty()) {
        LOG_ERROR(WebService, "URL is invalid");
        return;
    }

    const bool are_credentials_provided{!token.empty() && !username.empty()};
    if (!allow_anonymous && !are_credentials_provided) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return;
    }

    if (are_credentials_provided) {
        // Authenticated request if credentials are provided
        cpr::PostAsync(cpr::Url{url}, cpr::Body{data},
                       cpr::Header{{"Content-Type", "application/json"},
                                   {"x-username", username},
                                   {"x-token", token},
                                   {"api-version", API_VERSION}});
    } else {
        // Otherwise, anonymous request
        cpr::PostAsync(
            cpr::Url{url}, cpr::Body{data},
            cpr::Header{{"Content-Type", "application/json"}, {"api-version", API_VERSION}});
    }
}

} // namespace WebService
