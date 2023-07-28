// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

#include "common/common_types.h"

namespace AudioCore {

struct ADTSData {
    u8 header_length = 0;
    bool mpeg2 = false;
    u8 profile = 0;
    u8 channels = 0;
    u8 channel_idx = 0;
    u8 framecount = 0;
    u8 samplerate_idx = 0;
    u32 length = 0;
    u32 samplerate = 0;
};

ADTSData ParseADTS(const u8* buffer);

// last two bytes of MF AAC decoder user data
// see https://docs.microsoft.com/en-us/windows/desktop/medfound/aac-decoder#example-media-types
u16 MFGetAACTag(const ADTSData& input);

} // namespace AudioCore
