// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "audio_core/audio_core.h"
#include "audio_core/hle/dsp.h"
#include "audio_core/hle/pipe.h"
#include "audio_core/null_sink.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"

#include "core/core_timing.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/dsp_dsp.h"

namespace AudioCore {

// Audio Ticks occur about every 5 miliseconds.
static int tick_event;                               ///< CoreTiming event
static constexpr u64 audio_frame_ticks = 1310252ull; ///< Units: ARM11 cycles

static void AudioTickCallback(u64 /*userdata*/, int cycles_late) {
    if (DSP::HLE::Tick()) {
        // TODO(merry): Signal all the other interrupts as appropriate.
        DSP_DSP::SignalPipeInterrupt(DSP::HLE::DspPipe::Audio);
        // HACK(merry): Added to prevent regressions. Will remove soon.
        DSP_DSP::SignalPipeInterrupt(DSP::HLE::DspPipe::Binary);
    }

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(audio_frame_ticks - cycles_late, tick_event);
}

void Init() {
    DSP::HLE::Init();

    tick_event = CoreTiming::RegisterEvent("AudioCore::tick_event", AudioTickCallback);
    CoreTiming::ScheduleEvent(audio_frame_ticks, tick_event);
}

void AddAddressSpace(Kernel::VMManager& address_space) {
    auto r0_vma = address_space.MapBackingMemory(DSP::HLE::region0_base, reinterpret_cast<u8*>(&DSP::HLE::g_regions[0]), sizeof(DSP::HLE::SharedMemory), Kernel::MemoryState::IO).MoveFrom();
    address_space.Reprotect(r0_vma, Kernel::VMAPermission::ReadWrite);

    auto r1_vma = address_space.MapBackingMemory(DSP::HLE::region1_base, reinterpret_cast<u8*>(&DSP::HLE::g_regions[1]), sizeof(DSP::HLE::SharedMemory), Kernel::MemoryState::IO).MoveFrom();
    address_space.Reprotect(r1_vma, Kernel::VMAPermission::ReadWrite);
}

void SelectSink(std::string sink_id) {
    if (sink_id == "auto") {
        // Auto-select.
        // g_sink_details is ordered in terms of desirability, with the best choice at the front.
        const auto& sink_detail = g_sink_details.front();
        DSP::HLE::SetSink(sink_detail.factory());
        return;
    }

    auto iter = std::find_if(g_sink_details.begin(), g_sink_details.end(), [sink_id](const auto& sink_detail) {
        return sink_detail.id == sink_id;
    });

    if (iter == g_sink_details.end()) {
        LOG_ERROR(Audio, "AudioCore::SelectSink given invalid sink_id");
        DSP::HLE::SetSink(std::make_unique<NullSink>());
        return;
    }

    DSP::HLE::SetSink(iter->factory());
}

void EnableStretching(bool enable) {
    DSP::HLE::EnableStretching(enable);
}

void Shutdown() {
    CoreTiming::UnscheduleEvent(tick_event, 0);
    DSP::HLE::Shutdown();
}

} // namespace AudioCore
