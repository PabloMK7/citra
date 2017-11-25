// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <memory>
#include <string>
#include "audio_core/audio_core.h"
#include "audio_core/hle/dsp.h"
#include "audio_core/hle/pipe.h"
#include "audio_core/null_sink.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/hle/service/dsp_dsp.h"

namespace AudioCore {

// Audio Ticks occur about every 5 miliseconds.
static CoreTiming::EventType* tick_event;            ///< CoreTiming event
static constexpr u64 audio_frame_ticks = 1310252ull; ///< Units: ARM11 cycles

static void AudioTickCallback(u64 /*userdata*/, int cycles_late) {
    if (DSP::HLE::Tick()) {
        // TODO(merry): Signal all the other interrupts as appropriate.
        Service::DSP_DSP::SignalPipeInterrupt(DSP::HLE::DspPipe::Audio);
        // HACK(merry): Added to prevent regressions. Will remove soon.
        Service::DSP_DSP::SignalPipeInterrupt(DSP::HLE::DspPipe::Binary);
    }

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(audio_frame_ticks - cycles_late, tick_event);
}

void Init() {
    DSP::HLE::Init();

    tick_event = CoreTiming::RegisterEvent("AudioCore::tick_event", AudioTickCallback);
    CoreTiming::ScheduleEvent(audio_frame_ticks, tick_event);
}

std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory() {
    return DSP::HLE::g_dsp_memory.raw_memory;
}

void SelectSink(std::string sink_id) {
    const SinkDetails& sink_details = GetSinkDetails(sink_id);
    DSP::HLE::SetSink(sink_details.factory());
}

void EnableStretching(bool enable) {
    DSP::HLE::EnableStretching(enable);
}

void Shutdown() {
    CoreTiming::UnscheduleEvent(tick_event, 0);
    DSP::HLE::Shutdown();
}

} // namespace AudioCore
