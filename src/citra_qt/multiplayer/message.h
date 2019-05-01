// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>

namespace NetworkMessage {

class ConnectionError {

public:
    explicit ConnectionError(std::string str) : err(std::move(str)) {}
    const std::string& GetString() const {
        return err;
    }

private:
    std::string err;
};

/// When the nickname is considered invalid by the client
extern const ConnectionError USERNAME_NOT_VALID;
extern const ConnectionError ROOMNAME_NOT_VALID;
/// When the nickname is considered invalid by the room server
extern const ConnectionError USERNAME_NOT_VALID_SERVER;
extern const ConnectionError IP_ADDRESS_NOT_VALID;
extern const ConnectionError PORT_NOT_VALID;
extern const ConnectionError GAME_NOT_SELECTED;
extern const ConnectionError NO_INTERNET;
extern const ConnectionError UNABLE_TO_CONNECT;
extern const ConnectionError ROOM_IS_FULL;
extern const ConnectionError COULD_NOT_CREATE_ROOM;
extern const ConnectionError HOST_BANNED;
extern const ConnectionError WRONG_VERSION;
extern const ConnectionError WRONG_PASSWORD;
extern const ConnectionError GENERIC_ERROR;
extern const ConnectionError LOST_CONNECTION;
extern const ConnectionError HOST_KICKED;
extern const ConnectionError MAC_COLLISION;
extern const ConnectionError CONSOLE_ID_COLLISION;
extern const ConnectionError PERMISSION_DENIED;
extern const ConnectionError NO_SUCH_USER;

/**
 *  Shows a standard QMessageBox with a error message
 */
void ShowError(const ConnectionError& e);

/**
 * Show a standard QMessageBox with a warning message about leaving the room
 * return true if the user wants to close the network connection
 */
bool WarnCloseRoom();

/**
 * Show a standard QMessageBox with a warning message about disconnecting from the room
 * return true if the user wants to disconnect
 */
bool WarnDisconnect();

} // namespace NetworkMessage
