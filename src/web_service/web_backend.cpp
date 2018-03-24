// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <string>
#include <thread>
#include <LUrlParser.h>
#include <httplib.h>
#include "common/announce_multiplayer_room.h"
#include "common/logging/log.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char API_VERSION[]{"1"};

constexpr int HTTP_PORT = 80;
constexpr int HTTPS_PORT = 443;

constexpr int TIMEOUT_SECONDS = 30;

std::unique_ptr<httplib::Client> GetClientFor(const LUrlParser::clParseURL& parsedUrl) {
    namespace hl = httplib;

    int port;

    std::unique_ptr<hl::Client> cli;

    if (parsedUrl.m_Scheme == "http") {
        if (!parsedUrl.GetPort(&port)) {
            port = HTTP_PORT;
        }
        return std::make_unique<hl::Client>(parsedUrl.m_Host.c_str(), port, TIMEOUT_SECONDS,
                                            hl::HttpVersion::v1_1);
    } else if (parsedUrl.m_Scheme == "https") {
        if (!parsedUrl.GetPort(&port)) {
            port = HTTPS_PORT;
        }
        return std::make_unique<hl::SSLClient>(parsedUrl.m_Host.c_str(), port, TIMEOUT_SECONDS,
                                               hl::HttpVersion::v1_1);
    } else {
        LOG_ERROR(WebService, "Bad URL scheme %s", parsedUrl.m_Scheme.c_str());
        return nullptr;
    }
}

std::future<Common::WebResult> PostJson(const std::string& url, const std::string& data,
                                        bool allow_anonymous, const std::string& username,
                                        const std::string& token) {
    using lup = LUrlParser::clParseURL;
    namespace hl = httplib;

    lup parsedUrl = lup::ParseURL(url);

    if (url.empty() || !parsedUrl.IsValid()) {
        LOG_ERROR(WebService, "URL is invalid");
        return std::async(std::launch::deferred, []() {
            return Common::WebResult{Common::WebResult::Code::InvalidURL, "URL is invalid"};
        });
    }

    const bool are_credentials_provided{!token.empty() && !username.empty()};
    if (!allow_anonymous && !are_credentials_provided) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return std::async(std::launch::deferred, []() {
            return Common::WebResult{Common::WebResult::Code::CredentialsMissing,
                                     "Credentials needed"};
        });
    }

    // Built request header
    hl::Headers params;
    if (are_credentials_provided) {
        // Authenticated request if credentials are provided
        params = {{std::string("x-username"), username},
                  {std::string("x-token"), token},
                  {std::string("api-version"), std::string(API_VERSION)},
                  {std::string("Content-Type"), std::string("application/json")}};
    } else {
        // Otherwise, anonymous request
        params = {{std::string("api-version"), std::string(API_VERSION)},
                  {std::string("Content-Type"), std::string("application/json")}};
    }

    // Post JSON asynchronously
    return std::async(std::launch::async, [url, parsedUrl, params, data] {
        std::unique_ptr<hl::Client> cli = GetClientFor(parsedUrl);

        if (cli == nullptr) {
            return Common::WebResult{Common::WebResult::Code::InvalidURL, "URL is invalid"};
        }

        hl::Request request;
        request.method = "POST";
        request.path = "/" + parsedUrl.m_Path;
        request.headers = params;
        request.body = data;

        hl::Response response;

        if (!cli->send(request, response)) {
            LOG_ERROR(WebService, "POST to %s returned null", url.c_str());
            return Common::WebResult{Common::WebResult::Code::LibError, "Null response"};
        }

        if (response.status >= 400) {
            LOG_ERROR(WebService, "POST to %s returned error status code: %u", url.c_str(),
                      response.status);
            return Common::WebResult{Common::WebResult::Code::HttpError,
                                     std::to_string(response.status)};
        }

        auto content_type = response.headers.find("content-type");

        if (content_type == response.headers.end() ||
            content_type->second.find("application/json") == std::string::npos) {
            LOG_ERROR(WebService, "POST to %s returned wrong content: %s", url.c_str(),
                      content_type->second.c_str());
            return Common::WebResult{Common::WebResult::Code::WrongContent, content_type->second};
        }

        return Common::WebResult{Common::WebResult::Code::Success, ""};
    });
}

template <typename T>
std::future<T> GetJson(std::function<T(const std::string&)> func, const std::string& url,
                       bool allow_anonymous, const std::string& username,
                       const std::string& token) {
    using lup = LUrlParser::clParseURL;
    namespace hl = httplib;

    lup parsedUrl = lup::ParseURL(url);

    if (url.empty() || !parsedUrl.IsValid()) {
        LOG_ERROR(WebService, "URL is invalid");
        return std::async(std::launch::deferred, [func{std::move(func)}]() { return func(""); });
    }

    const bool are_credentials_provided{!token.empty() && !username.empty()};
    if (!allow_anonymous && !are_credentials_provided) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return std::async(std::launch::deferred, [func{std::move(func)}]() { return func(""); });
    }

    // Built request header
    hl::Headers params;
    if (are_credentials_provided) {
        // Authenticated request if credentials are provided
        params = {{std::string("x-username"), username},
                  {std::string("x-token"), token},
                  {std::string("api-version"), std::string(API_VERSION)}};
    } else {
        // Otherwise, anonymous request
        params = {{std::string("api-version"), std::string(API_VERSION)}};
    }

    // Get JSON asynchronously
    return std::async(std::launch::async, [func, url, parsedUrl, params] {
        std::unique_ptr<hl::Client> cli = GetClientFor(parsedUrl);

        if (cli == nullptr) {
            return func("");
        }

        hl::Request request;
        request.method = "GET";
        request.path = "/" + parsedUrl.m_Path;
        request.headers = params;

        hl::Response response;

        if (!cli->send(request, response)) {
            LOG_ERROR(WebService, "GET to %s returned null", url.c_str());
            return func("");
        }

        if (response.status >= 400) {
            LOG_ERROR(WebService, "GET to %s returned error status code: %u", url.c_str(),
                      response.status);
            return func("");
        }

        auto content_type = response.headers.find("content-type");

        if (content_type == response.headers.end() ||
            content_type->second.find("application/json") == std::string::npos) {
            LOG_ERROR(WebService, "GET to %s returned wrong content: %s", url.c_str(),
                      content_type->second.c_str());
            return func("");
        }

        return func(response.body);
    });
}

template std::future<bool> GetJson(std::function<bool(const std::string&)> func,
                                   const std::string& url, bool allow_anonymous,
                                   const std::string& username, const std::string& token);
template std::future<AnnounceMultiplayerRoom::RoomList> GetJson(
    std::function<AnnounceMultiplayerRoom::RoomList(const std::string&)> func,
    const std::string& url, bool allow_anonymous, const std::string& username,
    const std::string& token);

void DeleteJson(const std::string& url, const std::string& data, const std::string& username,
                const std::string& token) {
    using lup = LUrlParser::clParseURL;
    namespace hl = httplib;

    lup parsedUrl = lup::ParseURL(url);

    if (url.empty() || !parsedUrl.IsValid()) {
        LOG_ERROR(WebService, "URL is invalid");
        return;
    }

    const bool are_credentials_provided{!token.empty() && !username.empty()};
    if (!are_credentials_provided) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return;
    }

    // Built request header
    hl::Headers params = {{std::string("x-username"), username},
                          {std::string("x-token"), token},
                          {std::string("api-version"), std::string(API_VERSION)},
                          {std::string("Content-Type"), std::string("application/json")}};

    // Delete JSON asynchronously
    std::async(std::launch::async, [url, parsedUrl, params, data] {
        std::unique_ptr<hl::Client> cli = GetClientFor(parsedUrl);

        if (cli == nullptr) {
            return;
        }

        hl::Request request;
        request.method = "DELETE";
        request.path = "/" + parsedUrl.m_Path;
        request.headers = params;
        request.body = data;

        hl::Response response;

        if (!cli->send(request, response)) {
            LOG_ERROR(WebService, "DELETE to %s returned null", url.c_str());
            return;
        }

        if (response.status >= 400) {
            LOG_ERROR(WebService, "DELETE to %s returned error status code: %u", url.c_str(),
                      response.status);
            return;
        }

        auto content_type = response.headers.find("content-type");

        if (content_type == response.headers.end() ||
            content_type->second.find("application/json") == std::string::npos) {
            LOG_ERROR(WebService, "DELETE to %s returned wrong content: %s", url.c_str(),
                      content_type->second.c_str());
            return;
        }

        return;
    });
}

} // namespace WebService
