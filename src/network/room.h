// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"

namespace Network {

constexpr u16 DefaultRoomPort = 1234;
constexpr size_t NumChannels = 1; // Number of channels used for the connection

struct RoomInformation {
    std::string name; ///< Name of the server
    u32 member_slots; ///< Maximum number of members in this room
};

// The different types of messages that can be sent. The first byte of each packet defines the type
typedef uint8_t MessageID;
enum RoomMessageTypes {
    IdJoinRequest = 1,
    IdJoinSuccess,
    IdRoomInformation,
    IdSetGameName,
    IdWifiPacket,
    IdChatMessage,
    IdNameCollision,
    IdMacCollision
};

/// This is what a server [person creating a server] would use.
class Room final {
public:
    enum class State : u8 {
        Open,   ///< The room is open and ready to accept connections.
        Closed, ///< The room is not opened and can not accept connections.
    };

    Room();
    ~Room();

    /**
     * Gets the current state of the room.
     */
    State GetState() const;

    /**
     * Gets the room information of the room.
     */
    const RoomInformation& GetRoomInformation() const;

    /**
     * Creates the socket for this room. Will bind to default address if
     * server is empty string.
     */
    void Create(const std::string& name, const std::string& server = "",
                u16 server_port = DefaultRoomPort);

    /**
     * Destroys the socket
     */
    void Destroy();

private:
    class RoomImpl;
    std::unique_ptr<RoomImpl> room_impl;
};

} // namespace Network
