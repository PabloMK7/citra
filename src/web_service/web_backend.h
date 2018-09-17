// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <tuple>
#include <httplib.h>
#include "common/announce_multiplayer_room.h"
#include "common/common_types.h"

namespace httplib {
class Client;
}

namespace WebService {

class Client {
public:
    Client(const std::string& host, const std::string& username, const std::string& token);

    /**
     * Posts JSON to the specified path.
     * @param path the URL segment after the host address.
     * @param data String of JSON data to use for the body of the POST request.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult PostJson(const std::string& path, const std::string& data,
                               bool allow_anonymous) {
        return GenericJson("POST", path, data, allow_anonymous);
    }

    /**
     * Gets JSON from the specified path.
     * @param path the URL segment after the host address.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult GetJson(const std::string& path, bool allow_anonymous) {
        return GenericJson("GET", path, "", allow_anonymous);
    }

    /**
     * Deletes JSON to the specified path.
     * @param path the URL segment after the host address.
     * @param data String of JSON data to use for the body of the DELETE request.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult DeleteJson(const std::string& path, const std::string& data,
                                 bool allow_anonymous) {
        return GenericJson("DELETE", path, data, allow_anonymous);
    }

private:
    /// A generic function handles POST, GET and DELETE request together
    Common::WebResult GenericJson(const std::string& method, const std::string& path,
                                  const std::string& data, bool allow_anonymous);

    /**
     * A generic function with explicit authentication method specified
     * JWT is used if the jwt parameter is not empty
     * username + token is used if jwt is empty but username and token are not empty
     * anonymous if all of jwt, username and token are empty
     */
    Common::WebResult GenericJson(const std::string& method, const std::string& path,
                                  const std::string& data, const std::string& jwt = "",
                                  const std::string& username = "", const std::string& token = "");

    // Retrieve a new JWT from given username and token
    void UpdateJWT();

    std::string host;
    std::string username;
    std::string token;
    std::string jwt;
    std::unique_ptr<httplib::Client> cli;

    struct JWTCache {
        std::mutex mutex;
        std::string username;
        std::string token;
        std::string jwt;
    };
    static JWTCache jwt_cache;
};

} // namespace WebService
