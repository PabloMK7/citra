// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <list>
#include <mutex>
#include <set>
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

    /// The current game name, id and version
    GameInfo current_game_info;

    std::atomic<State> state{State::Idle}; ///< Current state of the RoomMember.
    void SetState(const State new_state);
    bool IsConnected() const;

    std::string nickname;   ///< The nickname of this member.
    MacAddress mac_address; ///< The mac_address of this member.

    std::mutex network_mutex; ///< Mutex that controls access to the `client` variable.
    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> loop_thread;
    std::mutex send_list_mutex;  ///< Mutex that controls access to the `send_list` variable.
    std::list<Packet> send_list; ///< A list that stores all packets to send the async

    template <typename T>
    using CallbackSet = std::set<CallbackHandle<T>>;
    std::mutex callback_mutex; ///< The mutex used for handling callbacks

    class Callbacks {
    public:
        template <typename T>
        CallbackSet<T>& Get();

    private:
        CallbackSet<WifiPacket> callback_set_wifi_packet;
        CallbackSet<ChatEntry> callback_set_chat_messages;
        CallbackSet<RoomInformation> callback_set_room_information;
        CallbackSet<State> callback_set_state;
    };
    Callbacks callbacks; ///< All CallbackSets to all events

    void MemberLoop();

    void StartLoop();

    /**
     * Sends data to the room. It will be send on channel 0 with flag RELIABLE
     * @param packet The data to send
     */
    void Send(Packet&& packet);

    /**
     * Sends a request to the server, asking for permission to join a room with the specified
     * nickname and preferred mac.
     * @params nickname The desired nickname.
     * @params preferred_mac The preferred MAC address to use in the room, the NoPreferredMac tells
     * @params password The password for the room
     * the server to assign one for us.
     */
    void SendJoinRequest(const std::string& nickname,
                         const MacAddress& preferred_mac = NoPreferredMac,
                         const std::string& password = "");

    /**
     * Extracts a MAC Address from a received ENet packet.
     * @param event The ENet event that was received.
     */
    void HandleJoinPacket(const ENetEvent* event);
    /**
     * Extracts RoomInformation and MemberInformation from a received ENet packet.
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

    template <typename T>
    void Invoke(const T& data);

    template <typename T>
    CallbackHandle<T> Bind(std::function<void(const T&)> callback);
};

// RoomMemberImpl
void RoomMember::RoomMemberImpl::SetState(const State new_state) {
    if (state != new_state) {
        state = new_state;
        Invoke<State>(state);
    }
}

bool RoomMember::RoomMemberImpl::IsConnected() const {
    return state == State::Joining || state == State::Joined;
}

void RoomMember::RoomMemberImpl::MemberLoop() {
    // Receive packets while the connection is open
    while (IsConnected()) {
        std::lock_guard<std::mutex> lock(network_mutex);
        ENetEvent event;
        if (enet_host_service(client, &event, 100) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                case IdWifiPacket:
                    HandleWifiPackets(&event);
                    break;
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
                case IdVersionMismatch:
                    SetState(State::WrongVersion);
                    break;
                case IdWrongPassword:
                    SetState(State::WrongPassword);
                    break;
                case IdCloseRoom:
                    SetState(State::LostConnection);
                    break;
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                SetState(State::LostConnection);
                break;
            }
        }
        {
            std::lock_guard<std::mutex> lock(send_list_mutex);
            for (const auto& packet : send_list) {
                ENetPacket* enetPacket = enet_packet_create(packet.GetData(), packet.GetDataSize(),
                                                            ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(server, 0, enetPacket);
            }
            enet_host_flush(client);
            send_list.clear();
        }
    }
    Disconnect();
};

void RoomMember::RoomMemberImpl::StartLoop() {
    loop_thread = std::make_unique<std::thread>(&RoomMember::RoomMemberImpl::MemberLoop, this);
}

void RoomMember::RoomMemberImpl::Send(Packet&& packet) {
    std::lock_guard<std::mutex> lock(send_list_mutex);
    send_list.push_back(std::move(packet));
}

void RoomMember::RoomMemberImpl::SendJoinRequest(const std::string& nickname,
                                                 const MacAddress& preferred_mac,
                                                 const std::string& password) {
    Packet packet;
    packet << static_cast<u8>(IdJoinRequest);
    packet << nickname;
    packet << preferred_mac;
    packet << network_version;
    packet << password;
    Send(std::move(packet));
}

void RoomMember::RoomMemberImpl::HandleRoomInformationPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    RoomInformation info{};
    packet >> info.name;
    packet >> info.member_slots;
    packet >> info.uid;
    packet >> info.port;
    packet >> info.preferred_game;
    room_information.name = info.name;
    room_information.member_slots = info.member_slots;
    room_information.port = info.port;
    room_information.preferred_game = info.preferred_game;

    u32 num_members;
    packet >> num_members;
    member_information.resize(num_members);

    for (auto& member : member_information) {
        packet >> member.nickname;
        packet >> member.mac_address;
        packet >> member.game_info.name;
        packet >> member.game_info.id;
    }
    Invoke(room_information);
}

void RoomMember::RoomMemberImpl::HandleJoinPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    // Parse the MAC Address from the packet
    packet >> mac_address;
    SetState(State::Joined);
}

void RoomMember::RoomMemberImpl::HandleWifiPackets(const ENetEvent* event) {
    WifiPacket wifi_packet{};
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    // Parse the WifiPacket from the packet
    u8 frame_type;
    packet >> frame_type;
    WifiPacket::PacketType type = static_cast<WifiPacket::PacketType>(frame_type);

    wifi_packet.type = type;
    packet >> wifi_packet.channel;
    packet >> wifi_packet.transmitter_address;
    packet >> wifi_packet.destination_address;
    packet >> wifi_packet.data;

    Invoke<WifiPacket>(wifi_packet);
}

void RoomMember::RoomMemberImpl::HandleChatPacket(const ENetEvent* event) {
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);

    // Ignore the first byte, which is the message id.
    packet.IgnoreBytes(sizeof(u8));

    ChatEntry chat_entry{};
    packet >> chat_entry.nickname;
    packet >> chat_entry.message;
    Invoke<ChatEntry>(chat_entry);
}

void RoomMember::RoomMemberImpl::Disconnect() {
    member_information.clear();
    room_information.member_slots = 0;
    room_information.name.clear();

    if (!server)
        return;
    enet_peer_disconnect(server, 0);

    ENetEvent event;
    while (enet_host_service(client, &event, ConnectionTimeoutMs) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet); // Ignore all incoming data
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            server = nullptr;
            return;
        case ENET_EVENT_TYPE_NONE:
        case ENET_EVENT_TYPE_CONNECT:
            break;
        }
    }
    // didn't disconnect gracefully force disconnect
    enet_peer_reset(server);
    server = nullptr;
}

template <>
RoomMember::RoomMemberImpl::CallbackSet<WifiPacket>& RoomMember::RoomMemberImpl::Callbacks::Get() {
    return callback_set_wifi_packet;
}

template <>
RoomMember::RoomMemberImpl::CallbackSet<RoomMember::State>&
RoomMember::RoomMemberImpl::Callbacks::Get() {
    return callback_set_state;
}

template <>
RoomMember::RoomMemberImpl::CallbackSet<RoomInformation>&
RoomMember::RoomMemberImpl::Callbacks::Get() {
    return callback_set_room_information;
}

template <>
RoomMember::RoomMemberImpl::CallbackSet<ChatEntry>& RoomMember::RoomMemberImpl::Callbacks::Get() {
    return callback_set_chat_messages;
}

template <typename T>
void RoomMember::RoomMemberImpl::Invoke(const T& data) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    CallbackSet<T> callback_set = callbacks.Get<T>();
    for (auto const& callback : callback_set)
        (*callback)(data);
}

template <typename T>
RoomMember::CallbackHandle<T> RoomMember::RoomMemberImpl::Bind(
    std::function<void(const T&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    CallbackHandle<T> handle;
    handle = std::make_shared<std::function<void(const T&)>>(callback);
    callbacks.Get<T>().insert(handle);
    return handle;
}

// RoomMember
RoomMember::RoomMember() : room_member_impl{std::make_unique<RoomMemberImpl>()} {}

RoomMember::~RoomMember() {
    ASSERT_MSG(!IsConnected(), "RoomMember is being destroyed while connected");
    if (room_member_impl->loop_thread) {
        Leave();
    }
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
    ASSERT_MSG(IsConnected(), "Tried to get MAC address while not connected");
    return room_member_impl->mac_address;
}

RoomInformation RoomMember::GetRoomInformation() const {
    return room_member_impl->room_information;
}

void RoomMember::Join(const std::string& nick, const char* server_addr, u16 server_port,
                      u16 client_port, const MacAddress& preferred_mac,
                      const std::string& password) {
    // If the member is connected, kill the connection first
    if (room_member_impl->loop_thread && room_member_impl->loop_thread->joinable()) {
        Leave();
    }
    // If the thread isn't running but the ptr still exists, reset it
    else if (room_member_impl->loop_thread) {
        room_member_impl->loop_thread.reset();
    }

    if (!room_member_impl->client) {
        room_member_impl->client = enet_host_create(nullptr, 1, NumChannels, 0, 0);
        ASSERT_MSG(room_member_impl->client != nullptr, "Could not create client");
    }

    room_member_impl->SetState(State::Joining);

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
        room_member_impl->StartLoop();
        room_member_impl->SendJoinRequest(nick, preferred_mac, password);
        SendGameInfo(room_member_impl->current_game_info);
    } else {
        enet_peer_disconnect(room_member_impl->server, 0);
        room_member_impl->SetState(State::CouldNotConnect);
    }
}

bool RoomMember::IsConnected() const {
    return room_member_impl->IsConnected();
}

void RoomMember::SendWifiPacket(const WifiPacket& wifi_packet) {
    Packet packet;
    packet << static_cast<u8>(IdWifiPacket);
    packet << static_cast<u8>(wifi_packet.type);
    packet << wifi_packet.channel;
    packet << wifi_packet.transmitter_address;
    packet << wifi_packet.destination_address;
    packet << wifi_packet.data;
    room_member_impl->Send(std::move(packet));
}

void RoomMember::SendChatMessage(const std::string& message) {
    Packet packet;
    packet << static_cast<u8>(IdChatMessage);
    packet << message;
    room_member_impl->Send(std::move(packet));
}

void RoomMember::SendGameInfo(const GameInfo& game_info) {
    room_member_impl->current_game_info = game_info;
    if (!IsConnected())
        return;

    Packet packet;
    packet << static_cast<u8>(IdSetGameInfo);
    packet << game_info.name;
    packet << game_info.id;
    room_member_impl->Send(std::move(packet));
}

RoomMember::CallbackHandle<RoomMember::State> RoomMember::BindOnStateChanged(
    std::function<void(const RoomMember::State&)> callback) {
    return room_member_impl->Bind(callback);
}

RoomMember::CallbackHandle<WifiPacket> RoomMember::BindOnWifiPacketReceived(
    std::function<void(const WifiPacket&)> callback) {
    return room_member_impl->Bind(callback);
}

RoomMember::CallbackHandle<RoomInformation> RoomMember::BindOnRoomInformationChanged(
    std::function<void(const RoomInformation&)> callback) {
    return room_member_impl->Bind(callback);
}

RoomMember::CallbackHandle<ChatEntry> RoomMember::BindOnChatMessageRecieved(
    std::function<void(const ChatEntry&)> callback) {
    return room_member_impl->Bind(callback);
}

template <typename T>
void RoomMember::Unbind(CallbackHandle<T> handle) {
    std::lock_guard<std::mutex> lock(room_member_impl->callback_mutex);
    room_member_impl->callbacks.Get<T>().erase(handle);
}

void RoomMember::Leave() {
    room_member_impl->SetState(State::Idle);
    room_member_impl->loop_thread->join();
    room_member_impl->loop_thread.reset();

    enet_host_destroy(room_member_impl->client);
    room_member_impl->client = nullptr;
}

template void RoomMember::Unbind(CallbackHandle<WifiPacket>);
template void RoomMember::Unbind(CallbackHandle<RoomMember::State>);
template void RoomMember::Unbind(CallbackHandle<RoomInformation>);
template void RoomMember::Unbind(CallbackHandle<ChatEntry>);

} // namespace Network
