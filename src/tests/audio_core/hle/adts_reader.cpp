// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include "audio_core/hle/adts.h"

namespace {
constexpr std::array<u32, 16> freq_table = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                            16000, 12000, 11025, 8000,  7350,  0,     0,     0};
constexpr std::array<u8, 8> channel_table = {0, 1, 2, 3, 4, 5, 6, 8};

AudioCore::ADTSData ParseADTS_Old(const unsigned char* buffer) {
    u32 tmp = 0;
    AudioCore::ADTSData out{};

    // sync word 0xfff
    tmp = (buffer[0] << 8) | (buffer[1] & 0xf0);
    if ((tmp & 0xffff) != 0xfff0) {
        out.length = 0;
        return out;
    }
    // bit 16 = no CRC
    out.header_length = (buffer[1] & 0x1) ? 7 : 9;
    out.mpeg2 = (buffer[1] >> 3) & 0x1;
    // bit 17 to 18
    out.profile = (buffer[2] >> 6) + 1;
    // bit 19 to 22
    tmp = (buffer[2] >> 2) & 0xf;
    out.samplerate_idx = tmp;
    out.samplerate = (tmp > 15) ? 0 : freq_table[tmp];
    // bit 24 to 26
    tmp = ((buffer[2] & 0x1) << 2) | ((buffer[3] >> 6) & 0x3);
    out.channel_idx = tmp;
    out.channels = (tmp > 7) ? 0 : channel_table[tmp];

    // bit 55 to 56
    out.framecount = (buffer[6] & 0x3) + 1;

    // bit 31 to 43
    tmp = (buffer[3] & 0x3) << 11;
    tmp |= (buffer[4] << 3) & 0x7f8;
    tmp |= (buffer[5] >> 5) & 0x7;

    out.length = tmp;

    return out;
}
} // namespace

TEST_CASE("ParseADTS fuzz", "[audio_core][hle]") {
    for (u32 i = 0; i < 0x10000; i++) {
        std::array<u8, 7> adts_header;
        std::string adts_header_string = "ADTS Header: ";
        for (auto& it : adts_header) {
            it = static_cast<u8>(rand());
            adts_header_string.append(fmt::format("{:2X} ", it));
        }
        INFO(adts_header_string);

        AudioCore::ADTSData out_old_impl =
            ParseADTS_Old(reinterpret_cast<const unsigned char*>(adts_header.data()));
        AudioCore::ADTSData out = AudioCore::ParseADTS(adts_header.data());

        REQUIRE(out_old_impl.length == out.length);
        REQUIRE(out_old_impl.channels == out.channels);
        REQUIRE(out_old_impl.channel_idx == out.channel_idx);
        REQUIRE(out_old_impl.framecount == out.framecount);
        REQUIRE(out_old_impl.header_length == out.header_length);
        REQUIRE(out_old_impl.mpeg2 == out.mpeg2);
        REQUIRE(out_old_impl.profile == out.profile);
        REQUIRE(out_old_impl.samplerate == out.samplerate);
        REQUIRE(out_old_impl.samplerate_idx == out.samplerate_idx);
    }
}
