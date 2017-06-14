// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

namespace Service {
namespace NWM {

enum class SAP : u8 { SNAPExtensionUsed = 0xAA };

enum class PDUControl : u8 { UnnumberedInformation = 3 };

enum class EtherType : u16 { SecureData = 0x876D, EAPoL = 0x888E };

/*
 * 802.2 header, UDS packets always use SNAP for these headers,
 * which means the dsap and ssap are always SNAPExtensionUsed (0xAA)
 * and the OUI is always 0.
 */
struct LLCHeader {
    u8 dsap = static_cast<u8>(SAP::SNAPExtensionUsed);
    u8 ssap = static_cast<u8>(SAP::SNAPExtensionUsed);
    u8 control = static_cast<u8>(PDUControl::UnnumberedInformation);
    std::array<u8, 3> OUI = {};
    u16_be protocol;
};

static_assert(sizeof(LLCHeader) == 8, "LLCHeader has the wrong size");

/*
 * Nintendo SecureData header, every UDS packet contains one,
 * it is used to store metadata about the transmission such as
 * the source and destination network node ids.
 */
struct SecureDataHeader {
    // TODO(Subv): It is likely that the first 4 bytes of this header are
    // actually part of another container protocol.
    u16_be protocol_size;
    INSERT_PADDING_BYTES(2);
    u16_be securedata_size;
    u8 is_management;
    u8 data_channel;
    u16_be sequence_number;
    u16_be dest_node_id;
    u16_be src_node_id;
};

static_assert(sizeof(SecureDataHeader) == 14, "SecureDataHeader has the wrong size");

/*
 * The raw bytes of this structure are the CTR used in the encryption (AES-CTR)
 * process used to generate the CCMP key for data frame encryption.
 */
struct DataFrameCryptoCTR {
    u32_le wlan_comm_id;
    u32_le network_id;
    std::array<u8, 6> host_mac;
    u16_le id;
};

static_assert(sizeof(DataFrameCryptoCTR) == 16, "DataFrameCryptoCTR has the wrong size");

/**
 * Generates an unencrypted 802.11 data payload.
 * @returns The generated frame payload.
 */
std::vector<u8> GenerateDataPayload(const std::vector<u8>& data, u8 channel, u16 dest_node,
                                    u16 src_node, u16 sequence_number);

} // namespace NWM
} // namespace Service
