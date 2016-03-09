// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/audio_core.h"

#include "core/core.h"
#include "core/core_timing.h"
#include "core/system.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hw/hw.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"

#include "video_core/video_core.h"

namespace System {

Result Init(EmuWindow* emu_window) {
    Core::Init();
    CoreTiming::Init();
    Memory::Init();
    HW::Init();
    Kernel::Init();
    HLE::Init();
    if (!VideoCore::Init(emu_window)) {
        return Result::ErrorInitVideoCore;
    }
    AudioCore::Init();
    GDBStub::Init();

    return Result::Success;
}

void Shutdown() {
    GDBStub::Shutdown();
    AudioCore::Shutdown();
    VideoCore::Shutdown();
    HLE::Shutdown();
    Kernel::Shutdown();
    HW::Shutdown();
    CoreTiming::Shutdown();
    Core::Shutdown();
}

} // namespace
