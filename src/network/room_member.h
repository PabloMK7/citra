// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "network/room.h"

namespace Network {

/// Information about the received WiFi packets.
/// Acts as our own 802.11 header.
struct WifiPacket {
    enum class PacketType { Beacon, Data, Management };
    PacketType type;           ///< The type of 802.11 frame, Beacon / Data.
    std::vector<uint8_t> data; ///< Raw 802.11 frame data, starting at the management frame header
                               /// for management frames.
    MacAddress transmitter_address; ///< Mac address of the transmitter.
    MacAddress destination_address; ///< Mac address of the receiver.
    uint8_t channel;                ///< WiFi channel where this frame was transmitted.
};

/// Represents a chat message.
struct ChatEntry {
    std::string nickname; ///< Nickname of the client who sent this message.
    std::string message;  ///< Body of the message.
};

/**
 * This is what a client [person joining a server] would use.
 * It also has to be used if you host a game yourself (You'd create both, a Room and a
 * RoomMembership for yourself)
 */
class RoomMember final {
public:
    enum class State : u8 {
        Idle,    ///< Default state
        Error,   ///< Some error [permissions to network device missing or something]
        Joining, ///< The client is attempting to join a room.
        Joined,  ///< The client is connected to the room and is ready to send/receive packets.
        LostConnection, ///< Connection closed

        // Reasons why connection was rejected
        NameCollision,  ///< Somebody is already using this name
        MacCollision,   ///< Somebody is already using that mac-address
        CouldNotConnect ///< The room is not responding to a connection attempt
    };

    struct MemberInformation {
        std::string nickname;   ///< Nickname of the member.
        std::string game_name;  ///< Name of the game they're currently playing, or empty if they're
                                /// not playing anything.
        MacAddress mac_address; ///< MAC address associated with this member.
    };
    using MemberList = std::vector<MemberInformation>;

    RoomMember();
    ~RoomMember();

    /**
     * Returns the status of our connection to the room.
     */
    State GetState() const;

    /**
     * Returns information about the members in the room we're currently connected to.
     */
    const MemberList& GetMemberInformation() const;
    /**
     * Returns information about the room we're currently connected to.
     */
    RoomInformation GetRoomInformation() const;

    /**
     * Returns whether we're connected to a server or not.
     */
    bool IsConnected() const;

    /**
     * Attempts to join a room at the specified address and port, using the specified nickname.
     * This may fail if the username is already taken.
     */
    void Join(const std::string& nickname, const char* server_addr = "127.0.0.1",
              const u16 serverPort = DefaultRoomPort, const u16 clientPort = 0);

    /**
     * Sends a WiFi packet to the room.
     * @param packet The WiFi packet to send.
     */
    void SendWifiPacket(const WifiPacket& packet);

    /**
     * Sends a chat message to the room.
     * @param message The contents of the message.
     */
    void SendChatMessage(const std::string& message);

    /**
     * Leaves the current room.
     */
    void Leave();

private:
    class RoomMemberImpl;
    std::unique_ptr<RoomMemberImpl> room_member_impl;
};

} // namespace Network
