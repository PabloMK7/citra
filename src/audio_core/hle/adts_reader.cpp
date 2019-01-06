// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#include "adts.h"

constexpr std::array<u32, 16> freq_table = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                 16000, 12000, 11025, 8000,  7350,  0,     0,     0};
constexpr std::array<u8, 8> channel_table = {0, 1, 2, 3, 4, 5, 6, 8};

u32 parse_adts(char* buffer, struct ADTSData* out) {
    u32 tmp = 0;

    // sync word 0xfff
    tmp = (buffer[0] << 8) | (buffer[1] & 0xf0);
    if ((tmp & 0xffff) != 0xfff0)
        return 0;
    out->MPEG2 = (buffer[1] >> 3) & 0x1;
    // bit 17 to 18
    out->profile = (buffer[2] >> 6) + 1;
    // bit 19 to 22
    tmp = (buffer[2] >> 2) & 0xf;
    out->samplerate_idx = tmp;
    out->samplerate = (tmp > 15) ? 0 : freq_table[tmp];
    // bit 24 to 26
    tmp = ((buffer[2] & 0x1) << 2) | ((buffer[3] >> 6) & 0x3);
    out->channel_idx = tmp;
    out->channels = (tmp > 7) ? 0 : channel_table[tmp];

    // bit 55 to 56
    out->framecount = (buffer[6] & 0x3) + 1;

    // bit 31 to 43
    tmp = (buffer[3] & 0x3) << 11;
    tmp |= (buffer[4] << 3) & 0x7f8;
    tmp |= (buffer[5] >> 5) & 0x7;

    out->length = tmp;

    return tmp;
}

// last two bytes of MF AAC decoder user data
u16 mf_get_aac_tag(struct ADTSData input) {
    u16 tag = 0;

    tag |= input.profile << 11;
    tag |= input.samplerate_idx << 7;
    tag |= input.channel_idx << 3;

    return tag;
}
