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

void Win32WSAStartup() {
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
}

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

    Win32WSAStartup();

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
    static std::future<void> future;
    future = cpr::PostCallback(
        [](cpr::Response r) {
            if (r.error) {
                LOG_ERROR(WebService, "POST returned cpr error: %u:%s",
                          static_cast<u32>(r.error.code), r.error.message.c_str());
                return;
            }
            if (r.status_code >= 400) {
                LOG_ERROR(WebService, "POST returned error status code: %u", r.status_code);
                return;
            }
            if (r.header["content-type"].find("application/json") == std::string::npos) {
                LOG_ERROR(WebService, "POST returned wrong content: %s",
                          r.header["content-type"].c_str());
                return;
            }
        },
        cpr::Url{url}, cpr::Body{data}, header);
}

template <typename T>
std::future<T> GetJson(std::function<T(const std::string&)> func, const std::string& url,
                       bool allow_anonymous, const std::string& username,
                       const std::string& token) {
    if (url.empty()) {
        LOG_ERROR(WebService, "URL is invalid");
        return std::async(std::launch::async, [func{std::move(func)}]() { return func(""); });
    }

    const bool are_credentials_provided{!token.empty() && !username.empty()};
    if (!allow_anonymous && !are_credentials_provided) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return std::async(std::launch::async, [func{std::move(func)}]() { return func(""); });
    }

    Win32WSAStartup();

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

    // Get JSON asynchronously
    return cpr::GetCallback(
        [func{std::move(func)}](cpr::Response r) {
            if (r.error) {
                LOG_ERROR(WebService, "GET returned cpr error: %u:%s",
                          static_cast<u32>(r.error.code), r.error.message.c_str());
                return func("");
            }
            if (r.status_code >= 400) {
                LOG_ERROR(WebService, "GET returned error code: %u", r.status_code);
                return func("");
            }
            if (r.header["content-type"].find("application/json") == std::string::npos) {
                LOG_ERROR(WebService, "GET returned wrong content: %s",
                          r.header["content-type"].c_str());
                return func("");
            }
            return func(r.text);
        },
        cpr::Url{url}, header);
}

template std::future<bool> GetJson(std::function<bool(const std::string&)> func,
                                   const std::string& url, bool allow_anonymous,
                                   const std::string& username, const std::string& token);

} // namespace WebService
