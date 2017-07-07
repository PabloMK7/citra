// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "enet/enet.h"
#include "network/room_member.h"

namespace Network {

constexpr u32 ConnectionTimeoutMs = 5000;

class RoomMember::RoomMemberImpl {
public:
    ENetHost* client = nullptr; ///< ENet network interface.
    ENetPeer* server = nullptr; ///< The server peer the client is connected to

    std::atomic<State> state{State::Idle}; ///< Current state of the RoomMember.

    std::string nickname; ///< The nickname of this member.
};

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

void RoomMember::Join(const std::string& nick, const char* server_addr, u16 server_port,
                      u16 client_port) {
    ENetAddress address{};
    enet_address_set_host(&address, server_addr);
    address.port = server_port;

    room_member_impl->server =
        enet_host_connect(room_member_impl->client, &address, NumChannels, 0);

    if (!room_member_impl->server) {
        room_member_impl->state = State::Error;
        return;
    }

    ENetEvent event{};
    int net = enet_host_service(room_member_impl->client, &event, ConnectionTimeoutMs);
    if (net > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        room_member_impl->nickname = nick;
        room_member_impl->state = State::Joining;
        // TODO(B3N30): Send a join request with the nickname to the server
        // TODO(B3N30): Start the receive thread
    } else {
        room_member_impl->state = State::CouldNotConnect;
    }
}

bool RoomMember::IsConnected() const {
    return room_member_impl->state == State::Joining || room_member_impl->state == State::Joined;
}

void RoomMember::Leave() {
    enet_peer_disconnect(room_member_impl->server, 0);
    room_member_impl->state = State::Idle;
    // TODO(B3N30): Close the receive thread
    enet_peer_reset(room_member_impl->server);
}

} // namespace Network
