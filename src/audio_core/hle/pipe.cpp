// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>

#include "audio_core/hle/pipe.h"

#include "common/common_types.h"
#include "common/logging/log.h"

namespace DSP {
namespace HLE {

static size_t pipe2position = 0;

void ResetPipes() {
    pipe2position = 0;
}

std::vector<u8> PipeRead(u32 pipe_number, u32 length) {
    if (pipe_number != 2) {
        LOG_WARNING(Audio_DSP, "pipe_number = %u (!= 2), unimplemented", pipe_number);
        return {}; // We currently don't handle anything other than the audio pipe.
    }

    // Canned DSP responses that games expect. These were taken from HW by 3dmoo team.
    // TODO: Our implementation will actually use a slightly different response than this one.
    // TODO: Use offsetof on DSP structures instead for a proper response.
    static const std::array<u8, 32> canned_response {{
        0x0F, 0x00, 0xFF, 0xBF, 0x8E, 0x9E, 0x80, 0x86, 0x8E, 0xA7, 0x30, 0x94, 0x00, 0x84, 0x40, 0x85,
        0x8E, 0x94, 0x10, 0x87, 0x10, 0x84, 0x0E, 0xA9, 0x0E, 0xAA, 0xCE, 0xAA, 0x4E, 0xAC, 0x58, 0xAC
    }};

    // TODO: Move this into dsp::DSP service since it happens on the service side.
    // Hardware observation: No data is returned if requested length reads beyond the end of the data in-pipe.
    if (pipe2position + length > canned_response.size()) {
        return {};
    }

    std::vector<u8> ret;
    for (size_t i = 0; i < length; i++, pipe2position++) {
        ret.emplace_back(canned_response[pipe2position]);
    }

    return ret;
}

void PipeWrite(u32 pipe_number, const std::vector<u8>& buffer) {
    // TODO: proper pipe behaviour
}

} // namespace HLE
} // namespace DSP
