// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>
#include "audio_core/hle/dsp.h"
#include "audio_core/hle/pipe.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/service/dsp_dsp.h"

namespace DSP {
namespace HLE {

static DspState dsp_state = DspState::Off;

static std::array<std::vector<u8>, NUM_DSP_PIPE> pipe_data;

void ResetPipes() {
    for (auto& data : pipe_data) {
        data.clear();
    }
    dsp_state = DspState::Off;
}

std::vector<u8> PipeRead(DspPipe pipe_number, u32 length) {
    const size_t pipe_index = static_cast<size_t>(pipe_number);

    if (pipe_index >= NUM_DSP_PIPE) {
        LOG_ERROR(Audio_DSP, "pipe_number = %zu invalid", pipe_index);
        return {};
    }

    if (length > UINT16_MAX) { // Can only read at most UINT16_MAX from the pipe
        LOG_ERROR(Audio_DSP, "length of %u greater than max of %u", length, UINT16_MAX);
        return {};
    }

    std::vector<u8>& data = pipe_data[pipe_index];

    if (length > data.size()) {
        LOG_WARNING(
            Audio_DSP,
            "pipe_number = %zu is out of data, application requested read of %u but %zu remain",
            pipe_index, length, data.size());
        length = static_cast<u32>(data.size());
    }

    if (length == 0)
        return {};

    std::vector<u8> ret(data.begin(), data.begin() + length);
    data.erase(data.begin(), data.begin() + length);
    return ret;
}

size_t GetPipeReadableSize(DspPipe pipe_number) {
    const size_t pipe_index = static_cast<size_t>(pipe_number);

    if (pipe_index >= NUM_DSP_PIPE) {
        LOG_ERROR(Audio_DSP, "pipe_number = %zu invalid", pipe_index);
        return 0;
    }

    return pipe_data[pipe_index].size();
}

static void WriteU16(DspPipe pipe_number, u16 value) {
    const size_t pipe_index = static_cast<size_t>(pipe_number);

    std::vector<u8>& data = pipe_data.at(pipe_index);
    // Little endian
    data.emplace_back(value & 0xFF);
    data.emplace_back(value >> 8);
}

static void AudioPipeWriteStructAddresses() {
    // These struct addresses are DSP dram addresses.
    // See also: DSP_DSP::ConvertProcessAddressFromDspDram
    static const std::array<u16, 15> struct_addresses = {
        0x8000 + offsetof(SharedMemory, frame_counter) / 2,
        0x8000 + offsetof(SharedMemory, source_configurations) / 2,
        0x8000 + offsetof(SharedMemory, source_statuses) / 2,
        0x8000 + offsetof(SharedMemory, adpcm_coefficients) / 2,
        0x8000 + offsetof(SharedMemory, dsp_configuration) / 2,
        0x8000 + offsetof(SharedMemory, dsp_status) / 2,
        0x8000 + offsetof(SharedMemory, final_samples) / 2,
        0x8000 + offsetof(SharedMemory, intermediate_mix_samples) / 2,
        0x8000 + offsetof(SharedMemory, compressor) / 2,
        0x8000 + offsetof(SharedMemory, dsp_debug) / 2,
        0x8000 + offsetof(SharedMemory, unknown10) / 2,
        0x8000 + offsetof(SharedMemory, unknown11) / 2,
        0x8000 + offsetof(SharedMemory, unknown12) / 2,
        0x8000 + offsetof(SharedMemory, unknown13) / 2,
        0x8000 + offsetof(SharedMemory, unknown14) / 2,
    };

    // Begin with a u16 denoting the number of structs.
    WriteU16(DspPipe::Audio, static_cast<u16>(struct_addresses.size()));
    // Then write the struct addresses.
    for (u16 addr : struct_addresses) {
        WriteU16(DspPipe::Audio, addr);
    }
    // Signal that we have data on this pipe.
    DSP_DSP::SignalPipeInterrupt(DspPipe::Audio);
}

void PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) {
    switch (pipe_number) {
    case DspPipe::Audio: {
        if (buffer.size() != 4) {
            LOG_ERROR(Audio_DSP, "DspPipe::Audio: Unexpected buffer length %zu was written",
                      buffer.size());
            return;
        }

        enum class StateChange {
            Initialize = 0,
            Shutdown = 1,
            Wakeup = 2,
            Sleep = 3,
        };

        // The difference between Initialize and Wakeup is that Input state is maintained
        // when sleeping but isn't when turning it off and on again. (TODO: Implement this.)
        // Waking up from sleep garbles some of the structs in the memory region. (TODO:
        // Implement this.) Applications store away the state of these structs before
        // sleeping and reset it back after wakeup on behalf of the DSP.

        switch (static_cast<StateChange>(buffer[0])) {
        case StateChange::Initialize:
            LOG_INFO(Audio_DSP, "Application has requested initialization of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Shutdown:
            LOG_INFO(Audio_DSP, "Application has requested shutdown of DSP hardware");
            dsp_state = DspState::Off;
            break;
        case StateChange::Wakeup:
            LOG_INFO(Audio_DSP, "Application has requested wakeup of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Sleep:
            LOG_INFO(Audio_DSP, "Application has requested sleep of DSP hardware");
            UNIMPLEMENTED();
            dsp_state = DspState::Sleeping;
            break;
        default:
            LOG_ERROR(Audio_DSP,
                      "Application has requested unknown state transition of DSP hardware %hhu",
                      buffer[0]);
            dsp_state = DspState::Off;
            break;
        }

        return;
    }
    default:
        LOG_CRITICAL(Audio_DSP, "pipe_number = %zu unimplemented",
                     static_cast<size_t>(pipe_number));
        UNIMPLEMENTED();
        return;
    }
}

DspState GetDspState() {
    return dsp_state;
}

} // namespace HLE
} // namespace DSP
