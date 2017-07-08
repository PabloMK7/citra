// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <random>
#include <thread>
#include <vector>
#include "enet/enet.h"
#include "network/packet.h"
#include "network/room.h"

namespace Network {

/// Maximum number of concurrent connections allowed to this room.
static constexpr u32 MaxConcurrentConnections = 10;

class Room::RoomImpl {
public:
    // This MAC address is used to generate a 'Nintendo' like Mac address.
    const MacAddress NintendoOUI = {0x00, 0x1F, 0x32, 0x00, 0x00, 0x00};
    std::mt19937 random_gen; ///< Random number generator. Used for GenerateMacAddress

    ENetHost* server = nullptr; ///< Network interface.

    std::atomic<State> state{State::Closed}; ///< Current state of the room.
    RoomInformation room_information;        ///< Information about this room.

    struct Member {
        std::string nickname;   ///< The nickname of the member.
        std::string game_name;  ///< The current game of the member
        MacAddress mac_address; ///< The assigned mac address of the member.
        ENetPeer* peer;         ///< The remote peer.
    };
    using MemberList = std::vector<Member>;
    MemberList members; ///< Information about the members of this room.

    RoomImpl() : random_gen(std::random_device()()) {}

    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> room_thread;

    /// Thread function that will receive and dispatch messages until the room is destroyed.
    void ServerLoop();
    void StartLoop();

    /**
     * Parses and answers a room join request from a client.
     * Validates the uniqueness of the username and assigns the MAC address
     * that the client will use for the remainder of the connection.
     */
    void HandleJoinRequest(const ENetEvent* event);

    /**
     * Returns whether the nickname is valid, ie. isn't already taken by someone else in the room.
     */
    bool IsValidNickname(const std::string& nickname) const;

    /**
     * Returns whether the MAC address is valid, ie. isn't already taken by someone else in the
     * room.
     */
    bool IsValidMacAddress(const MacAddress& address) const;

    /**
     * Sends a ID_ROOM_NAME_COLLISION message telling the client that the name is invalid.
     */
    void SendNameCollision(ENetPeer* client);

    /**
     * Sends a ID_ROOM_MAC_COLLISION message telling the client that the MAC is invalid.
     */
    void SendMacCollision(ENetPeer* client);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room.
     */
    void SendJoinSuccess(ENetPeer* client, MacAddress mac_address);

    /**
     * Sends the information about the room, along with the list of members
     * to every connected client in the room.
     * The packet has the structure:
     * <MessageID>ID_ROOM_INFORMATION
     * <String> room_name
     * <uint32_t> member_slots: The max number of clients allowed in this room
     * <uint32_t> num_members: the number of currently joined clients
     * This is followed by the following three values for each member:
     * <String> nickname of that member
     * <MacAddress> mac_address of that member
     * <String> game_name of that member
     */
    void BroadcastRoomInformation();

    /**
     * Generates a free MAC address to assign to a new client.
     * The first 3 bytes are the NintendoOUI 0x00, 0x1F, 0x32
     */
    MacAddress GenerateMacAddress();
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (state != State::Closed) {
        ENetEvent event;
        if (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                case IdJoinRequest:
                    HandleJoinRequest(&event);
                    break;
                    // TODO(B3N30): Handle the other message types
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                // TODO(B3N30): Handle the disconnect from a client
                break;
            }
        }
    }
}

void Room::RoomImpl::StartLoop() {
    room_thread = std::make_unique<std::thread>(&Room::RoomImpl::ServerLoop, this);
}

void Room::RoomImpl::HandleJoinRequest(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(MessageID));
    std::string nickname;
    packet >> nickname;

    MacAddress preferred_mac;
    packet >> preferred_mac;

    if (!IsValidNickname(nickname)) {
        SendNameCollision(event->peer);
        return;
    }

    if (preferred_mac != NoPreferredMac) {
        // Verify if the preferred mac is available
        if (!IsValidMacAddress(preferred_mac)) {
            SendMacCollision(event->peer);
            return;
        }
    } else {
        // Assign a MAC address of this client automatically
        preferred_mac = GenerateMacAddress();
    }

    // At this point the client is ready to be added to the room.
    Member member{};
    member.mac_address = preferred_mac;
    member.nickname = nickname;
    member.peer = event->peer;

    members.push_back(member);

    // Notify everyone that the room information has changed.
    BroadcastRoomInformation();
    SendJoinSuccess(event->peer, preferred_mac);
}

bool Room::RoomImpl::IsValidNickname(const std::string& nickname) const {
    // A nickname is valid if it is not already taken by anybody else in the room.
    // TODO(B3N30): Check for empty names, spaces, etc.
    for (const Member& member : members) {
        if (member.nickname == nickname) {
            return false;
        }
    }
    return true;
}

bool Room::RoomImpl::IsValidMacAddress(const MacAddress& address) const {
    // A MAC address is valid if it is not already taken by anybody else in the room.
    for (const Member& member : members) {
        if (member.mac_address == address) {
            return false;
        }
    }
    return true;
}

void Room::RoomImpl::SendNameCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<MessageID>(IdNameCollision);

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendMacCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<MessageID>(IdMacCollision);

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendJoinSuccess(ENetPeer* client, MacAddress mac_address) {
    Packet packet;
    packet << static_cast<MessageID>(IdJoinSuccess);
    packet << mac_address;
    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::BroadcastRoomInformation() {
    Packet packet;
    packet << static_cast<MessageID>(IdRoomInformation);
    packet << room_information.name;
    packet << room_information.member_slots;

    packet << static_cast<uint32_t>(members.size());
    for (const auto& member : members) {
        packet << member.nickname;
        packet << member.mac_address;
        packet << member.game_name;
    }

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, enet_packet);
    enet_host_flush(server);
}

MacAddress Room::RoomImpl::GenerateMacAddress() {
    MacAddress result_mac = NintendoOUI;
    std::uniform_int_distribution<> dis(0x00, 0xFF); // Random byte between 0 and 0xFF
    do {
        for (int i = 3; i < result_mac.size(); ++i) {
            result_mac[i] = dis(random_gen);
        }
    } while (!IsValidMacAddress(result_mac));
    return result_mac;
}

// Room
Room::Room() : room_impl{std::make_unique<RoomImpl>()} {}

Room::~Room() = default;

void Room::Create(const std::string& name, const std::string& server_address, u16 server_port) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    enet_address_set_host(&address, server_address.c_str());
    address.port = server_port;

    room_impl->server = enet_host_create(&address, MaxConcurrentConnections, NumChannels, 0, 0);
    // TODO(B3N30): Allow specifying the maximum number of concurrent connections.
    room_impl->state = State::Open;

    room_impl->room_information.name = name;
    room_impl->room_information.member_slots = MaxConcurrentConnections;
    room_impl->StartLoop();
}

Room::State Room::GetState() const {
    return room_impl->state;
}

const RoomInformation& Room::GetRoomInformation() const {
    return room_impl->room_information;
}

void Room::Destroy() {
    room_impl->state = State::Closed;
    room_impl->room_thread->join();
    room_impl->room_thread.reset();

    if (room_impl->server) {
        enet_host_destroy(room_impl->server);
    }
    room_impl->room_information = {};
    room_impl->server = nullptr;
}

} // namespace Network
