// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <thread>
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

    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> room_thread;

    /// Thread function that will receive and dispatch messages until the room is destroyed.
    void ServerLoop();
    void StartLoop();
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (state != State::Closed) {
        ENetEvent event;
        if (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                // TODO(B3N30): Select the type of message and handle it
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
