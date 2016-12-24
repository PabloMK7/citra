// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "audio_core/audio_core.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/service.h"
#include "core/hw/hw.h"
#include "core/loader/loader.h"
#include "core/settings.h"
#include "video_core/video_core.h"

namespace Core {

/*static*/ System System::s_instance;

System::ResultStatus System::RunLoop(int tight_loop) {
    if (!cpu_core) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (GDBStub::IsServerEnabled()) {
        GDBStub::HandlePacket();

        // If the loop is halted and we want to step, use a tiny (1) number of instructions to
        // execute. Otherwise, get out of the loop function.
        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                GDBStub::SetCpuStepFlag(false);
                tight_loop = 1;
            } else {
                return ResultStatus::Success;
            }
        }
    }

    // If we don't have a currently active thread then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (Kernel::GetCurrentThread() == nullptr) {
        LOG_TRACE(Core_ARM11, "Idling");
        CoreTiming::Idle();
        CoreTiming::Advance();
        PrepareReschedule();
    } else {
        cpu_core->Run(tight_loop);
    }

    HW::Update();
    Reschedule();

    return ResultStatus::Success;
}

System::ResultStatus System::SingleStep() {
    return RunLoop(1);
}

System::ResultStatus System::Load(EmuWindow* emu_window, const std::string& filepath) {
    if (app_loader) {
        app_loader.reset();
    }

    app_loader = Loader::GetLoader(filepath);

    if (!app_loader) {
        LOG_CRITICAL(Core, "Failed to obtain loader for %s!", filepath.c_str());
        return ResultStatus::ErrorGetLoader;
    }

    boost::optional<u32> system_mode{app_loader->LoadKernelSystemMode()};
    if (!system_mode) {
        LOG_CRITICAL(Core, "Failed to determine system mode!");
        return ResultStatus::ErrorSystemMode;
    }

    ResultStatus init_result{Init(emu_window, system_mode.get())};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error %i)!", init_result);
        System::Shutdown();
        return init_result;
    }

    const Loader::ResultStatus load_result{app_loader->Load()};
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error %i)!", load_result);
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

void System::PrepareReschedule() {
    cpu_core->PrepareReschedule();
    reschedule_pending = true;
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    Kernel::Reschedule();
}

System::ResultStatus System::Init(EmuWindow* emu_window, u32 system_mode) {
    if (cpu_core) {
        cpu_core.reset();
    }

    Memory::Init();

    if (Settings::values.use_cpu_jit) {
        cpu_core = std::make_unique<ARM_Dynarmic>(USER32MODE);
    } else {
        cpu_core = std::make_unique<ARM_DynCom>(USER32MODE);
    }

    CoreTiming::Init();
    HW::Init();
    Kernel::Init(system_mode);
    Service::Init();
    AudioCore::Init();
    GDBStub::Init();

    if (!VideoCore::Init(emu_window)) {
        return ResultStatus::ErrorVideoCore;
    }

    LOG_DEBUG(Core, "Initialized OK");

    return ResultStatus::Success;
}

void System::Shutdown() {
    GDBStub::Shutdown();
    AudioCore::Shutdown();
    VideoCore::Shutdown();
    Service::Shutdown();
    Kernel::Shutdown();
    HW::Shutdown();
    CoreTiming::Shutdown();
    cpu_core.reset();

    LOG_DEBUG(Core, "Shutdown OK");
}

} // namespace
