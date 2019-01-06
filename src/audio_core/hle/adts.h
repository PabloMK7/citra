// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

#include <array>
#include "common/common_types.h"

struct ADTSData {
    bool MPEG2;
    u8 profile;
    u8 channels;
    u8 channel_idx;
    u8 framecount;
    u8 samplerate_idx;
    u32 length;
    u32 samplerate;
};

u32 parse_adts(char* buffer, struct ADTSData* out);
// last two bytes of MF AAC decoder user data
u16 mf_get_aac_tag(struct ADTSData input);
