// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include "audio_core/audio_core.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/service.h"
#include "core/hw/hw.h"
#include "core/loader/loader.h"
#include "core/memory_setup.h"
#include "core/settings.h"
#include "network/network.h"
#include "video_core/video_core.h"

namespace Core {

/*static*/ System System::s_instance;

System::ResultStatus System::RunLoop(bool tight_loop) {
    status = ResultStatus::Success;
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
        CoreTiming::Advance();
        if (tight_loop) {
            cpu_core->Run();
        } else {
            cpu_core->Step();
        }
    }

    HW::Update();
    Reschedule();

    return status;
}

System::ResultStatus System::SingleStep() {
    return RunLoop(false);
}

System::ResultStatus System::Load(EmuWindow* emu_window, const std::string& filepath) {
    app_loader = Loader::GetLoader(filepath);

    if (!app_loader) {
        LOG_CRITICAL(Core, "Failed to obtain loader for %s!", filepath.c_str());
        return ResultStatus::ErrorGetLoader;
    }
    std::pair<boost::optional<u32>, Loader::ResultStatus> system_mode =
        app_loader->LoadKernelSystemMode();

    if (system_mode.second != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to determine system mode (Error %i)!",
                     static_cast<int>(system_mode.second));
        System::Shutdown();

        switch (system_mode.second) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorInvalidFormat;
        default:
            return ResultStatus::ErrorSystemMode;
        }
    }

    ResultStatus init_result{Init(emu_window, system_mode.first.get())};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error %u)!",
                     static_cast<u32>(init_result));
        System::Shutdown();
        return init_result;
    }

    const Loader::ResultStatus load_result{app_loader->Load(Kernel::g_current_process)};
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error %u)!", static_cast<u32>(load_result));
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
    Memory::SetCurrentPageTable(&Kernel::g_current_process->vm_manager.page_table);
    status = ResultStatus::Success;
    return status;
}

void System::PrepareReschedule() {
    cpu_core->PrepareReschedule();
    reschedule_pending = true;
}

PerfStats::Results System::GetAndResetPerfStats() {
    return perf_stats.GetAndResetStats(CoreTiming::GetGlobalTimeUs());
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    Kernel::Reschedule();
}

System::ResultStatus System::Init(EmuWindow* emu_window, u32 system_mode) {
    LOG_DEBUG(HW_Memory, "initialized OK");

    if (Settings::values.use_cpu_jit) {
        cpu_core = std::make_unique<ARM_Dynarmic>(USER32MODE);
    } else {
        cpu_core = std::make_unique<ARM_DynCom>(USER32MODE);
    }

    telemetry_session = std::make_unique<Core::TelemetrySession>();

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

    // Reset counters and set time origin to current frame
    GetAndResetPerfStats();
    perf_stats.BeginSystemFrame();

    return ResultStatus::Success;
}

void System::Shutdown() {
    // Log last frame performance stats
    auto perf_results = GetAndResetPerfStats();
    Telemetry().AddField(Telemetry::FieldType::Performance, "Shutdown_EmulationSpeed",
                         perf_results.emulation_speed * 100.0);
    Telemetry().AddField(Telemetry::FieldType::Performance, "Shutdown_Framerate",
                         perf_results.game_fps);
    Telemetry().AddField(Telemetry::FieldType::Performance, "Shutdown_Frametime",
                         perf_results.frametime * 1000.0);

    // Shutdown emulation session
    GDBStub::Shutdown();
    AudioCore::Shutdown();
    VideoCore::Shutdown();
    Service::Shutdown();
    Kernel::Shutdown();
    HW::Shutdown();
    CoreTiming::Shutdown();
    cpu_core = nullptr;
    app_loader = nullptr;
    telemetry_session = nullptr;
    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info{};
        room_member->SendGameInfo(game_info);
    }

    LOG_DEBUG(Core, "Shutdown OK");
}

} // namespace Core
