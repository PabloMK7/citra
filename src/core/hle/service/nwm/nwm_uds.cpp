// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cryptopp/osrng.h>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/nwm/uds_beacon.h"
#include "core/hle/service/nwm/uds_connection.h"
#include "core/hle/service/nwm/uds_data.h"
#include "core/memory.h"
#include "network/network.h"

namespace Service {
namespace NWM {

namespace ErrCodes {
enum {
    NotInitialized = 2,
    WrongStatus = 490,
};
} // namespace ErrCodes

// Event that is signaled every time the connection status changes.
static Kernel::SharedPtr<Kernel::Event> connection_status_event;

// Shared memory provided by the application to store the receive buffer.
// This is not currently used.
static Kernel::SharedPtr<Kernel::SharedMemory> recv_buffer_memory;

// Connection status of this 3DS.
static ConnectionStatus connection_status{};

static std::atomic<bool> initialized(false);

/* Node information about the current network.
 * The amount of elements in this vector is always the maximum number
 * of nodes specified in the network configuration.
 * The first node is always the host.
 */
static NodeList node_info;

// Node information about our own system.
static NodeInfo current_node;

struct BindNodeData {
    u32 bind_node_id;    ///< Id of the bind node associated with this data.
    u8 channel;          ///< Channel that this bind node was bound to.
    u16 network_node_id; ///< Node id this bind node is associated with, only packets from this
                         /// network node will be received.
    Kernel::SharedPtr<Kernel::Event> event;       ///< Receive event for this bind node.
    std::deque<std::vector<u8>> received_packets; ///< List of packets received on this channel.
};

// Mapping of data channels to their internal data.
static std::unordered_map<u32, BindNodeData> channel_data;

// The WiFi network channel that the network is currently on.
// Since we're not actually interacting with physical radio waves, this is just a dummy value.
static u8 network_channel = DefaultNetworkChannel;

// Information about the network that we're currently connected to.
static NetworkInfo network_info;

// Mapping of mac addresses to their respective node_ids.
static std::map<MacAddress, u16> node_map;

// Event that will generate and send the 802.11 beacon frames.
static CoreTiming::EventType* beacon_broadcast_event;

// Callback identifier for the OnWifiPacketReceived event.
static Network::RoomMember::CallbackHandle<Network::WifiPacket> wifi_packet_received;

// Mutex to synchronize access to the connection status between the emulation thread and the
// network thread.
static std::mutex connection_status_mutex;

static Kernel::SharedPtr<Kernel::Event> connection_event;

// Mutex to synchronize access to the list of received beacons between the emulation thread and the
// network thread.
static std::mutex beacon_mutex;

// Number of beacons to store before we start dropping the old ones.
// TODO(Subv): Find a more accurate value for this limit.
constexpr std::size_t MaxBeaconFrames = 15;

// List of the last <MaxBeaconFrames> beacons received from the network.
static std::list<Network::WifiPacket> received_beacons;

// Network node id used when a SecureData packet is addressed to every connected node.
constexpr u16 BroadcastNetworkNodeId = 0xFFFF;

/**
 * Returns a list of received 802.11 beacon frames from the specified sender since the last call.
 */
std::list<Network::WifiPacket> GetReceivedBeacons(const MacAddress& sender) {
    std::lock_guard<std::mutex> lock(beacon_mutex);
    if (sender != Network::BroadcastMac) {
        std::list<Network::WifiPacket> filtered_list;
        const auto beacon = std::find_if(received_beacons.begin(), received_beacons.end(),
                                         [&sender](const Network::WifiPacket& packet) {
                                             return packet.transmitter_address == sender;
                                         });
        if (beacon != received_beacons.end()) {
            filtered_list.push_back(*beacon);
            // TODO(B3N30): Check if the complete deque is cleared or just the fetched entries
            received_beacons.erase(beacon);
        }
        return filtered_list;
    }
    return std::move(received_beacons);
}

/// Sends a WifiPacket to the room we're currently connected to.
void SendPacket(Network::WifiPacket& packet) {
    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->GetState() == Network::RoomMember::State::Joined) {
            packet.transmitter_address = room_member->GetMacAddress();
            room_member->SendWifiPacket(packet);
        }
    }
}

/*
 * Returns an available index in the nodes array for the
 * currently-hosted UDS network.
 */
static u16 GetNextAvailableNodeId() {
    for (u16 index = 0; index < connection_status.max_nodes; ++index) {
        if ((connection_status.node_bitmask & (1 << index)) == 0)
            return index + 1;
    }

    // Any connection attempts to an already full network should have been refused.
    ASSERT_MSG(false, "No available connection slots in the network");
}

static void BroadcastNodeMap() {
    // Note: This is not how UDS on a 3ds does it but it shouldn't be
    // necessary for citra
    Network::WifiPacket packet;
    packet.channel = network_channel;
    packet.type = Network::WifiPacket::PacketType::NodeMap;
    packet.destination_address = Network::BroadcastMac;
    std::size_t size = node_map.size();
    using node_t = decltype(node_map)::value_type;
    packet.data.resize(sizeof(size) + (sizeof(node_t::first) + sizeof(node_t::second)) * size);
    std::memcpy(packet.data.data(), &size, sizeof(size));
    std::size_t offset = sizeof(size);
    for (const auto& node : node_map) {
        std::memcpy(packet.data.data() + offset, node.first.data(), sizeof(node.first));
        std::memcpy(packet.data.data() + offset + sizeof(node.first), &node.second,
                    sizeof(node.second));
        offset += sizeof(node.first) + sizeof(node.second);
    }

    SendPacket(packet);
}

static void HandleNodeMapPacket(const Network::WifiPacket& packet) {
    std::lock_guard<std::mutex> lock(connection_status_mutex);
    node_map.clear();
    std::size_t num_entries;
    Network::MacAddress address;
    u16 id;
    std::memcpy(&num_entries, packet.data.data(), sizeof(num_entries));
    std::size_t offset = sizeof(num_entries);
    for (std::size_t i = 0; i < num_entries; ++i) {
        std::memcpy(&address, packet.data.data() + offset, sizeof(address));
        std::memcpy(&id, packet.data.data() + offset + sizeof(address), sizeof(id));
        node_map[address] = id;
        offset += sizeof(address) + sizeof(id);
    }
}

// Inserts the received beacon frame in the beacon queue and removes any older beacons if the size
// limit is exceeded.
void HandleBeaconFrame(const Network::WifiPacket& packet) {
    std::lock_guard<std::mutex> lock(beacon_mutex);
    const auto unique_beacon =
        std::find_if(received_beacons.begin(), received_beacons.end(),
                     [&packet](const Network::WifiPacket& new_packet) {
                         return new_packet.transmitter_address == packet.transmitter_address;
                     });
    if (unique_beacon != received_beacons.end()) {
        // We already have a beacon from the same mac in the deque, remove the old one;
        received_beacons.erase(unique_beacon);
    }

    received_beacons.emplace_back(packet);

    // Discard old beacons if the buffer is full.
    if (received_beacons.size() > MaxBeaconFrames)
        received_beacons.pop_front();
}

void HandleAssociationResponseFrame(const Network::WifiPacket& packet) {
    auto assoc_result = GetAssociationResult(packet.data);

    ASSERT_MSG(std::get<AssocStatus>(assoc_result) == AssocStatus::Successful,
               "Could not join network");
    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        ASSERT(connection_status.status == static_cast<u32>(NetworkStatus::Connecting));
    }

    // Send the EAPoL-Start packet to the server.
    using Network::WifiPacket;
    WifiPacket eapol_start;
    eapol_start.channel = network_channel;
    eapol_start.data = GenerateEAPoLStartFrame(std::get<u16>(assoc_result), current_node);
    // TODO(B3N30): Encrypt the packet.
    eapol_start.destination_address = packet.transmitter_address;
    eapol_start.type = WifiPacket::PacketType::Data;

    SendPacket(eapol_start);
}

static void HandleEAPoLPacket(const Network::WifiPacket& packet) {
    std::unique_lock<std::recursive_mutex> hle_lock(HLE::g_hle_lock, std::defer_lock);
    std::unique_lock<std::mutex> lock(connection_status_mutex, std::defer_lock);
    std::lock(hle_lock, lock);

    if (GetEAPoLFrameType(packet.data) == EAPoLStartMagic) {
        if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
            LOG_DEBUG(Service_NWM, "Connection sequence aborted, because connection status is {}",
                      connection_status.status);
            return;
        }

        auto node = DeserializeNodeInfoFromFrame(packet.data);

        if (connection_status.max_nodes == connection_status.total_nodes) {
            // Reject connection attempt
            LOG_ERROR(Service_NWM, "Reached maximum nodes, but reject packet wasn't sent.");
            // TODO(B3N30): Figure out what packet is sent here
            return;
        }

        // Get an unused network node id
        u16 node_id = GetNextAvailableNodeId();
        node.network_node_id = node_id;

        connection_status.node_bitmask |= 1 << (node_id - 1);
        connection_status.changed_nodes |= 1 << (node_id - 1);
        connection_status.nodes[node_id - 1] = node.network_node_id;
        connection_status.total_nodes++;

        u8 current_nodes = network_info.total_nodes;
        node_info[current_nodes] = node;

        network_info.total_nodes++;

        node_map[packet.transmitter_address] = node.network_node_id;

        BroadcastNodeMap();

        // Send the EAPoL-Logoff packet.
        using Network::WifiPacket;
        WifiPacket eapol_logoff;
        eapol_logoff.channel = network_channel;
        eapol_logoff.data =
            GenerateEAPoLLogoffFrame(packet.transmitter_address, node.network_node_id, node_info,
                                     network_info.max_nodes, network_info.total_nodes);
        // TODO(Subv): Encrypt the packet.

        // TODO(B3N30): send the eapol packet just to the new client and implement a proper
        // broadcast packet for all other clients
        // On a 3ds the eapol packet is only sent to packet.transmitter_address
        // while a packet containing the node information is broadcasted
        // For now we will broadcast the eapol packet instead
        eapol_logoff.destination_address = Network::BroadcastMac;
        eapol_logoff.type = WifiPacket::PacketType::Data;

        SendPacket(eapol_logoff);

        connection_status_event->Signal();
    } else if (connection_status.status == static_cast<u32>(NetworkStatus::Connecting)) {
        auto logoff = ParseEAPoLLogoffFrame(packet.data);

        network_info.host_mac_address = packet.transmitter_address;
        network_info.total_nodes = logoff.connected_nodes;
        network_info.max_nodes = logoff.max_nodes;

        connection_status.network_node_id = logoff.assigned_node_id;
        connection_status.total_nodes = logoff.connected_nodes;
        connection_status.max_nodes = logoff.max_nodes;

        node_info.clear();
        node_info.reserve(network_info.max_nodes);
        for (std::size_t index = 0; index < logoff.connected_nodes; ++index) {
            connection_status.node_bitmask |= 1 << index;
            connection_status.changed_nodes |= 1 << index;
            connection_status.nodes[index] = logoff.nodes[index].network_node_id;

            node_info.emplace_back(DeserializeNodeInfo(logoff.nodes[index]));
        }

        // We're now connected, signal the application
        connection_status.status = static_cast<u32>(NetworkStatus::ConnectedAsClient);
        // Some games require ConnectToNetwork to block, for now it doesn't
        // If blocking is implemented this lock needs to be changed,
        // otherwise it might cause deadlocks
        connection_status_event->Signal();
        connection_event->Signal();
    } else if (connection_status.status == static_cast<u32>(NetworkStatus::ConnectedAsClient)) {
        // On a 3ds this packet wouldn't be addressed to already connected clients
        // We use this information because in the current implementation the host
        // isn't broadcasting the node information
        auto logoff = ParseEAPoLLogoffFrame(packet.data);

        network_info.total_nodes = logoff.connected_nodes;
        connection_status.total_nodes = logoff.connected_nodes;

        node_info.clear();
        node_info.reserve(network_info.max_nodes);
        for (std::size_t index = 0; index < logoff.connected_nodes; ++index) {
            if ((connection_status.node_bitmask & (1 << index)) == 0) {
                connection_status.changed_nodes |= 1 << index;
            }
            connection_status.nodes[index] = logoff.nodes[index].network_node_id;
            connection_status.node_bitmask |= 1 << index;
            node_info.emplace_back(DeserializeNodeInfo(logoff.nodes[index]));
        }
        connection_status_event->Signal();
    }
}

static void HandleSecureDataPacket(const Network::WifiPacket& packet) {
    auto secure_data = ParseSecureDataHeader(packet.data);
    std::unique_lock<std::recursive_mutex> hle_lock(HLE::g_hle_lock, std::defer_lock);
    std::unique_lock<std::mutex> lock(connection_status_mutex, std::defer_lock);
    std::lock(hle_lock, lock);

    if (secure_data.src_node_id == connection_status.network_node_id) {
        // Ignore packets that came from ourselves.
        return;
    }

    if (secure_data.dest_node_id != connection_status.network_node_id &&
        secure_data.dest_node_id != BroadcastNetworkNodeId) {
        // The packet wasn't addressed to us, we can only act as a router if we're the host.
        // However, we might have received this packet due to a broadcast from the host, in that
        // case just ignore it.
        ASSERT_MSG(packet.destination_address == Network::BroadcastMac ||
                       connection_status.status == static_cast<u32>(NetworkStatus::ConnectedAsHost),
                   "Can't be a router if we're not a host");

        if (connection_status.status == static_cast<u32>(NetworkStatus::ConnectedAsHost) &&
            secure_data.dest_node_id != BroadcastNetworkNodeId) {
            // Broadcast the packet so the right receiver can get it.
            // TODO(B3N30): Is there a flag that makes this kind of routing be unicast instead of
            // multicast? Perhaps this is a way to allow spectators to see some of the packets.
            Network::WifiPacket out_packet = packet;
            out_packet.destination_address = Network::BroadcastMac;
            SendPacket(out_packet);
        }
        return;
    }

    // The packet is addressed to us (or to everyone using the broadcast node id), handle it.
    // TODO(B3N30): We don't currently send nor handle management frames.
    ASSERT(!secure_data.is_management);

    // TODO(B3N30): Allow more than one bind node per channel.
    auto channel_info = channel_data.find(secure_data.data_channel);
    // Ignore packets from channels we're not interested in.
    if (channel_info == channel_data.end())
        return;

    if (channel_info->second.network_node_id != BroadcastNetworkNodeId &&
        channel_info->second.network_node_id != secure_data.src_node_id)
        return;

    // Add the received packet to the data queue.
    channel_info->second.received_packets.emplace_back(packet.data);

    // Signal the data event. We can do this directly because we locked g_hle_lock
    channel_info->second.event->Signal();
}

/*
 * Start a connection sequence with an UDS server. The sequence starts by sending an 802.11
 * authentication frame with SEQ1.
 */
void StartConnectionSequence(const MacAddress& server) {
    using Network::WifiPacket;
    WifiPacket auth_request;
    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        connection_status.status = static_cast<u32>(NetworkStatus::Connecting);

        // TODO(Subv): Handle timeout.

        // Send an authentication frame with SEQ1
        auth_request.channel = network_channel;
        auth_request.data = GenerateAuthenticationFrame(AuthenticationSeq::SEQ1);
        auth_request.destination_address = server;
        auth_request.type = WifiPacket::PacketType::Authentication;
    }

    SendPacket(auth_request);
}

/// Sends an Association Response frame to the specified mac address
void SendAssociationResponseFrame(const MacAddress& address) {
    using Network::WifiPacket;
    WifiPacket assoc_response;

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
            LOG_ERROR(Service_NWM, "Connection sequence aborted, because connection status is {}",
                      connection_status.status);
            return;
        }

        assoc_response.channel = network_channel;
        // TODO(Subv): This will cause multiple clients to end up with the same association id, but
        // we're not using that for anything.
        u16 association_id = 1;
        assoc_response.data = GenerateAssocResponseFrame(AssocStatus::Successful, association_id,
                                                         network_info.network_id);
        assoc_response.destination_address = address;
        assoc_response.type = WifiPacket::PacketType::AssociationResponse;
    }

    SendPacket(assoc_response);
}

/*
 * Handles the authentication request frame and sends the authentication response and association
 * response frames. Once an Authentication frame with SEQ1 is received by the server, it responds
 * with an Authentication frame containing SEQ2, and immediately sends an Association response frame
 * containing the details of the access point and the assigned association id for the new client.
 */
void HandleAuthenticationFrame(const Network::WifiPacket& packet) {
    // Only the SEQ1 auth frame is handled here, the SEQ2 frame doesn't need any special behavior
    if (GetAuthenticationSeqNumber(packet.data) == AuthenticationSeq::SEQ1) {
        using Network::WifiPacket;
        WifiPacket auth_request;
        {
            std::lock_guard<std::mutex> lock(connection_status_mutex);
            if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
                LOG_ERROR(Service_NWM,
                          "Connection sequence aborted, because connection status is {}",
                          connection_status.status);
                return;
            }

            // Respond with an authentication response frame with SEQ2
            auth_request.channel = network_channel;
            auth_request.data = GenerateAuthenticationFrame(AuthenticationSeq::SEQ2);
            auth_request.destination_address = packet.transmitter_address;
            auth_request.type = WifiPacket::PacketType::Authentication;
        }
        SendPacket(auth_request);

        SendAssociationResponseFrame(packet.transmitter_address);
    }
}

/// Handles the deauthentication frames sent from clients to hosts, when they leave a session
void HandleDeauthenticationFrame(const Network::WifiPacket& packet) {
    LOG_DEBUG(Service_NWM, "called");
    std::unique_lock<std::recursive_mutex> hle_lock(HLE::g_hle_lock, std::defer_lock);
    std::unique_lock<std::mutex> lock(connection_status_mutex, std::defer_lock);
    std::lock(hle_lock, lock);
    if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
        LOG_ERROR(Service_NWM, "Got deauthentication frame but we are not the host");
        return;
    }
    if (node_map.find(packet.transmitter_address) == node_map.end()) {
        LOG_ERROR(Service_NWM, "Got deauthentication frame from unknown node");
        return;
    }

    u16 node_id = node_map[packet.transmitter_address];
    auto node = std::find_if(node_info.begin(), node_info.end(), [&node_id](const NodeInfo& info) {
        return info.network_node_id == node_id;
    });
    ASSERT(node != node_info.end());

    connection_status.node_bitmask &= ~(1 << (node_id - 1));
    connection_status.changed_nodes |= 1 << (node_id - 1);
    connection_status.total_nodes--;

    network_info.total_nodes--;
    connection_status_event->Signal();
}

static void HandleDataFrame(const Network::WifiPacket& packet) {
    switch (GetFrameEtherType(packet.data)) {
    case EtherType::EAPoL:
        HandleEAPoLPacket(packet);
        break;
    case EtherType::SecureData:
        HandleSecureDataPacket(packet);
        break;
    }
}

/// Callback to parse and handle a received wifi packet.
void OnWifiPacketReceived(const Network::WifiPacket& packet) {
    switch (packet.type) {
    case Network::WifiPacket::PacketType::Beacon:
        HandleBeaconFrame(packet);
        break;
    case Network::WifiPacket::PacketType::Authentication:
        HandleAuthenticationFrame(packet);
        break;
    case Network::WifiPacket::PacketType::AssociationResponse:
        HandleAssociationResponseFrame(packet);
        break;
    case Network::WifiPacket::PacketType::Data:
        HandleDataFrame(packet);
        break;
    case Network::WifiPacket::PacketType::Deauthentication:
        HandleDeauthenticationFrame(packet);
        break;
    case Network::WifiPacket::PacketType::NodeMap:
        HandleNodeMapPacket(packet);
        break;
    }
}

void NWM_UDS::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    if (auto room_member = Network::GetRoomMember().lock())
        room_member->Unbind(wifi_packet_received);

    for (auto bind_node : channel_data) {
        bind_node.second.event->Signal();
    }
    channel_data.clear();
    node_map.clear();

    recv_buffer_memory.reset();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 16, 4);

    u32 out_buffer_size = rp.Pop<u32>();
    u32 unk1 = rp.Pop<u32>();
    u32 unk2 = rp.Pop<u32>();

    MacAddress mac_address;
    rp.PopRaw(mac_address);

    rp.Skip(9, false);

    u32 wlan_comm_id = rp.Pop<u32>();
    u32 id = rp.Pop<u32>();
    // From 3dbrew:
    // 'Official user processes create a new event handle which is then passed to this command.
    // However, those user processes don't save that handle anywhere afterwards.'
    // So we don't save/use that event too.
    Kernel::SharedPtr<Kernel::Event> input_event = rp.PopObject<Kernel::Event>();

    Kernel::MappedBuffer out_buffer = rp.PopMappedBuffer();
    ASSERT(out_buffer.GetSize() == out_buffer_size);

    std::size_t cur_buffer_size = sizeof(BeaconDataReplyHeader);

    // Retrieve all beacon frames that were received from the desired mac address.
    auto beacons = GetReceivedBeacons(mac_address);

    BeaconDataReplyHeader data_reply_header{};
    data_reply_header.total_entries = static_cast<u32>(beacons.size());
    data_reply_header.max_output_size = out_buffer_size;

    // Write each of the received beacons into the buffer
    for (const auto& beacon : beacons) {
        BeaconEntryHeader entry{};
        // TODO(Subv): Figure out what this size is used for.
        entry.unk_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.total_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.wifi_channel = beacon.channel;
        entry.header_size = sizeof(BeaconEntryHeader);
        entry.mac_address = beacon.transmitter_address;

        ASSERT(cur_buffer_size < out_buffer_size);

        out_buffer.Write(&entry, cur_buffer_size, sizeof(BeaconEntryHeader));
        cur_buffer_size += sizeof(BeaconEntryHeader);
        const unsigned char* beacon_data = beacon.data.data();
        out_buffer.Write(beacon_data, cur_buffer_size, beacon.data.size());
        cur_buffer_size += beacon.data.size();
    }

    // Update the total size in the structure and write it to the buffer again.
    data_reply_header.total_size = static_cast<u32>(cur_buffer_size);
    out_buffer.Write(&data_reply_header, 0, sizeof(BeaconDataReplyHeader));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(out_buffer);

    LOG_DEBUG(Service_NWM,
              "called out_buffer_size=0x{:08X}, wlan_comm_id=0x{:08X}, id=0x{:08X},"
              "unk1=0x{:08X}, unk2=0x{:08X}, offset={}",
              out_buffer_size, wlan_comm_id, id, unk1, unk2, cur_buffer_size);
}

void NWM_UDS::InitializeWithVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 12, 2);

    u32 sharedmem_size = rp.Pop<u32>();

    // Update the node information with the data the game gave us.
    rp.PopRaw(current_node);

    u16 version = rp.Pop<u16>();

    recv_buffer_memory = rp.PopObject<Kernel::SharedMemory>();

    initialized = true;

    ASSERT_MSG(recv_buffer_memory->size == sharedmem_size, "Invalid shared memory size.");

    if (auto room_member = Network::GetRoomMember().lock()) {
        wifi_packet_received = room_member->BindOnWifiPacketReceived(OnWifiPacketReceived);
    } else {
        LOG_ERROR(Service_NWM, "Network isn't initalized");
    }

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);

        // Reset the connection status, it contains all zeros after initialization,
        // except for the actual status value.
        connection_status = {};
        connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
        node_info.clear();
        node_info.push_back(current_node);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(connection_status_event);

    LOG_DEBUG(Service_NWM, "called sharedmem_size=0x{:08X}, version=0x{:08X}", sharedmem_size,
              version);
}

void NWM_UDS::GetConnectionStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(13, 0);

    rb.Push(RESULT_SUCCESS);
    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        rb.PushRaw(connection_status);

        // Reset the bitmask of changed nodes after each call to this
        // function to prevent falsely informing games of outstanding
        // changes in subsequent calls.
        // TODO(Subv): Find exactly where the NWM module resets this value.
        connection_status.changed_nodes = 0;
    }

    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::GetNodeInformation(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xD, 1, 0);
    u16 network_node_id = rp.Pop<u16>();

    if (!initialized) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotInitialized, ErrorModule::UDS,
                           ErrorSummary::StatusChanged, ErrorLevel::Status));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        auto itr = std::find_if(node_info.begin(), node_info.end(),
                                [network_node_id](const NodeInfo& node) {
                                    return node.network_node_id == network_node_id;
                                });
        if (itr == node_info.end()) {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::UDS,
                               ErrorSummary::WrongArgument, ErrorLevel::Status));
            return;
        }

        IPC::RequestBuilder rb = rp.MakeBuilder(11, 0);
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<NodeInfo>(*itr);
    }
    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::Bind(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 4, 0);

    u32 bind_node_id = rp.Pop<u32>();
    u32 recv_buffer_size = rp.Pop<u32>();
    u8 data_channel = rp.Pop<u8>();
    u16 network_node_id = rp.Pop<u16>();

    LOG_DEBUG(Service_NWM, "called");

    if (data_channel == 0 || bind_node_id == 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        LOG_WARNING(Service_NWM, "data_channel = {}, bind_node_id = {}", data_channel,
                    bind_node_id);
        return;
    }

    constexpr std::size_t MaxBindNodes = 16;
    if (channel_data.size() >= MaxBindNodes) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::OutOfMemory, ErrorModule::UDS,
                           ErrorSummary::OutOfResource, ErrorLevel::Status));
        LOG_WARNING(Service_NWM, "max bind nodes");
        return;
    }

    constexpr u32 MinRecvBufferSize = 0x5F4;
    if (recv_buffer_size < MinRecvBufferSize) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        LOG_WARNING(Service_NWM, "MinRecvBufferSize");
        return;
    }

    // Create a new event for this bind node.
    auto event = Kernel::Event::Create(Kernel::ResetType::OneShot,
                                       "NWM::BindNodeEvent" + std::to_string(bind_node_id));
    std::lock_guard<std::mutex> lock(connection_status_mutex);

    ASSERT(channel_data.find(data_channel) == channel_data.end());
    // TODO(B3N30): Support more than one bind node per channel.
    channel_data[data_channel] = {bind_node_id, data_channel, network_node_id, event};

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(event);
}

void NWM_UDS::Unbind(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 1, 0);

    u32 bind_node_id = rp.Pop<u32>();
    if (bind_node_id == 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    std::lock_guard<std::mutex> lock(connection_status_mutex);

    auto itr =
        std::find_if(channel_data.begin(), channel_data.end(), [bind_node_id](const auto& data) {
            return data.second.bind_node_id == bind_node_id;
        });

    if (itr != channel_data.end()) {
        channel_data.erase(itr);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(bind_node_id);
    // TODO(B3N30): Find out what the other return values are
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push<u32>(0);
}

void NWM_UDS::BeginHostingNetwork(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1D, 1, 4);

    const u32 passphrase_size = rp.Pop<u32>();

    const std::vector<u8> network_info_buffer = rp.PopStaticBuffer();
    ASSERT(network_info_buffer.size() == sizeof(NetworkInfo));
    const std::vector<u8> passphrase = rp.PopStaticBuffer();
    ASSERT(passphrase.size() == passphrase_size);

    // TODO(Subv): Store the passphrase and verify it when attempting a connection.

    LOG_DEBUG(Service_NWM, "called");

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        std::memcpy(&network_info, network_info_buffer.data(), sizeof(NetworkInfo));

        // The real UDS module throws a fatal error if this assert fails.
        ASSERT_MSG(network_info.max_nodes > 1, "Trying to host a network of only one member.");

        connection_status.status = static_cast<u32>(NetworkStatus::ConnectedAsHost);

        // Ensure the application data size is less than the maximum value.
        ASSERT_MSG(network_info.application_data_size <= ApplicationDataSize,
                   "Data size is too big.");

        // Set up basic information for this network.
        network_info.oui_value = NintendoOUI;
        network_info.oui_type = static_cast<u8>(NintendoTagId::NetworkInfo);

        connection_status.max_nodes = network_info.max_nodes;

        // Resize the nodes list to hold max_nodes.
        node_info.clear();
        node_info.resize(network_info.max_nodes);

        // There's currently only one node in the network (the host).
        connection_status.total_nodes = 1;
        network_info.total_nodes = 1;

        // The host is always the first node
        connection_status.network_node_id = 1;
        current_node.network_node_id = 1;
        connection_status.nodes[0] = connection_status.network_node_id;
        // Set the bit 0 in the nodes bitmask to indicate that node 1 is already taken.
        connection_status.node_bitmask |= 1;
        // Notify the application that the first node was set.
        connection_status.changed_nodes |= 1;

        if (auto room_member = Network::GetRoomMember().lock()) {
            if (room_member->IsConnected()) {
                network_info.host_mac_address = room_member->GetMacAddress();
            } else {
                network_info.host_mac_address = {{0x0, 0x0, 0x0, 0x0, 0x0, 0x0}};
            }
        }
        node_info[0] = current_node;

        // If the game has a preferred channel, use that instead.
        if (network_info.channel != 0)
            network_channel = network_info.channel;
        else
            network_info.channel = DefaultNetworkChannel;
    }

    connection_status_event->Signal();

    // Start broadcasting the network, send a beacon frame every 102.4ms.
    CoreTiming::ScheduleEvent(msToCycles(DefaultBeaconInterval * MillisecondsPerTU),
                              beacon_broadcast_event, 0);

    LOG_DEBUG(Service_NWM, "An UDS network has been created.");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void NWM_UDS::UpdateNetworkAttribute(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 2, 0);
    rp.Skip(2, false);
    LOG_WARNING(Service_NWM, "stubbed");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void NWM_UDS::DestroyNetwork(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    // Unschedule the beacon broadcast event.
    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);

    // Only a host can destroy
    std::lock_guard<std::mutex> lock(connection_status_mutex);
    if (connection_status.status != static_cast<u8>(NetworkStatus::ConnectedAsHost)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::WrongStatus, ErrorModule::UDS, ErrorSummary::InvalidState,
                           ErrorLevel::Status));
        LOG_WARNING(Service_NWM, "called with status {}", connection_status.status);
        return;
    }

    // TODO(B3N30): Send 3 Deauth packets

    u16_le tmp_node_id = connection_status.network_node_id;
    connection_status = {};
    connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
    connection_status.network_node_id = tmp_node_id;
    node_map.clear();
    connection_status_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    for (auto bind_node : channel_data) {
        bind_node.second.event->Signal();
    }
    channel_data.clear();

    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::DisconnectNetwork(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xA, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    using Network::WifiPacket;
    WifiPacket deauth;
    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        if (connection_status.status == static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
            // A real 3ds makes strange things here. We do the same
            u16_le tmp_node_id = connection_status.network_node_id;
            connection_status = {};
            connection_status.status = static_cast<u32>(NetworkStatus::ConnectedAsHost);
            connection_status.network_node_id = tmp_node_id;
            node_map.clear();
            LOG_DEBUG(Service_NWM, "called as a host");
            rb.Push(ResultCode(ErrCodes::WrongStatus, ErrorModule::UDS, ErrorSummary::InvalidState,
                               ErrorLevel::Status));
            return;
        }
        u16_le tmp_node_id = connection_status.network_node_id;
        connection_status = {};
        connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
        connection_status.network_node_id = tmp_node_id;
        node_map.clear();
        connection_status_event->Signal();

        deauth.channel = network_channel;
        // TODO(B3N30): Add disconnect reason
        deauth.data = {};
        deauth.destination_address = network_info.host_mac_address;
        deauth.type = WifiPacket::PacketType::Deauthentication;
    }

    SendPacket(deauth);

    for (auto bind_node : channel_data) {
        bind_node.second.event->Signal();
    }
    channel_data.clear();

    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::SendTo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 6, 2);

    rp.Skip(1, false);
    u16 dest_node_id = rp.Pop<u16>();
    u8 data_channel = rp.Pop<u8>();
    rp.Skip(1, false);
    u32 data_size = rp.Pop<u32>();
    u8 flags = rp.Pop<u8>();

    // There should never be a dest_node_id of 0
    ASSERT(dest_node_id != 0);

    std::vector<u8> input_buffer = rp.PopStaticBuffer();
    ASSERT(input_buffer.size() >= data_size);
    input_buffer.resize(data_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    std::lock_guard<std::mutex> lock(connection_status_mutex);
    if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsClient) &&
        connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    if (dest_node_id == connection_status.network_node_id) {
        LOG_ERROR(Service_NWM, "tried to send packet to itself");
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Status));
        return;
    }

    Network::MacAddress dest_address;

    if (flags >> 2) {
        LOG_ERROR(Service_NWM, "Unexpected flags 0x{:02X}", flags);
    }

    if ((flags & (0x1 << 1)) || dest_node_id == 0xFFFF) {
        // Broadcast
        dest_address = Network::BroadcastMac;
    } else if (dest_node_id != 1) {
        // Send to specific client
        auto destination =
            std::find_if(node_map.begin(), node_map.end(),
                         [dest_node_id](const auto& node) { return node.second == dest_node_id; });
        if (destination == node_map.end()) {
            LOG_ERROR(Service_NWM, "tried to send packet to unknown dest id {}", dest_node_id);
            rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::UDS,
                               ErrorSummary::WrongArgument, ErrorLevel::Status));
            return;
        }
        dest_address = destination->first;
    } else {
        // Send message to host
        dest_address = network_info.host_mac_address;
    }

    constexpr std::size_t MaxSize = 0x5C6;
    if (data_size > MaxSize) {
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    // TODO(B3N30): Increment the sequence number after each sent packet.
    u16 sequence_number = 0;
    std::vector<u8> data_payload =
        GenerateDataPayload(input_buffer, data_channel, dest_node_id,
                            connection_status.network_node_id, sequence_number);

    // TODO(B3N30): Retrieve the MAC address of the dest_node_id and our own to encrypt
    // and encapsulate the payload.

    Network::WifiPacket packet;

    packet.destination_address = dest_address;
    packet.channel = network_channel;
    packet.data = std::move(data_payload);
    packet.type = Network::WifiPacket::PacketType::Data;

    SendPacket(packet);

    rb.Push(RESULT_SUCCESS);
}

void NWM_UDS::PullPacket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 3, 0);

    u32 bind_node_id = rp.Pop<u32>();
    u32 max_out_buff_size_aligned = rp.Pop<u32>();
    u32 max_out_buff_size = rp.Pop<u32>();

    // This size is hard coded into the uds module. We don't know the meaning yet.
    u32 buff_size = std::min<u32>(max_out_buff_size_aligned, 0x172) << 2;

    std::lock_guard<std::mutex> lock(connection_status_mutex);
    if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost) &&
        connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsClient) &&
        connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsSpectator)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    auto channel =
        std::find_if(channel_data.begin(), channel_data.end(), [bind_node_id](const auto& data) {
            return data.second.bind_node_id == bind_node_id;
        });

    if (channel == channel_data.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    if (channel->second.received_packets.empty()) {
        std::vector<u8> output_buffer(buff_size, 0);
        IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
        rb.Push<u16>(0);
        rb.PushStaticBuffer(output_buffer, 0);
        return;
    }

    const auto& next_packet = channel->second.received_packets.front();

    auto secure_data = ParseSecureDataHeader(next_packet);
    auto data_size = secure_data.GetActualDataSize();

    if (data_size > max_out_buff_size) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);

    std::vector<u8> output_buffer(buff_size, 0);
    // Write the actual data.
    std::memcpy(output_buffer.data(),
                next_packet.data() + sizeof(LLCHeader) + sizeof(SecureDataHeader), data_size);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(data_size);
    rb.Push<u16>(secure_data.src_node_id);
    rb.PushStaticBuffer(output_buffer, 0);

    channel->second.received_packets.pop_front();
}

void NWM_UDS::GetChannel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    std::lock_guard<std::mutex> lock(connection_status_mutex);
    bool is_connected = connection_status.status != static_cast<u32>(NetworkStatus::NotConnected);

    u8 channel = is_connected ? network_channel : 0;

    rb.Push(RESULT_SUCCESS);
    rb.Push(channel);

    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::ConnectToNetwork(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 2, 4);

    u8 connection_type = rp.Pop<u8>();
    u32 passphrase_size = rp.Pop<u32>();

    const std::vector<u8> network_struct_buffer = rp.PopStaticBuffer();
    ASSERT(network_struct_buffer.size() == sizeof(NetworkInfo));

    const std::vector<u8> passphrase = rp.PopStaticBuffer();

    std::memcpy(&network_info, network_struct_buffer.data(), sizeof(network_info));

    // Start the connection sequence
    StartConnectionSequence(network_info.host_mac_address);

    // 300 ms
    // Since this timing is handled by core_timing it could differ from the 'real world' time
    static constexpr std::chrono::nanoseconds UDSConnectionTimeout{300000000};

    connection_event = ctx.SleepClientThread(
        Kernel::GetCurrentThread(), "uds::ConnectToNetwork", UDSConnectionTimeout,
        [](Kernel::SharedPtr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx,
           ThreadWakeupReason reason) {
            // TODO(B3N30): Add error handling for host full and timeout
            IPC::RequestBuilder rb(ctx, 0x1E, 1, 0);
            rb.Push(RESULT_SUCCESS);
            LOG_DEBUG(Service_NWM, "connection sequence finished");
        });

    LOG_DEBUG(Service_NWM, "called");
}

void NWM_UDS::SetApplicationData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 1, 2);

    u32 size = rp.Pop<u32>();

    const std::vector<u8> application_data = rp.PopStaticBuffer();
    ASSERT(application_data.size() == size);

    LOG_DEBUG(Service_NWM, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (size > ApplicationDataSize) {
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    network_info.application_data_size = static_cast<u8>(size);
    std::memcpy(network_info.application_data.data(), application_data.data(), size);

    rb.Push(RESULT_SUCCESS);
}

void NWM_UDS::DecryptBeaconData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 0, 6);

    const std::vector<u8> network_struct_buffer = rp.PopStaticBuffer();
    ASSERT(network_struct_buffer.size() == sizeof(NetworkInfo));

    const std::vector<u8> encrypted_data0_buffer = rp.PopStaticBuffer();
    const std::vector<u8> encrypted_data1_buffer = rp.PopStaticBuffer();

    LOG_DEBUG(Service_NWM, "called");

    NetworkInfo net_info;
    std::memcpy(&net_info, network_struct_buffer.data(), sizeof(net_info));

    // Read the encrypted data.
    // The first 4 bytes should be the OUI and the OUI Type of the tags.
    std::array<u8, 3> oui;
    std::memcpy(oui.data(), encrypted_data0_buffer.data(), oui.size());
    ASSERT_MSG(oui == NintendoOUI, "Unexpected OUI");

    ASSERT_MSG(encrypted_data0_buffer[3] == static_cast<u8>(NintendoTagId::EncryptedData0),
               "Unexpected tag id");

    std::vector<u8> beacon_data(encrypted_data0_buffer.size() - 4 + encrypted_data1_buffer.size() -
                                4);
    std::memcpy(beacon_data.data(), encrypted_data0_buffer.data() + 4,
                encrypted_data0_buffer.size() - 4);
    std::memcpy(beacon_data.data() + encrypted_data0_buffer.size() - 4,
                encrypted_data1_buffer.data() + 4, encrypted_data1_buffer.size() - 4);

    // Decrypt the data
    DecryptBeacon(net_info, beacon_data);

    // The beacon data header contains the MD5 hash of the data.
    BeaconData beacon_header;
    std::memcpy(&beacon_header, beacon_data.data(), sizeof(beacon_header));

    // TODO(Subv): Verify the MD5 hash of the data and return 0xE1211005 if invalid.

    u8 num_nodes = net_info.max_nodes;

    std::vector<NodeInfo> nodes;

    for (int i = 0; i < num_nodes; ++i) {
        BeaconNodeInfo info;
        std::memcpy(&info, beacon_data.data() + sizeof(beacon_header) + i * sizeof(info),
                    sizeof(info));

        // Deserialize the node information.
        NodeInfo node{};
        node.friend_code_seed = info.friend_code_seed;
        node.network_node_id = info.network_node_id;
        for (int i = 0; i < info.username.size(); ++i)
            node.username[i] = info.username[i];

        nodes.push_back(node);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    std::vector<u8> output_buffer(sizeof(NodeInfo) * UDSMaxNodes, 0);
    std::memcpy(output_buffer.data(), nodes.data(), sizeof(NodeInfo) * nodes.size());
    rb.PushStaticBuffer(output_buffer, 0);
}

// Sends a 802.11 beacon frame with information about the current network.
static void BeaconBroadcastCallback(u64 userdata, s64 cycles_late) {
    // Don't do anything if we're not actually hosting a network
    if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost))
        return;

    std::vector<u8> frame = GenerateBeaconFrame(network_info, node_info);

    using Network::WifiPacket;
    WifiPacket packet;
    packet.type = WifiPacket::PacketType::Beacon;
    packet.data = std::move(frame);
    packet.destination_address = Network::BroadcastMac;
    packet.channel = network_channel;

    SendPacket(packet);

    // Start broadcasting the network, send a beacon frame every 102.4ms.
    CoreTiming::ScheduleEvent(msToCycles(DefaultBeaconInterval * MillisecondsPerTU) - cycles_late,
                              beacon_broadcast_event, 0);
}

NWM_UDS::NWM_UDS() : ServiceFramework("nwm::UDS") {
    static const FunctionInfo functions[] = {
        {0x000102C2, nullptr, "Initialize (deprecated)"},
        {0x00020000, nullptr, "Scrap"},
        {0x00030000, &NWM_UDS::Shutdown, "Shutdown"},
        {0x00040402, nullptr, "CreateNetwork (deprecated)"},
        {0x00050040, nullptr, "EjectClient"},
        {0x00060000, nullptr, "EjectSpectator"},
        {0x00070080, &NWM_UDS::UpdateNetworkAttribute, "UpdateNetworkAttribute"},
        {0x00080000, &NWM_UDS::DestroyNetwork, "DestroyNetwork"},
        {0x00090442, nullptr, "ConnectNetwork (deprecated)"},
        {0x000A0000, &NWM_UDS::DisconnectNetwork, "DisconnectNetwork"},
        {0x000B0000, &NWM_UDS::GetConnectionStatus, "GetConnectionStatus"},
        {0x000D0040, &NWM_UDS::GetNodeInformation, "GetNodeInformation"},
        {0x000E0006, nullptr, "DecryptBeaconData (deprecated)"},
        {0x000F0404, &NWM_UDS::RecvBeaconBroadcastData, "RecvBeaconBroadcastData"},
        {0x00100042, &NWM_UDS::SetApplicationData, "SetApplicationData"},
        {0x00110040, nullptr, "GetApplicationData"},
        {0x00120100, &NWM_UDS::Bind, "Bind"},
        {0x00130040, &NWM_UDS::Unbind, "Unbind"},
        {0x001400C0, &NWM_UDS::PullPacket, "PullPacket"},
        {0x00150080, nullptr, "SetMaxSendDelay"},
        {0x00170182, &NWM_UDS::SendTo, "SendTo"},
        {0x001A0000, &NWM_UDS::GetChannel, "GetChannel"},
        {0x001B0302, &NWM_UDS::InitializeWithVersion, "InitializeWithVersion"},
        {0x001D0044, &NWM_UDS::BeginHostingNetwork, "BeginHostingNetwork"},
        {0x001E0084, &NWM_UDS::ConnectToNetwork, "ConnectToNetwork"},
        {0x001F0006, &NWM_UDS::DecryptBeaconData, "DecryptBeaconData"},
        {0x00200040, nullptr, "Flush"},
        {0x00210080, nullptr, "SetProbeResponseParam"},
        {0x00220402, nullptr, "ScanOnConnection"},
    };
    connection_status_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NWM::connection_status_event");

    RegisterHandlers(functions);

    beacon_broadcast_event =
        CoreTiming::RegisterEvent("UDS::BeaconBroadcastCallback", BeaconBroadcastCallback);

    CryptoPP::AutoSeededRandomPool rng;
    auto mac = SharedPage::DefaultMac;
    // Keep the Nintendo 3DS MAC header and randomly generate the last 3 bytes
    rng.GenerateBlock(static_cast<CryptoPP::byte*>(mac.data() + 3), 3);

    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            mac = room_member->GetMacAddress();
        }
    }

    Core::System::GetInstance().GetSharedPageHandler()->SetMacAddress(mac);
    Core::System::GetInstance().GetSharedPageHandler()->SetWifiLinkLevel(
        SharedPage::WifiLinkLevel::BEST);
}

NWM_UDS::~NWM_UDS() {
    network_info = {};
    channel_data.clear();
    connection_status_event = nullptr;
    recv_buffer_memory = nullptr;
    initialized = false;

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        connection_status = {};
        connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
    }

    if (auto room_member = Network::GetRoomMember().lock())
        room_member->Unbind(wifi_packet_received);

    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);
}

} // namespace NWM
} // namespace Service
