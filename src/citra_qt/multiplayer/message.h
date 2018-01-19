// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace NetworkMessage {

class ConnectionError {

public:
    explicit ConnectionError(const std::string& str) : err(str) {}
    const std::string& GetString() const {
        return err;
    }

private:
    std::string err;
};

extern const ConnectionError USERNAME_NOT_VALID;
extern const ConnectionError ROOMNAME_NOT_VALID;
extern const ConnectionError USERNAME_IN_USE;
extern const ConnectionError IP_ADDRESS_NOT_VALID;
extern const ConnectionError PORT_NOT_VALID;
extern const ConnectionError NO_INTERNET;
extern const ConnectionError UNABLE_TO_CONNECT;
extern const ConnectionError COULD_NOT_CREATE_ROOM;
extern const ConnectionError HOST_BANNED;
extern const ConnectionError WRONG_VERSION;
extern const ConnectionError WRONG_PASSWORD;
extern const ConnectionError GENERIC_ERROR;
extern const ConnectionError LOST_CONNECTION;
extern const ConnectionError MAC_COLLISION;

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
