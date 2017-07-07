// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "enet/enet.h"
#include "network/room.h"

namespace Network {

/// Maximum number of concurrent connections allowed to this room.
static constexpr u32 MaxConcurrentConnections = 10;

class Room::RoomImpl {
public:
    ENetHost* server = nullptr; ///< Network interface.

    std::atomic<State> state{State::Closed}; ///< Current state of the room.
    RoomInformation room_information;        ///< Information about this room.
};

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

    // TODO(B3N30): Start the receiving thread
}

Room::State Room::GetState() const {
    return room_impl->state;
}

const RoomInformation& Room::GetRoomInformation() const {
    return room_impl->room_information;
}

void Room::Destroy() {
    room_impl->state = State::Closed;
    // TODO(B3n30): Join the receiving thread

    if (room_impl->server) {
        enet_host_destroy(room_impl->server);
    }
    room_impl->room_information = {};
    room_impl->server = nullptr;
}

} // namespace Network
