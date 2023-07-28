// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#include <array>
#include "adts.h"
#include "common/bit_field.h"

namespace AudioCore {
constexpr std::array<u32, 16> freq_table = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                            16000, 12000, 11025, 8000,  7350,  0,     0,     0};
constexpr std::array<u8, 8> channel_table = {0, 1, 2, 3, 4, 5, 6, 8};

struct ADTSHeader {
    union {
        std::array<u8, 7> raw{};
        BitFieldBE<52, 12, u64> sync_word;
        BitFieldBE<51, 1, u64> mpeg2;
        BitFieldBE<49, 2, u64> layer;
        BitFieldBE<48, 1, u64> protection_absent;
        BitFieldBE<46, 2, u64> profile;
        BitFieldBE<42, 4, u64> samplerate_idx;
        BitFieldBE<41, 1, u64> private_bit;
        BitFieldBE<38, 3, u64> channel_idx;
        BitFieldBE<37, 1, u64> originality;
        BitFieldBE<36, 1, u64> home;
        BitFieldBE<35, 1, u64> copyright_id;
        BitFieldBE<34, 1, u64> copyright_id_start;
        BitFieldBE<21, 13, u64> frame_length;
        BitFieldBE<10, 11, u64> buffer_fullness;
        BitFieldBE<8, 2, u64> frame_count;
    };
};

ADTSData ParseADTS(const u8* buffer) {
    ADTSHeader header;
    memcpy(header.raw.data(), buffer, sizeof(header.raw));

    // sync word 0xfff
    if (header.sync_word != 0xfff) {
        return {};
    }

    ADTSData out{};
    // bit 16 = no CRC
    out.header_length = header.protection_absent ? 7 : 9;
    out.mpeg2 = static_cast<bool>(header.mpeg2);
    // bit 17 to 18
    out.profile = static_cast<u8>(header.profile) + 1;
    // bit 19 to 22
    out.samplerate_idx = static_cast<u8>(header.samplerate_idx);
    out.samplerate = header.samplerate_idx > 15 ? 0 : freq_table[header.samplerate_idx];
    // bit 24 to 26
    out.channel_idx = static_cast<u8>(header.channel_idx);
    out.channels = (header.channel_idx > 7) ? 0 : channel_table[header.channel_idx];
    // bit 55 to 56
    out.framecount = static_cast<u8>(header.frame_count + 1);
    // bit 31 to 43
    out.length = static_cast<u32>(header.frame_length);

    return out;
}

// last two bytes of MF AAC decoder user data
// Audio object type (5 bits)
// Sample rate profile (4 bits)
// Channel configuration profile (4 bits)
// Frame length flag (1 bit)
// Depends on core coder (1 bit)
// Extension flag (1 bit)
u16 MFGetAACTag(const ADTSData& input) {
    u16 tag = 0;

    tag |= input.profile << 11;
    tag |= input.samplerate_idx << 7;
    tag |= input.channel_idx << 3;

    return tag;
}
} // namespace AudioCore
