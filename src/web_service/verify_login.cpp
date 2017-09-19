// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <json.hpp>
#include "web_service/verify_login.h"
#include "web_service/web_backend.h"

namespace WebService {

std::future<bool> VerifyLogin(std::string& username, std::string& token,
                              const std::string& endpoint_url, std::function<void()> func) {
    auto get_func = [func, username](const std::string& reply) -> bool {
        func();
        if (reply.empty())
            return false;
        nlohmann::json json = nlohmann::json::parse(reply);
        std::string result;
        try {
            result = json["username"];
        } catch (const nlohmann::detail::out_of_range&) {
        }
        return result == username;
    };
    return GetJson<bool>(get_func, endpoint_url, false, username, token);
}

} // namespace WebService
