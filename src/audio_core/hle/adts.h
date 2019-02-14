// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

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

ADTSData ParseADTS(const char* buffer);

// last two bytes of MF AAC decoder user data
// see https://docs.microsoft.com/en-us/windows/desktop/medfound/aac-decoder#example-media-types
u16 MFGetAACTag(const ADTSData& input);
