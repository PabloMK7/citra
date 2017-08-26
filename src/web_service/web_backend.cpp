// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32
#include <winsock.h>
#endif

#include <cstdlib>
#include <thread>
#include <cpr/cpr.h>
#include "common/logging/log.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char API_VERSION[]{"1"};

static std::unique_ptr<cpr::Session> g_session;

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

#ifdef _WIN32
    // On Windows, CPR/libcurl does not properly initialize Winsock. The below code is used to
    // initialize Winsock globally, which fixes this problem. Without this, only the first CPR
    // session will properly be created, and subsequent ones will fail.
    WSADATA wsa_data;
    const int wsa_result{WSAStartup(MAKEWORD(2, 2), &wsa_data)};
    if (wsa_result) {
        LOG_CRITICAL(WebService, "WSAStartup failed: %d", wsa_result);
    }
#endif

    // Built request header
    cpr::Header header;
    if (are_credentials_provided) {
        // Authenticated request if credentials are provided
        header = {{"Content-Type", "application/json"},
                  {"x-username", username.c_str()},
                  {"x-token", token.c_str()},
                  {"api-version", API_VERSION}};
    } else {
        // Otherwise, anonymous request
        header = cpr::Header{{"Content-Type", "application/json"}, {"api-version", API_VERSION}};
    }

    // Post JSON asynchronously
    static cpr::AsyncResponse future;
    future = cpr::PostAsync(cpr::Url{url.c_str()}, cpr::Body{data.c_str()}, header);
}

} // namespace WebService
