// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hw/hw.h"
#include "core/loader/loader.h"
#include "core/memory_setup.h"
#include "core/movie.h"
#ifdef ENABLE_SCRIPTING
#include "core/rpc/rpc_server.h"
#endif
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
                tight_loop = false;
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

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }

    HW::Update();
    Reschedule();

    if (reset_requested.exchange(false)) {
        Reset();
    } else if (shutdown_requested.exchange(false)) {
        return ResultStatus::ShutdownRequested;
    }

    return status;
}

System::ResultStatus System::SingleStep() {
    return RunLoop(false);
}

System::ResultStatus System::Load(EmuWindow& emu_window, const std::string& filepath) {
    app_loader = Loader::GetLoader(filepath);

    if (!app_loader) {
        LOG_CRITICAL(Core, "Failed to obtain loader for {}!", filepath);
        return ResultStatus::ErrorGetLoader;
    }
    std::pair<std::optional<u32>, Loader::ResultStatus> system_mode =
        app_loader->LoadKernelSystemMode();

    if (system_mode.second != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to determine system mode (Error {})!",
                     static_cast<int>(system_mode.second));

        switch (system_mode.second) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorInvalidFormat;
        default:
            return ResultStatus::ErrorSystemMode;
        }
    }

    ASSERT(system_mode.first);
    ResultStatus init_result{Init(emu_window, *system_mode.first)};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                     static_cast<u32>(init_result));
        System::Shutdown();
        return init_result;
    }

    Kernel::SharedPtr<Kernel::Process> process;
    const Loader::ResultStatus load_result{app_loader->Load(process)};
    kernel->SetCurrentProcess(process);
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", static_cast<u32>(load_result));
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
    Memory::SetCurrentPageTable(&kernel->GetCurrentProcess()->vm_manager.page_table);
    status = ResultStatus::Success;
    m_emu_window = &emu_window;
    m_filepath = filepath;
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

System::ResultStatus System::Init(EmuWindow& emu_window, u32 system_mode) {
    LOG_DEBUG(HW_Memory, "initialized OK");

    CoreTiming::Init();

    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        cpu_core = std::make_unique<ARM_Dynarmic>(USER32MODE);
#else
        cpu_core = std::make_unique<ARM_DynCom>(USER32MODE);
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        cpu_core = std::make_unique<ARM_DynCom>(USER32MODE);
    }

    dsp_core = std::make_unique<AudioCore::DspHle>();
    dsp_core->SetSink(Settings::values.sink_id, Settings::values.audio_device_id);
    dsp_core->EnableStretching(Settings::values.enable_audio_stretching);

    telemetry_session = std::make_unique<Core::TelemetrySession>();

#ifdef ENABLE_SCRIPTING
    rpc_server = std::make_unique<RPC::RPCServer>();
#endif

    service_manager = std::make_shared<Service::SM::ServiceManager>(*this);
    shared_page_handler = std::make_shared<SharedPage::Handler>();
    archive_manager = std::make_unique<Service::FS::ArchiveManager>(*this);

    HW::Init();
    kernel = std::make_unique<Kernel::KernelSystem>(system_mode);
    Service::Init(*this);
    GDBStub::Init();

    ResultStatus result = VideoCore::Init(emu_window);
    if (result != ResultStatus::Success) {
        return result;
    }

    LOG_DEBUG(Core, "Initialized OK");

    // Reset counters and set time origin to current frame
    GetAndResetPerfStats();
    perf_stats.BeginSystemFrame();

    return ResultStatus::Success;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *service_manager;
}

Service::FS::ArchiveManager& System::ArchiveManager() {
    return *archive_manager;
}

const Service::FS::ArchiveManager& System::ArchiveManager() const {
    return *archive_manager;
}

Kernel::KernelSystem& System::Kernel() {
    return *kernel;
}

const Kernel::KernelSystem& System::Kernel() const {
    return *kernel;
}

void System::RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd) {
    registered_swkbd = std::move(swkbd);
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
    VideoCore::Shutdown();
    Service::Shutdown();
    kernel.reset();
    HW::Shutdown();
    telemetry_session.reset();
#ifdef ENABLE_SCRIPTING
    rpc_server.reset();
#endif
    service_manager.reset();
    dsp_core.reset();
    cpu_core.reset();
    CoreTiming::Shutdown();
    app_loader.reset();

    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info{};
        room_member->SendGameInfo(game_info);
    }

    LOG_DEBUG(Core, "Shutdown OK");
}

void System::Reset() {
    // This is NOT a proper reset, but a temporary workaround by shutting down the system and
    // reloading.
    // TODO: Properly implement the reset

    Shutdown();
    // Reload the system with the same setting
    Load(*m_emu_window, m_filepath);
}

} // namespace Core
