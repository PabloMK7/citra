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

    /**
     * Extracts a WifiPacket from a received ENet packet.
     * @param event The  ENet event that was received.
     */
    void HandleWifiPackets(const ENetEvent* event);

    /**
     * Extracts a chat entry from a received ENet packet and adds it to the chat queue.
     * @param event The ENet event that was received.
     */
    void HandleChatPacket(const ENetEvent* event);

    /**
     * Disconnects the RoomMember from the Room
     */
    void Disconnect();
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
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                // TODO(B3N30): Handle the other message types
                case IdChatMessage:
                    HandleChatPacket(&event);
                    break;
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
                    break;
                case IdMacCollision:
                    SetState(State::MacCollision);
                    break;
                default:
                    break;
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                SetState(State::LostConnection);
            }
        }
    }
    Disconnect();
};

void RoomMember::RoomMemberImpl::StartLoop() {
    receive_thread = std::make_unique<std::thread>(&RoomMember::RoomMemberImpl::ReceiveLoop, this);
}

void RoomMember::RoomMemberImpl::Send(Packet& packet) {
    std::lock_guard<std::mutex> lock(network_mutex);
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

    u32 num_members;
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

void RoomMember::RoomMemberImpl::HandleWifiPackets(const ENetEvent* event) {
    WifiPacket wifi_packet{};
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(MessageID));

    // Parse the WifiPacket from the BitStream
    u8 frame_type;
    packet >> frame_type;
    WifiPacket::PacketType type = static_cast<WifiPacket::PacketType>(frame_type);

    wifi_packet.type = type;
    packet >> wifi_packet.channel;
    packet >> wifi_packet.transmitter_address;
    packet >> wifi_packet.destination_address;

    u32 data_length;
    packet >> data_length;

    std::vector<u8> data(data_length);
    packet >> data;

    wifi_packet.data = std::move(data);
    // TODO(B3N30): Invoke callbacks
}

void RoomMember::RoomMemberImpl::HandleChatPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(MessageID));

    ChatEntry chat_entry{};
    packet >> chat_entry.nickname;
    packet >> chat_entry.message;
    // TODO(B3N30): Invoke callbacks
}

void RoomMember::RoomMemberImpl::Disconnect() {
    member_information.clear();
    room_information.member_slots = 0;
    room_information.name.clear();

    if (server) {
        enet_peer_disconnect(server, 0);
    } else {
        return;
    }

    ENetEvent event;
    while (enet_host_service(client, &event, ConnectionTimeoutMs) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet); // Ignore all incoming data
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            server = nullptr;
            return;
        }
    }
    // didn't disconnect gracefully force disconnect
    enet_peer_reset(server);
    server = nullptr;
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

const std::string& RoomMember::GetNickname() const {
    return room_member_impl->nickname;
}

const MacAddress& RoomMember::GetMacAddress() const {
    if (GetState() == State::Joined)
        return room_member_impl->mac_address;
    return MacAddress{};
}

RoomInformation RoomMember::GetRoomInformation() const {
    return room_member_impl->room_information;
}

void RoomMember::Join(const std::string& nick, const char* server_addr, u16 server_port,
                      u16 client_port) {
    // If the member is connected, kill the connection first
    if (room_member_impl->receive_thread && room_member_impl->receive_thread->joinable()) {
        room_member_impl->SetState(State::Error);
        room_member_impl->receive_thread->join();
        room_member_impl->receive_thread.reset();
    }
    // If the thread isn't running but the ptr still exists, reset it
    else if (room_member_impl->receive_thread) {
        room_member_impl->receive_thread.reset();
    }

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

void RoomMember::SendWifiPacket(const WifiPacket& wifi_packet) {
    Packet packet;
    packet << static_cast<MessageID>(IdWifiPacket);
    packet << static_cast<u8>(wifi_packet.type);
    packet << wifi_packet.channel;
    packet << wifi_packet.transmitter_address;
    packet << wifi_packet.destination_address;
    packet << static_cast<u32>(wifi_packet.data.size());
    packet << wifi_packet.data;
    room_member_impl->Send(packet);
}

void RoomMember::SendChatMessage(const std::string& message) {
    Packet packet;
    packet << static_cast<MessageID>(IdChatMessage);
    packet << message;
    room_member_impl->Send(packet);
}

void RoomMember::SendGameName(const std::string& game_name) {
    Packet packet;
    packet << static_cast<MessageID>(IdSetGameName);
    packet << game_name;
    room_member_impl->Send(packet);
}

void RoomMember::Leave() {
    room_member_impl->SetState(State::Idle);
    room_member_impl->receive_thread->join();
    room_member_impl->receive_thread.reset();
}

} // namespace Network
