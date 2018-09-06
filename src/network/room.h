// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Network {

constexpr u32 network_version = 3; ///< The version of this Room and RoomMember

constexpr u16 DefaultRoomPort = 24872;

constexpr u32 MaxMessageSize = 500;

/// Maximum number of concurrent connections allowed to this room.
static constexpr u32 MaxConcurrentConnections = 254;

constexpr std::size_t NumChannels = 1; // Number of channels used for the connection

struct RoomInformation {
    std::string name;           ///< Name of the server
    u32 member_slots;           ///< Maximum number of members in this room
    std::string uid;            ///< The unique ID of the room
    u16 port;                   ///< The port of this room
    std::string preferred_game; ///< Game to advertise that you want to play
    u64 preferred_game_id;      ///< Title ID for the advertised game
};

struct GameInfo {
    std::string name{""};
    u64 id{0};
};

using MacAddress = std::array<u8, 6>;
/// A special MAC address that tells the room we're joining to assign us a MAC address
/// automatically.
constexpr MacAddress NoPreferredMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 802.11 broadcast MAC address
constexpr MacAddress BroadcastMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// The different types of messages that can be sent. The first byte of each packet defines the type
enum RoomMessageTypes : u8 {
    IdJoinRequest = 1,
    IdJoinSuccess,
    IdRoomInformation,
    IdSetGameInfo,
    IdWifiPacket,
    IdChatMessage,
    IdNameCollision,
    IdMacCollision,
    IdVersionMismatch,
    IdWrongPassword,
    IdCloseRoom
};

/// This is what a server [person creating a server] would use.
class Room final {
public:
    enum class State : u8 {
        Open,   ///< The room is open and ready to accept connections.
        Closed, ///< The room is not opened and can not accept connections.
    };

    struct Member {
        std::string nickname;   ///< The nickname of the member.
        GameInfo game_info;     ///< The current game of the member
        MacAddress mac_address; ///< The assigned mac address of the member.
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
     * Gets a list of the mbmers connected to the room.
     */
    std::vector<Member> GetRoomMemberList() const;

    /**
     * Checks if the room is password protected
     */
    bool HasPassword() const;

    /**
     * Creates the socket for this room. Will bind to default address if
     * server is empty string.
     */
    bool Create(const std::string& name, const std::string& server = "",
                u16 server_port = DefaultRoomPort, const std::string& password = "",
                const u32 max_connections = MaxConcurrentConnections,
                const std::string& preferred_game = "", u64 preferred_game_id = 0);

    /**
     * Destroys the socket
     */
    void Destroy();

private:
    class RoomImpl;
    std::unique_ptr<RoomImpl> room_impl;
};

} // namespace Network
