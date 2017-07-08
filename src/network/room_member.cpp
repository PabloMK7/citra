// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <mutex>
#include <thread>
#include "common/assert.h"
#include "enet/enet.h"
#include "network/packet.h"
#include "network/room_member.h"

namespace Network {

constexpr u32 ConnectionTimeoutMs = 5000;

class RoomMember::RoomMemberImpl {
public:
    ENetHost* client = nullptr; ///< ENet network interface.
    ENetPeer* server = nullptr; ///< The server peer the client is connected to

    /// Information about the clients connected to the same room as us.
    MemberList member_information;
    /// Information about the room we're connected to.
    RoomInformation room_information;

    std::atomic<State> state{State::Idle}; ///< Current state of the RoomMember.
    void SetState(const State new_state);
    bool IsConnected() const;

    std::string nickname;   ///< The nickname of this member.
    MacAddress mac_address; ///< The mac_address of this member.

    std::mutex network_mutex; ///< Mutex that controls access to the `client` variable.
    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> receive_thread;
    void ReceiveLoop();
    void StartLoop();

    /**
     * Sends data to the room. It will be send on channel 0 with flag RELIABLE
     * @param packet The data to send
     */
    void Send(Packet& packet);
    /**
     * Sends a request to the server, asking for permission to join a room with the specified
     * nickname and preferred mac.
     * @params nickname The desired nickname.
     * @params preferred_mac The preferred MAC address to use in the room, the NoPreferredMac tells
     * the server to assign one for us.
     */
    void SendJoinRequest(const std::string& nickname,
                         const MacAddress& preferred_mac = NoPreferredMac);

    /**
     * Extracts a MAC Address from a received ENet packet.
     * @param event The ENet event that was received.
     */
    void HandleJoinPacket(const ENetEvent* event);
    /**
     * Extracts RoomInformation and MemberInformation from a received RakNet packet.
     * @param event The ENet event that was received.
     */
    void HandleRoomInformationPacket(const ENetEvent* event);
};

// RoomMemberImpl
void RoomMember::RoomMemberImpl::SetState(const State new_state) {
    state = new_state;
    // TODO(B3N30): Invoke the callback functions
}

bool RoomMember::RoomMemberImpl::IsConnected() const {
    return state == State::Joining || state == State::Joined;
}

void RoomMember::RoomMemberImpl::ReceiveLoop() {
    // Receive packets while the connection is open
    while (IsConnected()) {
        std::lock_guard<std::mutex> lock(network_mutex);
        ENetEvent event;
        if (enet_host_service(client, &event, 1000) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                switch (event.packet->data[0]) {
                // TODO(B3N30): Handle the other message types
                case IdRoomInformation:
                    HandleRoomInformationPacket(&event);
                    break;
                case IdJoinSuccess:
                    // The join request was successful, we are now in the room.
                    // If we joined successfully, there must be at least one client in the room: us.
                    ASSERT_MSG(member_information.size() > 0,
                               "We have not yet received member information.");
                    HandleJoinPacket(&event); // Get the MAC Address for the client
                    SetState(State::Joined);
                    break;
                case IdNameCollision:
                    SetState(State::NameCollision);
                    enet_packet_destroy(event.packet);
                    enet_peer_disconnect(server, 0);
                    enet_peer_reset(server);
                    return;
                    break;
                case IdMacCollision:
                    SetState(State::MacCollision);
                    enet_packet_destroy(event.packet);
                    enet_peer_disconnect(server, 0);
                    enet_peer_reset(server);
                    return;
                    break;
                default:
                    break;
                }
                enet_packet_destroy(event.packet);
            }
        }
    }
};

void RoomMember::RoomMemberImpl::StartLoop() {
    receive_thread = std::make_unique<std::thread>(&RoomMember::RoomMemberImpl::ReceiveLoop, this);
}

void RoomMember::RoomMemberImpl::Send(Packet& packet) {
    ENetPacket* enetPacket =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(server, 0, enetPacket);
    enet_host_flush(client);
}

void RoomMember::RoomMemberImpl::SendJoinRequest(const std::string& nickname,
                                                 const MacAddress& preferred_mac) {
    Packet packet;
    packet << static_cast<MessageID>(IdJoinRequest);
    packet << nickname;
    packet << preferred_mac;
    Send(packet);
}

void RoomMember::RoomMemberImpl::HandleRoomInformationPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(MessageID));

    RoomInformation info{};
    packet >> info.name;
    packet >> info.member_slots;
    room_information.name = info.name;
    room_information.member_slots = info.member_slots;

    uint32_t num_members;
    packet >> num_members;
    member_information.resize(num_members);

    for (auto& member : member_information) {
        packet >> member.nickname;
        packet >> member.mac_address;
        packet >> member.game_name;
    }
    // TODO(B3N30): Invoke callbacks
}

void RoomMember::RoomMemberImpl::HandleJoinPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(MessageID));

    // Parse the MAC Address from the BitStream
    packet >> mac_address;
    // TODO(B3N30): Invoke callbacks
}

// RoomMember
RoomMember::RoomMember() : room_member_impl{std::make_unique<RoomMemberImpl>()} {
    room_member_impl->client = enet_host_create(nullptr, 1, NumChannels, 0, 0);
    ASSERT_MSG(room_member_impl->client != nullptr, "Could not create client");
}

RoomMember::~RoomMember() {
    ASSERT_MSG(!IsConnected(), "RoomMember is being destroyed while connected");
    enet_host_destroy(room_member_impl->client);
}

RoomMember::State RoomMember::GetState() const {
    return room_member_impl->state;
}

const RoomMember::MemberList& RoomMember::GetMemberInformation() const {
    return room_member_impl->member_information;
}

RoomInformation RoomMember::GetRoomInformation() const {
    return room_member_impl->room_information;
}

void RoomMember::Join(const std::string& nick, const char* server_addr, u16 server_port,
                      u16 client_port) {
    ENetAddress address{};
    enet_address_set_host(&address, server_addr);
    address.port = server_port;

    room_member_impl->server =
        enet_host_connect(room_member_impl->client, &address, NumChannels, 0);

    if (!room_member_impl->server) {
        room_member_impl->SetState(State::Error);
        return;
    }

    ENetEvent event{};
    int net = enet_host_service(room_member_impl->client, &event, ConnectionTimeoutMs);
    if (net > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        room_member_impl->nickname = nick;
        room_member_impl->SetState(State::Joining);
        room_member_impl->StartLoop();
        room_member_impl->SendJoinRequest(nick);
    } else {
        room_member_impl->SetState(State::CouldNotConnect);
    }
}

bool RoomMember::IsConnected() const {
    return room_member_impl->IsConnected();
}

void RoomMember::Leave() {
    ASSERT_MSG(room_member_impl->receive_thread != nullptr, "Must be in a room to leave it.");
    {
        std::lock_guard<std::mutex> lock(room_member_impl->network_mutex);
        enet_peer_disconnect(room_member_impl->server, 0);
        room_member_impl->SetState(State::Idle);
    }
    room_member_impl->receive_thread->join();
    room_member_impl->receive_thread.reset();
    enet_peer_reset(room_member_impl->server);
}

} // namespace Network
