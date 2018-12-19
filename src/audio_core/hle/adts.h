#pragma once
#ifndef ADTS_ADT
#define ADTS_ADT

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct ADTSData {
    bool MPEG2;
    uint8_t profile;
    uint8_t channels;
    uint8_t channel_idx;
    uint8_t framecount;
    uint8_t samplerate_idx;
    uint32_t length;
    uint32_t samplerate;
};

typedef struct ADTSData ADTSData;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
uint32_t parse_adts(char* buffer, struct ADTSData* out);
// last two bytes of MF AAC decoder user data
uint16_t mf_get_aac_tag(struct ADTSData input);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // ADTS_ADT
