// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "common/logging/log.h"
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

// Event that is signaled every time the connection status changes.
static Kernel::SharedPtr<Kernel::Event> connection_status_event;

// Shared memory provided by the application to store the receive buffer.
// This is not currently used.
static Kernel::SharedPtr<Kernel::SharedMemory> recv_buffer_memory;

// Connection status of this 3DS.
static ConnectionStatus connection_status{};

/* Node information about the current network.
 * The amount of elements in this vector is always the maximum number
 * of nodes specified in the network configuration.
 * The first node is always the host.
 */
static NodeList node_info;

// Node information about our own system.
static NodeInfo current_node;

// Mapping of bind node ids to their respective events.
static std::unordered_map<u32, Kernel::SharedPtr<Kernel::Event>> bind_node_events;

// The WiFi network channel that the network is currently on.
// Since we're not actually interacting with physical radio waves, this is just a dummy value.
static u8 network_channel = DefaultNetworkChannel;

// Information about the network that we're currently connected to.
static NetworkInfo network_info;

// Event that will generate and send the 802.11 beacon frames.
static int beacon_broadcast_event;

// Mutex to synchronize access to the connection status between the emulation thread and the
// network thread.
static std::mutex connection_status_mutex;

// Mutex to synchronize access to the list of received beacons between the emulation thread and the
// network thread.
static std::mutex beacon_mutex;

// Number of beacons to store before we start dropping the old ones.
// TODO(Subv): Find a more accurate value for this limit.
constexpr size_t MaxBeaconFrames = 15;

// List of the last <MaxBeaconFrames> beacons received from the network.
static std::list<Network::WifiPacket> received_beacons;

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
    // TODO(Subv): Implement.
}

/*
 * Returns an available index in the nodes array for the
 * currently-hosted UDS network.
 */
static u16 GetNextAvailableNodeId() {
    for (u16 index = 0; index < connection_status.max_nodes; ++index) {
        if ((connection_status.node_bitmask & (1 << index)) == 0)
            return index;
    }

    // Any connection attempts to an already full network should have been refused.
    ASSERT_MSG(false, "No available connection slots in the network");
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
    std::lock_guard<std::mutex> lock(connection_status_mutex);

    if (GetEAPoLFrameType(packet.data) == EAPoLStartMagic) {
        if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
            LOG_DEBUG(Service_NWM, "Connection sequence aborted, because connection status is %u",
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
        node.network_node_id = node_id + 1;

        connection_status.node_bitmask |= 1 << node_id;
        connection_status.changed_nodes |= 1 << node_id;
        connection_status.nodes[node_id] = node.network_node_id;
        connection_status.total_nodes++;

        u8 current_nodes = network_info.total_nodes;
        node_info[current_nodes] = node;

        network_info.total_nodes++;

        // Send the EAPoL-Logoff packet.
        using Network::WifiPacket;
        WifiPacket eapol_logoff;
        eapol_logoff.channel = network_channel;
        eapol_logoff.data =
            GenerateEAPoLLogoffFrame(packet.transmitter_address, node.network_node_id, node_info,
                                     network_info.max_nodes, network_info.total_nodes);
        // TODO(Subv): Encrypt the packet.
        eapol_logoff.destination_address = packet.transmitter_address;
        eapol_logoff.type = WifiPacket::PacketType::Data;

        SendPacket(eapol_logoff);
        // TODO(B3N30): Broadcast updated node list
        // The 3ds does this presumably to support spectators.
        std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);
        connection_status_event->Signal();
    } else {
        if (connection_status.status != static_cast<u32>(NetworkStatus::NotConnected)) {
            LOG_DEBUG(Service_NWM, "Connection sequence aborted, because connection status is %u",
                      connection_status.status);
            return;
        }
        auto logoff = ParseEAPoLLogoffFrame(packet.data);

        network_info.total_nodes = logoff.connected_nodes;
        network_info.max_nodes = logoff.max_nodes;

        connection_status.network_node_id = logoff.assigned_node_id;
        connection_status.total_nodes = logoff.connected_nodes;
        connection_status.max_nodes = logoff.max_nodes;

        node_info.clear();
        node_info.reserve(network_info.max_nodes);
        for (size_t index = 0; index < logoff.connected_nodes; ++index) {
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
        std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);
        connection_status_event->Signal();
    }
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
        ASSERT(connection_status.status == static_cast<u32>(NetworkStatus::NotConnected));

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
            LOG_ERROR(Service_NWM, "Connection sequence aborted, because connection status is %u",
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
                          "Connection sequence aborted, because connection status is %u",
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

static void HandleDataFrame(const Network::WifiPacket& packet) {
    switch (GetFrameEtherType(packet.data)) {
    case EtherType::EAPoL:
        HandleEAPoLPacket(packet);
        break;
    case EtherType::SecureData:
        // TODO(B3N30): Handle SecureData packets
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
    }
}

/**
 * NWM_UDS::Shutdown service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Shutdown(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_NWM, "(STUBBED) called");
}

/**
 * NWM_UDS::RecvBeaconBroadcastData service function
 * Returns the raw beacon data for nearby networks that match the supplied WlanCommId.
 *  Inputs:
 *      1 : Output buffer max size
 *    2-3 : Unknown
 *    4-5 : Host MAC address.
 *   6-14 : Unused
 *     15 : WLan Comm Id
 *     16 : Id
 *     17 : Value 0
 *     18 : Input handle
 *     19 : (Size<<4) | 12
 *     20 : Output buffer ptr
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RecvBeaconBroadcastData(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x0F, 16, 4);

    u32 out_buffer_size = rp.Pop<u32>();
    u32 unk1 = rp.Pop<u32>();
    u32 unk2 = rp.Pop<u32>();

    MacAddress mac_address;
    rp.PopRaw(mac_address);

    rp.Skip(9, false);

    u32 wlan_comm_id = rp.Pop<u32>();
    u32 id = rp.Pop<u32>();
    Kernel::Handle input_handle = rp.PopHandle();

    size_t desc_size;
    const VAddr out_buffer_ptr = rp.PopMappedBuffer(&desc_size);
    ASSERT(desc_size == out_buffer_size);

    VAddr current_buffer_pos = out_buffer_ptr;
    u32 total_size = sizeof(BeaconDataReplyHeader);

    // Retrieve all beacon frames that were received from the desired mac address.
    auto beacons = GetReceivedBeacons(mac_address);

    BeaconDataReplyHeader data_reply_header{};
    data_reply_header.total_entries = static_cast<u32>(beacons.size());
    data_reply_header.max_output_size = out_buffer_size;

    Memory::WriteBlock(current_buffer_pos, &data_reply_header, sizeof(BeaconDataReplyHeader));
    current_buffer_pos += sizeof(BeaconDataReplyHeader);

    // Write each of the received beacons into the buffer
    for (const auto& beacon : beacons) {
        BeaconEntryHeader entry{};
        // TODO(Subv): Figure out what this size is used for.
        entry.unk_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.total_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.wifi_channel = beacon.channel;
        entry.header_size = sizeof(BeaconEntryHeader);
        entry.mac_address = beacon.transmitter_address;

        ASSERT(current_buffer_pos < out_buffer_ptr + out_buffer_size);

        Memory::WriteBlock(current_buffer_pos, &entry, sizeof(BeaconEntryHeader));
        current_buffer_pos += sizeof(BeaconEntryHeader);

        Memory::WriteBlock(current_buffer_pos, beacon.data.data(), beacon.data.size());
        current_buffer_pos += static_cast<VAddr>(beacon.data.size());

        total_size += static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
    }

    // Update the total size in the structure and write it to the buffer again.
    data_reply_header.total_size = total_size;
    Memory::WriteBlock(out_buffer_ptr, &data_reply_header, sizeof(BeaconDataReplyHeader));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_NWM, "called out_buffer_size=0x%08X, wlan_comm_id=0x%08X, id=0x%08X,"
                           "input_handle=0x%08X, out_buffer_ptr=0x%08X, unk1=0x%08X, unk2=0x%08X",
              out_buffer_size, wlan_comm_id, id, input_handle, out_buffer_ptr, unk1, unk2);
}

/**
 * NWM_UDS::Initialize service function
 *  Inputs:
 *      1 : Shared memory size
 *   2-11 : Input NodeInfo Structure
 *     12 : 2-byte Version
 *     13 : Value 0
 *     14 : Shared memory handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Value 0
 *      3 : Output event handle
 */
static void InitializeWithVersion(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1B, 12, 2);

    u32 sharedmem_size = rp.Pop<u32>();

    // Update the node information with the data the game gave us.
    rp.PopRaw(current_node);

    u16 version = rp.Pop<u16>();

    Kernel::Handle sharedmem_handle = rp.PopHandle();

    recv_buffer_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(sharedmem_handle);

    ASSERT_MSG(recv_buffer_memory->size == sharedmem_size, "Invalid shared memory size.");

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);

        // Reset the connection status, it contains all zeros after initialization,
        // except for the actual status value.
        connection_status = {};
        connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(connection_status_event).Unwrap());

    // TODO(Subv): Connect the OnWifiPacketReceived function to the wifi packet received callback of
    // the room we're currently in.

    LOG_DEBUG(Service_NWM, "called sharedmem_size=0x%08X, version=0x%08X, sharedmem_handle=0x%08X",
              sharedmem_size, version, sharedmem_handle);
}

/**
 * NWM_UDS::GetConnectionStatus service function.
 * Returns the connection status structure for the currently open network connection.
 * This structure contains information about the connection,
 * like the number of connected nodes, etc.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-13 : Channel of the current WiFi network connection.
 */
static void GetConnectionStatus(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xB, 0, 0);
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

/**
 * NWM_UDS::Bind service function.
 * Binds a BindNodeId to a data channel and retrieves a data event.
 *  Inputs:
 *      1 : BindNodeId
 *      2 : Receive buffer size.
 *      3 : u8 Data channel to bind to.
 *      4 : Network node id.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Copy handle descriptor.
 *      3 : Data available event handle.
 */
static void Bind(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x12, 4, 0);

    u32 bind_node_id = rp.Pop<u32>();
    u32 recv_buffer_size = rp.Pop<u32>();
    u8 data_channel = rp.Pop<u8>();
    u16 network_node_id = rp.Pop<u16>();

    // TODO(Subv): Store the data channel and verify it when receiving data frames.

    LOG_DEBUG(Service_NWM, "called");

    if (data_channel == 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    // Create a new event for this bind node.
    // TODO(Subv): Signal this event when new data is received on this data channel.
    auto event = Kernel::Event::Create(Kernel::ResetType::OneShot,
                                       "NWM::BindNodeEvent" + std::to_string(bind_node_id));
    bind_node_events[bind_node_id] = event;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(event).Unwrap());
}

/**
 * NWM_UDS::BeginHostingNetwork service function.
 * Creates a network and starts broadcasting its presence.
 *  Inputs:
 *      1 : Passphrase buffer size.
 *      3 : VAddr of the NetworkInfo structure.
 *      5 : VAddr of the passphrase.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void BeginHostingNetwork(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1D, 1, 4);

    const u32 passphrase_size = rp.Pop<u32>();

    size_t desc_size;
    const VAddr network_info_address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == sizeof(NetworkInfo));
    const VAddr passphrase_address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == passphrase_size);

    // TODO(Subv): Store the passphrase and verify it when attempting a connection.

    LOG_DEBUG(Service_NWM, "called");

    Memory::ReadBlock(network_info_address, &network_info, sizeof(NetworkInfo));

    // The real UDS module throws a fatal error if this assert fails.
    ASSERT_MSG(network_info.max_nodes > 1, "Trying to host a network of only one member.");

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        connection_status.status = static_cast<u32>(NetworkStatus::ConnectedAsHost);

        // Ensure the application data size is less than the maximum value.
        ASSERT_MSG(network_info.application_data_size <= ApplicationDataSize,
                   "Data size is too big.");

        // Set up basic information for this network.
        network_info.oui_value = NintendoOUI;
        network_info.oui_type = static_cast<u8>(NintendoTagId::NetworkInfo);

        connection_status.max_nodes = network_info.max_nodes;

        // Resize the nodes list to hold max_nodes.
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
        node_info[0] = current_node;
    }

    // If the game has a preferred channel, use that instead.
    if (network_info.channel != 0)
        network_channel = network_info.channel;

    connection_status_event->Signal();

    // Start broadcasting the network, send a beacon frame every 102.4ms.
    CoreTiming::ScheduleEvent(msToCycles(DefaultBeaconInterval * MillisecondsPerTU),
                              beacon_broadcast_event, 0);

    LOG_WARNING(Service_NWM,
                "An UDS network has been created, but broadcasting it is unimplemented.");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

/**
 * NWM_UDS::DestroyNetwork service function.
 * Closes the network that we're currently hosting.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void DestroyNetwork(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x08, 0, 0);

    // TODO(Subv): Find out what happens if this is called while
    // no network is being hosted.

    // Unschedule the beacon broadcast event.
    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);

        // TODO(Subv): Check if connection_status is indeed reset after this call.
        connection_status = {};
        connection_status.status = static_cast<u8>(NetworkStatus::NotConnected);
    }
    connection_status_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NWM, "called");
}

/**
 * NWM_UDS::SendTo service function.
 * Sends a data frame to the UDS network we're connected to.
 *  Inputs:
 *      0 : Command header.
 *      1 : Unknown.
 *      2 : u16 Destination network node id.
 *      3 : u8 Data channel.
 *      4 : Buffer size >> 2
 *      5 : Data size
 *      6 : Flags
 *      7 : Input buffer descriptor
 *      8 : Input buffer address
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SendTo(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x17, 6, 2);

    rp.Skip(1, false);
    u16 dest_node_id = rp.Pop<u16>();
    u8 data_channel = rp.Pop<u8>();
    rp.Skip(1, false);
    u32 data_size = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();

    size_t desc_size;
    const VAddr input_address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == data_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    u16 network_node_id;

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsClient) &&
            connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost)) {
            rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                               ErrorSummary::InvalidState, ErrorLevel::Status));
            return;
        }

        if (dest_node_id == connection_status.network_node_id) {
            rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::UDS,
                               ErrorSummary::WrongArgument, ErrorLevel::Status));
            return;
        }

        network_node_id = connection_status.network_node_id;
    }

    // TODO(Subv): Do something with the flags.

    constexpr size_t MaxSize = 0x5C6;
    if (data_size > MaxSize) {
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    std::vector<u8> data(data_size);
    Memory::ReadBlock(input_address, data.data(), data.size());

    // TODO(Subv): Increment the sequence number after each sent packet.
    u16 sequence_number = 0;
    std::vector<u8> data_payload =
        GenerateDataPayload(data, data_channel, dest_node_id, network_node_id, sequence_number);

    // TODO(Subv): Retrieve the MAC address of the dest_node_id and our own to encrypt
    // and encapsulate the payload.

    // TODO(Subv): Send the frame.

    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NWM, "(STUB) called dest_node_id=%u size=%u flags=%u channel=%u",
                static_cast<u32>(dest_node_id), data_size, flags, static_cast<u32>(data_channel));
}

/**
 * NWM_UDS::GetChannel service function.
 * Returns the WiFi channel in which the network we're connected to is transmitting.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Channel of the current WiFi network connection.
 */
static void GetChannel(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    std::lock_guard<std::mutex> lock(connection_status_mutex);
    bool is_connected = connection_status.status != static_cast<u32>(NetworkStatus::NotConnected);

    u8 channel = is_connected ? network_channel : 0;

    rb.Push(RESULT_SUCCESS);
    rb.Push(channel);

    LOG_DEBUG(Service_NWM, "called");
}

/**
 * NWM_UDS::SetApplicationData service function.
 * Updates the application data that is being broadcast in the beacon frames
 * for the network that we're hosting.
 *  Inputs:
 *      1 : Data size.
 *      3 : VAddr of the data.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Channel of the current WiFi network connection.
 */
static void SetApplicationData(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 1, 2);

    u32 size = rp.Pop<u32>();

    size_t desc_size;
    const VAddr address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == size);

    LOG_DEBUG(Service_NWM, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (size > ApplicationDataSize) {
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    network_info.application_data_size = size;
    Memory::ReadBlock(address, network_info.application_data.data(), size);

    rb.Push(RESULT_SUCCESS);
}

/**
 * NWM_UDS::DecryptBeaconData service function.
 * Decrypts the encrypted data tags contained in the 802.11 beacons.
 *  Inputs:
 *      1 : Input network struct buffer descriptor.
 *      2 : Input network struct buffer ptr.
 *      3 : Input tag0 encrypted buffer descriptor.
 *      4 : Input tag0 encrypted buffer ptr.
 *      5 : Input tag1 encrypted buffer descriptor.
 *      6 : Input tag1 encrypted buffer ptr.
 *     64 : Output buffer descriptor.
 *     65 : Output buffer ptr.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void DecryptBeaconData(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1F, 0, 6);

    size_t desc_size;
    const VAddr network_struct_addr = rp.PopStaticBuffer(&desc_size);
    ASSERT(desc_size == sizeof(NetworkInfo));

    size_t data0_size;
    const VAddr encrypted_data0_addr = rp.PopStaticBuffer(&data0_size);

    size_t data1_size;
    const VAddr encrypted_data1_addr = rp.PopStaticBuffer(&data1_size);

    size_t output_buffer_size;
    const VAddr output_buffer_addr = rp.PeekStaticBuffer(0, &output_buffer_size);

    // This size is hardcoded in the 3DS UDS code.
    ASSERT(output_buffer_size == sizeof(NodeInfo) * UDSMaxNodes);

    LOG_WARNING(Service_NWM, "called in0=%08X in1=%08X out=%08X", encrypted_data0_addr,
                encrypted_data1_addr, output_buffer_addr);

    NetworkInfo net_info;
    Memory::ReadBlock(network_struct_addr, &net_info, sizeof(net_info));

    // Read the encrypted data.
    // The first 4 bytes should be the OUI and the OUI Type of the tags.
    std::array<u8, 3> oui;
    Memory::ReadBlock(encrypted_data0_addr, oui.data(), oui.size());
    ASSERT_MSG(oui == NintendoOUI, "Unexpected OUI");
    Memory::ReadBlock(encrypted_data1_addr, oui.data(), oui.size());
    ASSERT_MSG(oui == NintendoOUI, "Unexpected OUI");

    ASSERT_MSG(Memory::Read8(encrypted_data0_addr + 3) ==
                   static_cast<u8>(NintendoTagId::EncryptedData0),
               "Unexpected tag id");
    ASSERT_MSG(Memory::Read8(encrypted_data1_addr + 3) ==
                   static_cast<u8>(NintendoTagId::EncryptedData1),
               "Unexpected tag id");

    std::vector<u8> beacon_data(data0_size + data1_size);
    Memory::ReadBlock(encrypted_data0_addr + 4, beacon_data.data(), data0_size);
    Memory::ReadBlock(encrypted_data1_addr + 4, beacon_data.data() + data0_size, data1_size);

    // Decrypt the data
    DecryptBeaconData(net_info, beacon_data);

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

    Memory::ZeroBlock(output_buffer_addr, sizeof(NodeInfo) * UDSMaxNodes);
    Memory::WriteBlock(output_buffer_addr, nodes.data(), sizeof(NodeInfo) * nodes.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.PushStaticBuffer(output_buffer_addr, output_buffer_size, 0);
    rb.Push(RESULT_SUCCESS);
}

// Sends a 802.11 beacon frame with information about the current network.
static void BeaconBroadcastCallback(u64 userdata, int cycles_late) {
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

/*
 * Called when a client connects to an UDS network we're hosting,
 * updates the connection status and signals the update event.
 * @param network_node_id Network Node Id of the connecting client.
 */
void OnClientConnected(u16 network_node_id) {
    std::lock_guard<std::mutex> lock(connection_status_mutex);
    ASSERT_MSG(connection_status.status == static_cast<u32>(NetworkStatus::ConnectedAsHost),
               "Can not accept clients if we're not hosting a network");
    ASSERT_MSG(connection_status.total_nodes < connection_status.max_nodes,
               "Can not accept connections on a full network");

    u32 node_id = GetNextAvailableNodeId();
    connection_status.node_bitmask |= 1 << node_id;
    connection_status.changed_nodes |= 1 << node_id;
    connection_status.nodes[node_id] = network_node_id;
    connection_status.total_nodes++;
    connection_status_event->Signal();
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000102C2, nullptr, "Initialize (deprecated)"},
    {0x00020000, nullptr, "Scrap"},
    {0x00030000, Shutdown, "Shutdown"},
    {0x00040402, nullptr, "CreateNetwork (deprecated)"},
    {0x00050040, nullptr, "EjectClient"},
    {0x00060000, nullptr, "EjectSpectator"},
    {0x00070080, nullptr, "UpdateNetworkAttribute"},
    {0x00080000, DestroyNetwork, "DestroyNetwork"},
    {0x00090442, nullptr, "ConnectNetwork (deprecated)"},
    {0x000A0000, nullptr, "DisconnectNetwork"},
    {0x000B0000, GetConnectionStatus, "GetConnectionStatus"},
    {0x000D0040, nullptr, "GetNodeInformation"},
    {0x000E0006, nullptr, "DecryptBeaconData (deprecated)"},
    {0x000F0404, RecvBeaconBroadcastData, "RecvBeaconBroadcastData"},
    {0x00100042, SetApplicationData, "SetApplicationData"},
    {0x00110040, nullptr, "GetApplicationData"},
    {0x00120100, Bind, "Bind"},
    {0x00130040, nullptr, "Unbind"},
    {0x001400C0, nullptr, "PullPacket"},
    {0x00150080, nullptr, "SetMaxSendDelay"},
    {0x00170182, SendTo, "SendTo"},
    {0x001A0000, GetChannel, "GetChannel"},
    {0x001B0302, InitializeWithVersion, "InitializeWithVersion"},
    {0x001D0044, BeginHostingNetwork, "BeginHostingNetwork"},
    {0x001E0084, nullptr, "ConnectToNetwork"},
    {0x001F0006, DecryptBeaconData, "DecryptBeaconData"},
    {0x00200040, nullptr, "Flush"},
    {0x00210080, nullptr, "SetProbeResponseParam"},
    {0x00220402, nullptr, "ScanOnConnection"},
};

NWM_UDS::NWM_UDS() {
    connection_status_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NWM::connection_status_event");

    Register(FunctionTable);

    beacon_broadcast_event =
        CoreTiming::RegisterEvent("UDS::BeaconBroadcastCallback", BeaconBroadcastCallback);
}

NWM_UDS::~NWM_UDS() {
    network_info = {};
    bind_node_events.clear();
    connection_status_event = nullptr;
    recv_buffer_memory = nullptr;

    {
        std::lock_guard<std::mutex> lock(connection_status_mutex);
        connection_status = {};
        connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);
    }

    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);
}

} // namespace NWM
} // namespace Service
