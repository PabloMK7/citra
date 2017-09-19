// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <future>
#include <string>
#include "common/common_types.h"

namespace WebService {

/**
 * Posts JSON to services.citra-emu.org.
 * @param url URL of the services.citra-emu.org endpoint to post data to.
 * @param data String of JSON data to use for the body of the POST request.
 * @param allow_anonymous If true, allow anonymous unauthenticated requests.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 */
void PostJson(const std::string& url, const std::string& data, bool allow_anonymous,
              const std::string& username = {}, const std::string& token = {});

/**
 * Gets JSON from services.citra-emu.org.
 * @param func A function that gets exectued when the json as a string is received
 * @param url URL of the services.citra-emu.org endpoint to post data to.
 * @param allow_anonymous If true, allow anonymous unauthenticated requests.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 * @return future that holds the return value T of the func
 */
template <typename T>
std::future<T> GetJson(std::function<T(const std::string&)> func, const std::string& url,
                       bool allow_anonymous, const std::string& username = {},
                       const std::string& token = {});

} // namespace WebService
