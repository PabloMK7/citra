// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

// Local-WLAN service

namespace Service::NWM {

const std::size_t ApplicationDataSize = 0xC8;
const u8 DefaultNetworkChannel = 11;

// Number of milliseconds in a TU.
const double MillisecondsPerTU = 1.024;
// Interval measured in TU, the default value is 100TU = 102.4ms
const u16 DefaultBeaconInterval = 100;

/// The maximum number of nodes that can exist in an UDS session.
constexpr u32 UDSMaxNodes = 16;

struct NodeInfo {
    u64_le friend_code_seed;
    std::array<u16_le, 10> username;
    INSERT_PADDING_BYTES(4);
    u16_le network_node_id;
    INSERT_PADDING_BYTES(6);

    void Reset() {
        friend_code_seed = 0;
        username.fill(0);
        network_node_id = 0;
    }
};

static_assert(sizeof(NodeInfo) == 40, "NodeInfo has incorrect size.");

using NodeList = std::vector<NodeInfo>;

enum class NetworkStatus {
    NotConnected = 3,
    ConnectedAsHost = 6,
    Connecting = 7,
    ConnectedAsClient = 9,
    ConnectedAsSpectator = 10,
};

struct ConnectionStatus {
    u32_le status;
    INSERT_PADDING_WORDS(1);
    u16_le network_node_id;
    u16_le changed_nodes;
    u16_le nodes[UDSMaxNodes];
    u8 total_nodes;
    u8 max_nodes;
    u16_le node_bitmask;
};

static_assert(sizeof(ConnectionStatus) == 0x30, "ConnectionStatus has incorrect size.");

struct NetworkInfo {
    std::array<u8, 6> host_mac_address;
    u8 channel;
    INSERT_PADDING_BYTES(1);
    u8 initialized;
    INSERT_PADDING_BYTES(3);
    std::array<u8, 3> oui_value;
    u8 oui_type;
    // This field is received as BigEndian from the game.
    u32_be wlan_comm_id;
    u8 id;
    INSERT_PADDING_BYTES(1);
    u16_be attributes;
    u32_be network_id;
    u8 total_nodes;
    u8 max_nodes;
    INSERT_PADDING_BYTES(2);
    INSERT_PADDING_BYTES(0x1F);
    u8 application_data_size;
    std::array<u8, ApplicationDataSize> application_data;
};

static_assert(offsetof(NetworkInfo, oui_value) == 0xC, "oui_value is at the wrong offset.");
static_assert(offsetof(NetworkInfo, wlan_comm_id) == 0x10, "wlancommid is at the wrong offset.");
static_assert(sizeof(NetworkInfo) == 0x108, "NetworkInfo has incorrect size.");

/// Additional block tag ids in the Beacon and Association Response frames
enum class TagId : u8 {
    SSID = 0,
    SupportedRates = 1,
    DSParameterSet = 2,
    TrafficIndicationMap = 5,
    CountryInformation = 7,
    ERPInformation = 42,
    VendorSpecific = 221
};

class NWM_UDS final : public ServiceFramework<NWM_UDS> {
public:
    explicit NWM_UDS(Core::System& system);
    ~NWM_UDS();

private:
    Core::System& system;

    void UpdateNetworkAttribute(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Shutdown service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::DestroyNetwork service function.
     * Closes the network that we're currently hosting.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DestroyNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::DisconnectNetwork service function.
     * This disconnects this device from the network.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DisconnectNetwork(Kernel::HLERequestContext& ctx);

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
    void GetConnectionStatus(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::GetNodeInformation service function.
     * Returns the node inforamtion structure for the currently connected node.
     *  Inputs:
     *      0 : Command header.
     *      1 : Node ID.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-11 : NodeInfo structure.
     */
    void GetNodeInformation(Kernel::HLERequestContext& ctx);

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
     *      2, 3: output buffer return descriptor & ptr
     */
    void RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx);

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
    void SetApplicationData(Kernel::HLERequestContext& ctx);

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
    void Bind(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Unbind service function.
     * Unbinds a BindNodeId from a data channel.
     *  Inputs:
     *      1 : BindNodeId
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Unbind(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::PullPacket service function.
     * Receives a data frame from the specified bind node id
     *  Inputs:
     *      0 : Command header.
     *      1 : Bind node id.
     *      2 : Max out buff size >> 2.
     *      3 : Max out buff size.
     *     64 : Output buffer descriptor
     *     65 : Output buffer address
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Received data size
     *      3 : u16 Source network node id
     *      4 : Buffer descriptor
     *      5 : Buffer address
     */
    void PullPacket(Kernel::HLERequestContext& ctx);

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
    void SendTo(Kernel::HLERequestContext& ctx);

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
    void GetChannel(Kernel::HLERequestContext& ctx);

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
    void InitializeWithVersion(Kernel::HLERequestContext& ctx);

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
    void BeginHostingNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::ConnectToNetwork service function.
     * This connects to the specified network
     *  Inputs:
     *      0 : Command header
     *      1 : Connection type: 0x1 = Client, 0x2 = Spectator.
     *      2 : Passphrase buffer size
     *      3 : (NetworkStructSize<<12) | 0x402
     *      4 : Network struct buffer ptr
     *      5 : (PassphraseSize<<12) | 2
     *      6 : Input passphrase buffer ptr
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ConnectToNetwork(Kernel::HLERequestContext& ctx);

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
     *      2, 3: output buffer return descriptor & ptr
     */
    void DecryptBeaconData(Kernel::HLERequestContext& ctx);
};

} // namespace Service::NWM
