
#include "adts.h"

const uint32_t freq_table[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                 16000, 12000, 11025, 8000,  7350,  0,     0,     0};
const short channel_table[8] = {0, 1, 2, 3, 4, 5, 6, 8};

uint32_t parse_adts(char* buffer, struct ADTSData* out) {
    uint32_t tmp = 0;

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
uint16_t mf_get_aac_tag(struct ADTSData input) {
    uint16_t tag = 0;

    tag |= input.profile << 11;
    tag |= input.samplerate_idx << 7;
    tag |= input.channel_idx << 3;

    return tag;
}
