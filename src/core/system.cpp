// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/audio_core.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hw/hw.h"
#include "core/system.h"
#include "video_core/video_core.h"

namespace Core {

/*static*/ System System::s_instance;

System::ResultStatus System::Init(EmuWindow* emu_window, u32 system_mode) {
    Core::Init();
    CoreTiming::Init();
    Memory::Init();
    HW::Init();
    Kernel::Init(system_mode);
    HLE::Init();
    AudioCore::Init();
    GDBStub::Init();

    if (!VideoCore::Init(emu_window)) {
        return ResultStatus::ErrorVideoCore;
    }

    is_powered_on = true;

    return ResultStatus::Success;
}

void System::Shutdown() {
    GDBStub::Shutdown();
    AudioCore::Shutdown();
    VideoCore::Shutdown();
    HLE::Shutdown();
    Kernel::Shutdown();
    HW::Shutdown();
    CoreTiming::Shutdown();
    Core::Shutdown();

    is_powered_on = false;
}

System::ResultStatus System::Load(EmuWindow* emu_window, const std::string& filepath) {
    state.app_loader = Loader::GetLoader(filepath);

    if (!state.app_loader) {
        LOG_CRITICAL(Frontend, "Failed to obtain loader for %s!", filepath.c_str());
        return ResultStatus::ErrorGetLoader;
    }

    boost::optional<u32> system_mode{ state.app_loader->LoadKernelSystemMode() };
    if (!system_mode) {
        LOG_CRITICAL(Frontend, "Failed to determine system mode!");
        return ResultStatus::ErrorSystemMode;
    }

    ResultStatus init_result{ Init(emu_window, system_mode.get()) };
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Frontend, "Failed to initialize system (Error %i)!", init_result);
        System::Shutdown();
        return init_result;
    }

    const Loader::ResultStatus load_result{ state.app_loader->Load() };
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Frontend, "Failed to load ROM (Error %i)!", load_result);
        System::Shutdown();

        switch (load_result) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorInvalidFormat;
        default:
            return ResultStatus::ErrorLoader;
        }
    }
    return ResultStatus::Success;
}

} // namespace Core
