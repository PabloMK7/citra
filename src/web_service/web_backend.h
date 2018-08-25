// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <future>
#include <string>
#include <tuple>
#include <httplib.h>
#include "common/announce_multiplayer_room.h"
#include "common/common_types.h"

namespace LUrlParser {
class clParseURL;
}

namespace WebService {

/**
 * Requests a new JWT if necessary
 * @param force_new_token If true, force to request a new token from the server.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 * @return string with the current JWT toke
 */
std::string UpdateCoreJWT(bool force_new_token, const std::string& username,
                          const std::string& token);

/**
 * Posts JSON to a api.citra-emu.org.
 * @param url URL of the api.citra-emu.org endpoint to post data to.
 * @param parsed_url Parsed URL used for the POST request.
 * @param params Headers sent for the POST request.
 * @param data String of JSON data to use for the body of the POST request.
 * @param data If true, a JWT is requested in the function
 * @return future with the returned value of the POST
 */
static Common::WebResult PostJsonAsyncFn(const std::string& url,
                                         const LUrlParser::clParseURL& parsed_url,
                                         const httplib::Headers& params, const std::string& data,
                                         bool is_jwt_requested);

/**
 * Posts JSON to api.citra-emu.org.
 * @param url URL of the api.citra-emu.org endpoint to post data to.
 * @param data String of JSON data to use for the body of the POST request.
 * @param allow_anonymous If true, allow anonymous unauthenticated requests.
 * @return future with the returned value of the POST
 */
std::future<Common::WebResult> PostJson(const std::string& url, const std::string& data,
                                        bool allow_anonymous);

/**
 * Posts JSON to api.citra-emu.org.
 * @param url URL of the api.citra-emu.org endpoint to post data to.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 * @return future with the error or result of the POST
 */
std::future<Common::WebResult> PostJson(const std::string& url, const std::string& username,
                                        const std::string& token);

/**
 * Gets JSON from api.citra-emu.org.
 * @param func A function that gets exectued when the json as a string is received
 * @param url URL of the api.citra-emu.org endpoint to post data to.
 * @param allow_anonymous If true, allow anonymous unauthenticated requests.
 * @return future that holds the return value T of the func
 */
template <typename T>
std::future<T> GetJson(std::function<T(const std::string&)> func, const std::string& url,
                       bool allow_anonymous);

/**
 * Delete JSON to api.citra-emu.org.
 * @param url URL of the api.citra-emu.org endpoint to post data to.
 * @param data String of JSON data to use for the body of the DELETE request.
 */
void DeleteJson(const std::string& url, const std::string& data);

} // namespace WebService
