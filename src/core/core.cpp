// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <boost/serialization/array.hpp>
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "audio_core/lle/lle.h"
#include "common/logging/log.h"
#include "common/texture.h"
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/dumping/backend.h"
#ifdef ENABLE_FFMPEG_VIDEO_DUMPER
#include "core/dumping/ffmpeg_backend.h"
#endif
#include "core/custom_tex_cache.h"
#include "core/gdbstub/gdbstub.h"
#include "core/global.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/pm/pm_app.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/rpc/rpc_server.h"
#include "core/settings.h"
#include "network/network.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Core {

/*static*/ System System::s_instance;

template <>
Core::System& Global() {
    return System::GetInstance();
}

template <>
Kernel::KernelSystem& Global() {
    return System::GetInstance().Kernel();
}

template <>
Core::Timing& Global() {
    return System::GetInstance().CoreTiming();
}

System::~System() = default;

System::ResultStatus System::RunLoop(bool tight_loop) {
    status = ResultStatus::Success;
    if (std::any_of(cpu_cores.begin(), cpu_cores.end(),
                    [](std::shared_ptr<ARM_Interface> ptr) { return ptr == nullptr; })) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (GDBStub::IsServerEnabled()) {
        Kernel::Thread* thread = kernel->GetCurrentThreadManager().GetCurrentThread();
        if (thread && running_core) {
            running_core->SaveContext(thread->context);
        }
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

    Signal signal{Signal::None};
    u32 param{};
    {
        std::lock_guard lock{signal_mutex};
        if (current_signal != Signal::None) {
            signal = current_signal;
            param = signal_param;
            current_signal = Signal::None;
        }
    }
    switch (signal) {
    case Signal::Reset:
        Reset();
        return ResultStatus::Success;
    case Signal::Shutdown:
        return ResultStatus::ShutdownRequested;
    case Signal::Load: {
        LOG_INFO(Core, "Begin load");
        try {
            System::LoadState(param);
            LOG_INFO(Core, "Load completed");
        } catch (const std::exception& e) {
            LOG_ERROR(Core, "Error loading: {}", e.what());
            status_details = e.what();
            return ResultStatus::ErrorSavestate;
        }
        frame_limiter.WaitOnce();
        return ResultStatus::Success;
    }
    case Signal::Save: {
        LOG_INFO(Core, "Begin save");
        try {
            System::SaveState(param);
            LOG_INFO(Core, "Save completed");
        } catch (const std::exception& e) {
            LOG_ERROR(Core, "Error saving: {}", e.what());
            status_details = e.what();
            return ResultStatus::ErrorSavestate;
        }
        frame_limiter.WaitOnce();
        return ResultStatus::Success;
    }
    default:
        break;
    }

    // All cores should have executed the same amount of ticks. If this is not the case an event was
    // scheduled with a cycles_into_future smaller then the current downcount.
    // So we have to get those cores to the same global time first
    u64 global_ticks = timing->GetGlobalTicks();
    s64 max_delay = 0;
    std::shared_ptr<ARM_Interface> current_core_to_execute = nullptr;
    for (auto& cpu_core : cpu_cores) {
        if (cpu_core->GetTimer()->GetTicks() < global_ticks) {
            s64 delay = global_ticks - cpu_core->GetTimer()->GetTicks();
            cpu_core->GetTimer()->Advance(delay);
            if (max_delay < delay) {
                max_delay = delay;
                current_core_to_execute = cpu_core;
            }
        }
    }

    if (max_delay > 0) {
        LOG_TRACE(Core_ARM11, "Core {} running (delayed) for {} ticks",
                  current_core_to_execute->GetID(),
                  current_core_to_execute->GetTimer()->GetDowncount());
        running_core = current_core_to_execute.get();
        kernel->SetRunningCPU(current_core_to_execute);
        if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
            LOG_TRACE(Core_ARM11, "Core {} idling", current_core_to_execute->GetID());
            current_core_to_execute->GetTimer()->Idle();
            PrepareReschedule();
        } else {
            if (tight_loop) {
                current_core_to_execute->Run();
            } else {
                current_core_to_execute->Step();
            }
        }
    } else {
        // Now all cores are at the same global time. So we will run them one after the other
        // with a max slice that is the minimum of all max slices of all cores
        // TODO: Make special check for idle since we can easily revert the time of idle cores
        s64 max_slice = Timing::MAX_SLICE_LENGTH;
        for (const auto& cpu_core : cpu_cores) {
            max_slice = std::min(max_slice, cpu_core->GetTimer()->GetMaxSliceLength());
        }
        for (auto& cpu_core : cpu_cores) {
            cpu_core->GetTimer()->Advance(max_slice);
        }
        for (auto& cpu_core : cpu_cores) {
            LOG_TRACE(Core_ARM11, "Core {} running for {} ticks", cpu_core->GetID(),
                      cpu_core->GetTimer()->GetDowncount());
            running_core = cpu_core.get();
            kernel->SetRunningCPU(cpu_core);
            // If we don't have a currently active thread then don't execute instructions,
            // instead advance to the next event and try to yield to the next thread
            if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
                LOG_TRACE(Core_ARM11, "Core {} idling", cpu_core->GetID());
                cpu_core->GetTimer()->Idle();
                PrepareReschedule();
            } else {
                if (tight_loop) {
                    cpu_core->Run();
                } else {
                    cpu_core->Step();
                }
            }
        }
        timing->AddToGlobalTicks(max_slice);
    }

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }

    HW::Update();
    Reschedule();

    return status;
}

bool System::SendSignal(System::Signal signal, u32 param) {
    std::lock_guard lock{signal_mutex};
    if (current_signal != signal && current_signal != Signal::None) {
        LOG_ERROR(Core, "Unable to {} as {} is ongoing", signal, current_signal);
        return false;
    }
    current_signal = signal;
    signal_param = param;
    return true;
}

System::ResultStatus System::SingleStep() {
    return RunLoop(false);
}

System::ResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath) {
    FileUtil::SetCurrentRomPath(filepath);
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
    auto n3ds_mode = app_loader->LoadKernelN3dsMode();
    ASSERT(n3ds_mode.first);
    u32 num_cores = 2;
    if (Settings::values.is_new_3ds) {
        num_cores = 4;
    }
    ResultStatus init_result{Init(emu_window, *system_mode.first, *n3ds_mode.first, num_cores)};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                     static_cast<u32>(init_result));
        System::Shutdown();
        return init_result;
    }

    telemetry_session->AddInitialInfo(*app_loader);
    std::shared_ptr<Kernel::Process> process;
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
    cheat_engine = std::make_unique<Cheats::CheatEngine>(*this);
    title_id = 0;
    if (app_loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        LOG_ERROR(Core, "Failed to find title id for ROM (Error {})",
                  static_cast<u32>(load_result));
    }
    perf_stats = std::make_unique<PerfStats>(title_id);
    custom_tex_cache = std::make_unique<Core::CustomTexCache>();
    if (Settings::values.custom_textures) {
        FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                             FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                             Kernel().GetCurrentProcess()->codeset->program_id));
        custom_tex_cache->FindCustomTextures();
    }
    if (Settings::values.preload_textures)
        custom_tex_cache->PreloadTextures();
    status = ResultStatus::Success;
    m_emu_window = &emu_window;
    m_filepath = filepath;

    // Reset counters and set time origin to current frame
    GetAndResetPerfStats();
    perf_stats->BeginSystemFrame();
    return status;
}

void System::PrepareReschedule() {
    running_core->PrepareReschedule();
    reschedule_pending = true;
}

PerfStats::Results System::GetAndResetPerfStats() {
    return (perf_stats && timing) ? perf_stats->GetAndResetStats(timing->GetGlobalTimeUs())
                                  : PerfStats::Results{};
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    for (const auto& core : cpu_cores) {
        LOG_TRACE(Core_ARM11, "Reschedule core {}", core->GetID());
        kernel->GetThreadManager(core->GetID()).Reschedule();
    }
}

System::ResultStatus System::Init(Frontend::EmuWindow& emu_window, u32 system_mode, u8 n3ds_mode,
                                  u32 num_cores) {
    LOG_DEBUG(HW_Memory, "initialized OK");

    memory = std::make_unique<Memory::MemorySystem>();

    timing = std::make_unique<Timing>(num_cores, Settings::values.cpu_clock_percentage);

    kernel = std::make_unique<Kernel::KernelSystem>(
        *memory, *timing, [this] { PrepareReschedule(); }, system_mode, num_cores, n3ds_mode);

    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(
                std::make_shared<ARM_Dynarmic>(this, *memory, i, timing->GetTimer(i)));
        }
#else
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(this, *memory, USER32MODE, i, timing->GetTimer(i)));
        }
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(this, *memory, USER32MODE, i, timing->GetTimer(i)));
        }
    }
    running_core = cpu_cores[0].get();

    kernel->SetCPUs(cpu_cores);
    kernel->SetRunningCPU(cpu_cores[0]);

    if (Settings::values.enable_dsp_lle) {
        dsp_core = std::make_unique<AudioCore::DspLle>(*memory,
                                                       Settings::values.enable_dsp_lle_multithread);
    } else {
        dsp_core = std::make_unique<AudioCore::DspHle>(*memory);
    }

    memory->SetDSP(*dsp_core);

    dsp_core->SetSink(Settings::values.sink_id, Settings::values.audio_device_id);
    dsp_core->EnableStretching(Settings::values.enable_audio_stretching);

    telemetry_session = std::make_unique<Core::TelemetrySession>();

    rpc_server = std::make_unique<RPC::RPCServer>();

    service_manager = std::make_unique<Service::SM::ServiceManager>(*this);
    archive_manager = std::make_unique<Service::FS::ArchiveManager>(*this);

    HW::Init(*memory);
    Service::Init(*this);
    GDBStub::DeferStart();

#ifdef ENABLE_FFMPEG_VIDEO_DUMPER
    video_dumper = std::make_unique<VideoDumper::FFmpegBackend>();
#else
    video_dumper = std::make_unique<VideoDumper::NullBackend>();
#endif

    VideoCore::ResultStatus result = VideoCore::Init(emu_window, *memory);
    if (result != VideoCore::ResultStatus::Success) {
        switch (result) {
        case VideoCore::ResultStatus::ErrorGenericDrivers:
            return ResultStatus::ErrorVideoCore_ErrorGenericDrivers;
        case VideoCore::ResultStatus::ErrorBelowGL33:
            return ResultStatus::ErrorVideoCore_ErrorBelowGL33;
        default:
            return ResultStatus::ErrorVideoCore;
        }
    }

    LOG_DEBUG(Core, "Initialized OK");

    initalized = true;

    return ResultStatus::Success;
}

RendererBase& System::Renderer() {
    return *VideoCore::g_renderer;
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

Timing& System::CoreTiming() {
    return *timing;
}

const Timing& System::CoreTiming() const {
    return *timing;
}

Memory::MemorySystem& System::Memory() {
    return *memory;
}

const Memory::MemorySystem& System::Memory() const {
    return *memory;
}

Cheats::CheatEngine& System::CheatEngine() {
    return *cheat_engine;
}

const Cheats::CheatEngine& System::CheatEngine() const {
    return *cheat_engine;
}

VideoDumper::Backend& System::VideoDumper() {
    return *video_dumper;
}

const VideoDumper::Backend& System::VideoDumper() const {
    return *video_dumper;
}

Core::CustomTexCache& System::CustomTexCache() {
    return *custom_tex_cache;
}

const Core::CustomTexCache& System::CustomTexCache() const {
    return *custom_tex_cache;
}

void System::RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector) {
    registered_mii_selector = std::move(mii_selector);
}

void System::RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd) {
    registered_swkbd = std::move(swkbd);
}

void System::RegisterImageInterface(std::shared_ptr<Frontend::ImageInterface> image_interface) {
    registered_image_interface = std::move(image_interface);
}

void System::Shutdown(bool is_deserializing) {
    // Log last frame performance stats
    const auto perf_results = GetAndResetPerfStats();
    telemetry_session->AddField(Telemetry::FieldType::Performance, "Shutdown_EmulationSpeed",
                                perf_results.emulation_speed * 100.0);
    telemetry_session->AddField(Telemetry::FieldType::Performance, "Shutdown_Framerate",
                                perf_results.game_fps);
    telemetry_session->AddField(Telemetry::FieldType::Performance, "Shutdown_Frametime",
                                perf_results.frametime * 1000.0);
    telemetry_session->AddField(Telemetry::FieldType::Performance, "Mean_Frametime_MS",
                                perf_stats->GetMeanFrametime());

    // Shutdown emulation session
    VideoCore::Shutdown();
    HW::Shutdown();
    if (!is_deserializing) {
        GDBStub::Shutdown();
        perf_stats.reset();
        cheat_engine.reset();
        app_loader.reset();
    }
    telemetry_session.reset();
    rpc_server.reset();
    archive_manager.reset();
    service_manager.reset();
    dsp_core.reset();
    cpu_cores.clear();
    kernel.reset();
    timing.reset();

    if (video_dumper->IsDumping()) {
        video_dumper->StopDumping();
    }

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

template <class Archive>
void System::serialize(Archive& ar, const unsigned int file_version) {

    u32 num_cores;
    if (Archive::is_saving::value) {
        num_cores = this->GetNumCores();
    }
    ar& num_cores;

    if (Archive::is_loading::value) {
        // When loading, we want to make sure any lingering state gets cleared out before we begin.
        // Shutdown, but persist a few things between loads...
        Shutdown(true);

        // Re-initialize everything like it was before
        auto system_mode = this->app_loader->LoadKernelSystemMode();
        auto n3ds_mode = this->app_loader->LoadKernelN3dsMode();
        Init(*m_emu_window, *system_mode.first, *n3ds_mode.first, num_cores);
    }

    // flush on save, don't flush on load
    bool should_flush = !Archive::is_loading::value;
    Memory::RasterizerClearAll(should_flush);
    ar&* timing.get();
    for (u32 i = 0; i < num_cores; i++) {
        ar&* cpu_cores[i].get();
    }
    ar&* service_manager.get();
    ar&* archive_manager.get();
    ar& GPU::g_regs;
    ar& LCD::g_regs;

    // NOTE: DSP doesn't like being destroyed and recreated. So instead we do an inline
    // serialization; this means that the DSP Settings need to match for loading to work.
    auto dsp_hle = dynamic_cast<AudioCore::DspHle*>(dsp_core.get());
    if (dsp_hle) {
        ar&* dsp_hle;
    } else {
        throw std::runtime_error("LLE audio not supported for save states");
    }

    ar&* memory.get();
    ar&* kernel.get();
    VideoCore::serialize(ar, file_version);
    if (file_version >= 1) {
        ar& Movie::GetInstance();
    }

    // This needs to be set from somewhere - might as well be here!
    if (Archive::is_loading::value) {
        Service::GSP::SetGlobalModule(*this);
        memory->SetDSP(*dsp_core);
        cheat_engine->Connect();
        VideoCore::g_renderer->Sync();
    }
}

SERIALIZE_IMPL(System)

} // namespace Core
