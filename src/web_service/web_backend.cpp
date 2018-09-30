// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <string>
#include <thread>
#include <LUrlParser.h>
#include "common/announce_multiplayer_room.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "web_service/web_backend.h"

namespace WebService {

static constexpr char API_VERSION[]{"1"};

constexpr int HTTP_PORT = 80;
constexpr int HTTPS_PORT = 443;

constexpr int TIMEOUT_SECONDS = 30;

Client::JWTCache Client::jwt_cache{};

Client::Client(const std::string& host, const std::string& username, const std::string& token)
    : host(host), username(username), token(token) {
    std::lock_guard<std::mutex> lock(jwt_cache.mutex);
    if (username == jwt_cache.username && token == jwt_cache.token) {
        jwt = jwt_cache.jwt;
    }
}

Common::WebResult Client::GenericJson(const std::string& method, const std::string& path,
                                      const std::string& data, const std::string& jwt,
                                      const std::string& username, const std::string& token) {
    if (cli == nullptr) {
        auto parsedUrl = LUrlParser::clParseURL::ParseURL(host);
        int port;
        if (parsedUrl.m_Scheme == "http") {
            if (!parsedUrl.GetPort(&port)) {
                port = HTTP_PORT;
            }
            cli =
                std::make_unique<httplib::Client>(parsedUrl.m_Host.c_str(), port, TIMEOUT_SECONDS);
        } else if (parsedUrl.m_Scheme == "https") {
            if (!parsedUrl.GetPort(&port)) {
                port = HTTPS_PORT;
            }
            cli = std::make_unique<httplib::SSLClient>(parsedUrl.m_Host.c_str(), port,
                                                       TIMEOUT_SECONDS);
        } else {
            LOG_ERROR(WebService, "Bad URL scheme {}", parsedUrl.m_Scheme);
            return Common::WebResult{Common::WebResult::Code::InvalidURL, "Bad URL scheme"};
        }
    }
    if (cli == nullptr) {
        LOG_ERROR(WebService, "Invalid URL {}", host + path);
        return Common::WebResult{Common::WebResult::Code::InvalidURL, "Invalid URL"};
    }

    httplib::Headers params;
    if (!jwt.empty()) {
        params = {
            {std::string("Authorization"), fmt::format("Bearer {}", jwt)},
        };
    } else if (!username.empty()) {
        params = {
            {std::string("x-username"), username},
            {std::string("x-token"), token},
        };
    }

    params.emplace(std::string("api-version"), std::string(API_VERSION));
    if (method != "GET") {
        params.emplace(std::string("Content-Type"), std::string("application/json"));
    };

    httplib::Request request;
    request.method = method;
    request.path = path;
    request.headers = params;
    request.body = data;

    httplib::Response response;

    if (!cli->send(request, response)) {
        LOG_ERROR(WebService, "{} to {} returned null", method, host + path);
        return Common::WebResult{Common::WebResult::Code::LibError, "Null response"};
    }

    if (response.status >= 400) {
        LOG_ERROR(WebService, "{} to {} returned error status code: {}", method, host + path,
                  response.status);
        return Common::WebResult{Common::WebResult::Code::HttpError,
                                 std::to_string(response.status)};
    }

    auto content_type = response.headers.find("content-type");

    if (content_type == response.headers.end()) {
        LOG_ERROR(WebService, "{} to {} returned no content", method, host + path);
        return Common::WebResult{Common::WebResult::Code::WrongContent, ""};
    }

    if (content_type->second.find("application/json") == std::string::npos &&
        content_type->second.find("text/html; charset=utf-8") == std::string::npos) {
        LOG_ERROR(WebService, "{} to {} returned wrong content: {}", method, host + path,
                  content_type->second);
        return Common::WebResult{Common::WebResult::Code::WrongContent, "Wrong content"};
    }
    return Common::WebResult{Common::WebResult::Code::Success, "", response.body};
}

void Client::UpdateJWT() {
    if (!username.empty() && !token.empty()) {
        auto result = GenericJson("POST", "/jwt/internal", "", "", username, token);
        if (result.result_code != Common::WebResult::Code::Success) {
            LOG_ERROR(WebService, "UpdateJWT failed");
        } else {
            std::lock_guard<std::mutex> lock(jwt_cache.mutex);
            jwt_cache.username = username;
            jwt_cache.token = token;
            jwt_cache.jwt = jwt = result.returned_data;
        }
    }
}

Common::WebResult Client::GenericJson(const std::string& method, const std::string& path,
                                      const std::string& data, bool allow_anonymous) {
    if (jwt.empty()) {
        UpdateJWT();
    }

    if (jwt.empty() && !allow_anonymous) {
        LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
        return Common::WebResult{Common::WebResult::Code::CredentialsMissing, "Credentials needed"};
    }

    auto result = GenericJson(method, path, data, jwt);
    if (result.result_string == "401") {
        // Try again with new JWT
        UpdateJWT();
        result = GenericJson(method, path, data, jwt);
    }

    return result;
}

} // namespace WebService
