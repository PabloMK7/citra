#include <algorithm>
#include <cstring>

#include "core/rpc/packet.h"

namespace RPC {

Packet::Packet(const PacketHeader& header, u8* data,
               std::function<void(Packet&)> send_reply_callback)
    : header(header), send_reply_callback(std::move(send_reply_callback)) {

    std::memcpy(packet_data.data(), data, std::min(header.packet_size, MAX_PACKET_DATA_SIZE));
}

}; // namespace RPC
