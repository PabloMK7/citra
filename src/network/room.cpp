// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <thread>
#include "enet/enet.h"
#include "network/packet.h"
#include "network/room.h"

namespace Network {

/// Maximum number of concurrent connections allowed to this room.
static constexpr u32 MaxConcurrentConnections = 10;

class Room::RoomImpl {
public:
    // This MAC address is used to generate a 'Nintendo' like Mac address.
    const MacAddress NintendoOUI;
    std::mt19937 random_gen; ///< Random number generator. Used for GenerateMacAddress

    ENetHost* server = nullptr; ///< Network interface.

    std::atomic<State> state{State::Closed}; ///< Current state of the room.
    RoomInformation room_information;        ///< Information about this room.

    struct Member {
        std::string nickname;   ///< The nickname of the member.
        GameInfo game_info;     ///< The current game of the member
        MacAddress mac_address; ///< The assigned mac address of the member.
        ENetPeer* peer;         ///< The remote peer.
    };
    using MemberList = std::vector<Member>;
    MemberList members;              ///< Information about the members of this room
    mutable std::mutex member_mutex; ///< Mutex for locking the members list
    /// This should be a std::shared_mutex as soon as C++17 is supported

    RoomImpl()
        : random_gen(std::random_device()()), NintendoOUI{0x00, 0x1F, 0x32, 0x00, 0x00, 0x00} {}

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
     * Sends a ID_ROOM_VERSION_MISMATCH message telling the client that the version is invalid.
     */
    void SendVersionMismatch(ENetPeer* client);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room.
     */
    void SendJoinSuccess(ENetPeer* client, MacAddress mac_address);

    /**
     * Notifies the members that the room is closed,
     */
    void SendCloseMessage();

    /**
     * Sends the information about the room, along with the list of members
     * to every connected client in the room.
     * The packet has the structure:
     * <MessageID>ID_ROOM_INFORMATION
     * <String> room_name
     * <u32> member_slots: The max number of clients allowed in this room
     * <u32> num_members: the number of currently joined clients
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

    /**
     * Broadcasts this packet to all members except the sender.
     * @param event The ENet event containing the data
     */
    void HandleWifiPacket(const ENetEvent* event);

    /**
     * Extracts a chat entry from a received ENet packet and adds it to the chat queue.
     * @param event The ENet event that was received.
     */
    void HandleChatPacket(const ENetEvent* event);

    /**
     * Extracts the game name from a received ENet packet and broadcasts it.
     * @param event The ENet event that was received.
     */
    void HandleGameNamePacket(const ENetEvent* event);

    /**
     * Removes the client from the members list if it was in it and announces the change
     * to all other clients.
     */
    void HandleClientDisconnection(ENetPeer* client);
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (state != State::Closed) {
        ENetEvent event;
        if (enet_host_service(server, &event, 100) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                case IdJoinRequest:
                    HandleJoinRequest(&event);
                    break;
                case IdSetGameInfo:
                    HandleGameNamePacket(&event);
                    break;
                case IdWifiPacket:
                    HandleWifiPacket(&event);
                    break;
                case IdChatMessage:
                    HandleChatPacket(&event);
                    break;
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                HandleClientDisconnection(event.peer);
                break;
            }
        }
    }
    // Close the connection to all members:
    SendCloseMessage();
}

void Room::RoomImpl::StartLoop() {
    room_thread = std::make_unique<std::thread>(&Room::RoomImpl::ServerLoop, this);
}

void Room::RoomImpl::HandleJoinRequest(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Igonore the message type
    std::string nickname;
    packet >> nickname;

    MacAddress preferred_mac;
    packet >> preferred_mac;

    u32 client_version;
    packet >> client_version;

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

    if (client_version != network_version) {
        SendVersionMismatch(event->peer);
        return;
    }

    // At this point the client is ready to be added to the room.
    Member member{};
    member.mac_address = preferred_mac;
    member.nickname = nickname;
    member.peer = event->peer;

    {
        std::lock_guard<std::mutex> lock(member_mutex);
        members.push_back(std::move(member));
    }

    // Notify everyone that the room information has changed.
    BroadcastRoomInformation();
    SendJoinSuccess(event->peer, preferred_mac);
}

bool Room::RoomImpl::IsValidNickname(const std::string& nickname) const {
    // A nickname is valid if it is not already taken by anybody else in the room.
    // TODO(B3N30): Check for empty names, spaces, etc.
    std::lock_guard<std::mutex> lock(member_mutex);
    return std::all_of(members.begin(), members.end(),
                       [&nickname](const auto& member) { return member.nickname != nickname; });
}

bool Room::RoomImpl::IsValidMacAddress(const MacAddress& address) const {
    // A MAC address is valid if it is not already taken by anybody else in the room.
    std::lock_guard<std::mutex> lock(member_mutex);
    return std::all_of(members.begin(), members.end(),
                       [&address](const auto& member) { return member.mac_address != address; });
}

void Room::RoomImpl::SendNameCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IdNameCollision);

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendMacCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IdMacCollision);

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendVersionMismatch(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IdVersionMismatch);
    packet << network_version;

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendJoinSuccess(ENetPeer* client, MacAddress mac_address) {
    Packet packet;
    packet << static_cast<u8>(IdJoinSuccess);
    packet << mac_address;
    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendCloseMessage() {
    Packet packet;
    packet << static_cast<u8>(IdCloseRoom);
    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    std::lock_guard<std::mutex> lock(member_mutex);
    for (auto& member : members) {
        enet_peer_send(member.peer, 0, enet_packet);
    }
    enet_host_flush(server);
    for (auto& member : members) {
        enet_peer_disconnect(member.peer, 0);
    }
}

void Room::RoomImpl::BroadcastRoomInformation() {
    Packet packet;
    packet << static_cast<u8>(IdRoomInformation);
    packet << room_information.name;
    packet << room_information.member_slots;

    packet << static_cast<u32>(members.size());
    {
        std::lock_guard<std::mutex> lock(member_mutex);
        for (const auto& member : members) {
            packet << member.nickname;
            packet << member.mac_address;
            packet << member.game_info.name;
            packet << member.game_info.id;
        }
    }

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, enet_packet);
    enet_host_flush(server);
}

MacAddress Room::RoomImpl::GenerateMacAddress() {
    MacAddress result_mac =
        NintendoOUI; // The first three bytes of each MAC address will be the NintendoOUI
    std::uniform_int_distribution<> dis(0x00, 0xFF); // Random byte between 0 and 0xFF
    do {
        for (size_t i = 3; i < result_mac.size(); ++i) {
            result_mac[i] = dis(random_gen);
        }
    } while (!IsValidMacAddress(result_mac));
    return result_mac;
}

void Room::RoomImpl::HandleWifiPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);
    in_packet.IgnoreBytes(sizeof(u8));         // Message type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Channel
    in_packet.IgnoreBytes(sizeof(MacAddress)); // WifiPacket Transmitter Address
    MacAddress destination_address;
    in_packet >> destination_address;

    Packet out_packet;
    out_packet.Append(event->packet->data, event->packet->dataLength);
    ENetPacket* enet_packet = enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                                 ENET_PACKET_FLAG_RELIABLE);

    if (destination_address == BroadcastMac) { // Send the data to everyone except the sender
        std::lock_guard<std::mutex> lock(member_mutex);
        for (const auto& member : members) {
            if (member.peer != event->peer)
                enet_peer_send(member.peer, 0, enet_packet);
        }
    } else { // Send the data only to the destination client
        std::lock_guard<std::mutex> lock(member_mutex);
        auto member = std::find_if(members.begin(), members.end(),
                                   [destination_address](const Member& member) -> bool {
                                       return member.mac_address == destination_address;
                                   });
        if (member != members.end()) {
            enet_peer_send(member->peer, 0, enet_packet);
        }
    }
    enet_host_flush(server);
}

void Room::RoomImpl::HandleChatPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);

    in_packet.IgnoreBytes(sizeof(u8)); // Igonore the message type
    std::string message;
    in_packet >> message;
    auto CompareNetworkAddress = [event](const Member member) -> bool {
        return member.peer == event->peer;
    };

    std::lock_guard<std::mutex> lock(member_mutex);
    const auto sending_member = std::find_if(members.begin(), members.end(), CompareNetworkAddress);
    if (sending_member == members.end()) {
        return; // Received a chat message from a unknown sender
    }

    Packet out_packet;
    out_packet << static_cast<u8>(IdChatMessage);
    out_packet << sending_member->nickname;
    out_packet << message;

    ENetPacket* enet_packet = enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                                 ENET_PACKET_FLAG_RELIABLE);
    for (const auto& member : members) {
        if (member.peer != event->peer)
            enet_peer_send(member.peer, 0, enet_packet);
    }
    enet_host_flush(server);
}

void Room::RoomImpl::HandleGameNamePacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);

    in_packet.IgnoreBytes(sizeof(u8)); // Igonore the message type
    GameInfo game_info;
    in_packet >> game_info.name;
    in_packet >> game_info.id;

    {
        std::lock_guard<std::mutex> lock(member_mutex);
        auto member =
            std::find_if(members.begin(), members.end(), [event](const Member& member) -> bool {
                return member.peer == event->peer;
            });
        if (member != members.end()) {
            member->game_info = game_info;
        }
    }
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleClientDisconnection(ENetPeer* client) {
    // Remove the client from the members list.
    {
        std::lock_guard<std::mutex> lock(member_mutex);
        members.erase(
            std::remove_if(members.begin(), members.end(),
                           [client](const Member& member) { return member.peer == client; }),
            members.end());
    }

    // Announce the change to all clients.
    enet_peer_disconnect(client, 0);
    BroadcastRoomInformation();
}

// Room
Room::Room() : room_impl{std::make_unique<RoomImpl>()} {}

Room::~Room() = default;

void Room::Create(const std::string& name, const std::string& server_address, u16 server_port) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    if (!server_address.empty()) {
        enet_address_set_host(&address, server_address.c_str());
    }
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

std::vector<Room::Member> Room::GetRoomMemberList() const {
    std::vector<Room::Member> member_list;
    std::lock_guard<std::mutex> lock(room_impl->member_mutex);
    for (const auto& member_impl : room_impl->members) {
        Member member;
        member.nickname = member_impl.nickname;
        member.mac_address = member_impl.mac_address;
        member.game_info = member_impl.game_info;
        member_list.push_back(member);
    }
    return member_list;
};

void Room::Destroy() {
    room_impl->state = State::Closed;
    room_impl->room_thread->join();
    room_impl->room_thread.reset();

    if (room_impl->server) {
        enet_host_destroy(room_impl->server);
    }
    room_impl->room_information = {};
    room_impl->server = nullptr;
    {
        std::lock_guard<std::mutex> lock(room_impl->member_mutex);
        room_impl->members.clear();
    }
    room_impl->room_information.member_slots = 0;
    room_impl->room_information.name.clear();
}

} // namespace Network
