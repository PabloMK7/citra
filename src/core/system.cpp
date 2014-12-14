// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/core_timing.h"
#include "core/mem_map.h"
#include "core/system.h"
#include "core/hw/hw.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"

#include "video_core/video_core.h"

namespace System {

volatile State g_state;

void UpdateState(State state) {
}

void Init(EmuWindow* emu_window) {
    Core::Init();
    Memory::Init();
    HW::Init();
    Kernel::Init();
    HLE::Init();
    CoreTiming::Init();
    VideoCore::Init(emu_window);
}

void RunLoopFor(int cycles) {
    RunLoopUntil(CoreTiming::GetTicks() + cycles);
}

void RunLoopUntil(u64 global_cycles) {
}

void Shutdown() {
    VideoCore::Shutdown();
    CoreTiming::Shutdown();
    HLE::Shutdown();
    Kernel::Shutdown();
    HW::Shutdown();
    Memory::Shutdown();
    Core::Shutdown();
}

} // namespace
