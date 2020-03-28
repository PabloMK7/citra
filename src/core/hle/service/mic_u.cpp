// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend/mic.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/mic_u.h"
#include "core/settings.h"

SERVICE_CONSTRUCT_IMPL(Service::MIC::MIC_U)
SERIALIZE_EXPORT_IMPL(Service::MIC::MIC_U)

namespace Service::MIC {

template <class Archive>
void MIC_U::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar&* impl.get();
}

/// Microphone audio encodings.
enum class Encoding : u8 {
    PCM8 = 0,        ///< Unsigned 8-bit PCM.
    PCM16 = 1,       ///< Unsigned 16-bit PCM.
    PCM8Signed = 2,  ///< Signed 8-bit PCM.
    PCM16Signed = 3, ///< Signed 16-bit PCM.
};

/// Microphone audio sampling rates. The actual accurate sampling rate can be calculated using
/// (16756991 / 512) / (SampleRate + 1) where SampleRate is one of the above values.
enum class SampleRate : u8 {
    Rate32730 = 0, ///< 32728.498 Hz
    Rate16360 = 1, ///< 16364.479 Hz
    Rate10910 = 2, ///< 10909.499 Hz
    Rate8180 = 3   ///< 8182.1245 Hz
};

constexpr u32 GetSampleRateInHz(SampleRate sample_rate) {
    switch (sample_rate) {
    case SampleRate::Rate8180:
        return 8182;
    case SampleRate::Rate10910:
        return 10909;
    case SampleRate::Rate16360:
        return 16364;
    case SampleRate::Rate32730:
        return 32728;
    default:
        UNREACHABLE();
    }
}

// The 3ds hardware was tested to write to the sharedmem every 15 samples regardless of sample_rate.
// So we can just divide the sample rate by 16 and that'll give the correct timing for the event
constexpr u64 GetBufferUpdateRate(SampleRate sample_rate) {
    return GetSampleRateInHz(sample_rate) / 16;
}

// Variables holding the current mic buffer writing state
struct State {
    std::shared_ptr<Kernel::SharedMemory> memory_ref = nullptr;
    u8* sharedmem_buffer = nullptr;
    u32 sharedmem_size = 0;
    std::size_t size = 0;
    u32 offset = 0;
    u32 initial_offset = 0;
    bool looped_buffer = false;
    u8 sample_size = 0;
    SampleRate sample_rate = SampleRate::Rate16360;

    void WriteSamples(const std::vector<u8>& samples) {
        u32 bytes_total_written = 0;
        const std::size_t remaining_space = size - offset;
        std::size_t bytes_to_write = std::min(samples.size(), remaining_space);

        // Write as many samples as we can to the buffer.
        // TODO if the sample size is 16bit, this could theoretically cut a sample in the case where
        // the application configures an odd size
        std::memcpy(sharedmem_buffer + offset, samples.data(), bytes_to_write);
        offset += static_cast<u32>(bytes_to_write);
        bytes_total_written += static_cast<u32>(bytes_to_write);

        // If theres any samples left to write after we looped, go ahead and write them now
        if (looped_buffer && samples.size() > bytes_total_written) {
            offset = initial_offset;
            bytes_to_write = std::min(samples.size() - bytes_total_written, size);
            std::memcpy(sharedmem_buffer + offset, samples.data() + bytes_total_written,
                        bytes_to_write);
            offset += static_cast<u32>(bytes_to_write);
        }

        // The last 4 bytes of the shared memory contains the latest offset
        // so update that as well https://www.3dbrew.org/wiki/MIC_Shared_Memory
        u32_le off = offset;
        std::memcpy(sharedmem_buffer + (sharedmem_size - sizeof(u32)), reinterpret_cast<u8*>(&off),
                    sizeof(u32));
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& memory_ref;
        ar& sharedmem_size;
        ar& size;
        ar& offset;
        ar& initial_offset;
        ar& looped_buffer;
        ar& sample_size;
        ar& sample_rate;
        sharedmem_buffer = memory_ref ? memory_ref->GetPointer() : nullptr;
    }
    friend class boost::serialization::access;
};

struct MIC_U::Impl {
    explicit Impl(Core::System& system) : timing(system.CoreTiming()) {
        buffer_full_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "MIC_U::buffer_full_event");
        buffer_write_event =
            timing.RegisterEvent("MIC_U::UpdateBuffer", [this](u64 userdata, s64 cycles_late) {
                UpdateSharedMemBuffer(userdata, cycles_late);
            });
    }

    void MapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x01, 1, 2};
        const u32 size = rp.Pop<u32>();
        shared_memory = rp.PopObject<Kernel::SharedMemory>();

        if (shared_memory) {
            shared_memory->SetName("MIC_U:shared_memory");
            state.memory_ref = shared_memory;
            state.sharedmem_buffer = shared_memory->GetPointer();
            state.sharedmem_size = size;
        }

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);

        LOG_TRACE(Service_MIC, "called, size=0x{:X}", size);
    }

    void UnmapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x02, 0, 0};
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        shared_memory = nullptr;
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "called");
    }

    void UpdateSharedMemBuffer(u64 userdata, s64 cycles_late) {
        if (change_mic_impl_requested.exchange(false)) {
            CreateMic();
        }
        // If the event was scheduled before the application requested the mic to stop sampling
        if (!mic->IsSampling()) {
            return;
        }

        Frontend::Mic::Samples samples = mic->Read();
        if (!samples.empty()) {
            // write the samples to sharedmem page
            state.WriteSamples(samples);
        }

        // schedule next run
        timing.ScheduleEvent(GetBufferUpdateRate(state.sample_rate) - cycles_late,
                             buffer_write_event);
    }

    void StartSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x03, 5, 0};

        Encoding encoding = rp.PopEnum<Encoding>();
        SampleRate sample_rate = rp.PopEnum<SampleRate>();
        u32 audio_buffer_offset = rp.PopRaw<u32>();
        u32 audio_buffer_size = rp.Pop<u32>();
        bool audio_buffer_loop = rp.Pop<bool>();

        if (mic->IsSampling()) {
            LOG_CRITICAL(Service_MIC,
                         "Application started sampling again before stopping sampling");
            mic->StopSampling();
        }

        auto sign = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM16Signed
                        ? Frontend::Mic::Signedness::Signed
                        : Frontend::Mic::Signedness::Unsigned;
        u8 sample_size = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM8 ? 8 : 16;
        state.offset = state.initial_offset = audio_buffer_offset;
        state.sample_rate = sample_rate;
        state.sample_size = sample_size;
        state.looped_buffer = audio_buffer_loop;
        state.size = audio_buffer_size;

        mic->StartSampling({sign, sample_size, audio_buffer_loop, GetSampleRateInHz(sample_rate),
                            audio_buffer_offset, audio_buffer_size});

        timing.ScheduleEvent(GetBufferUpdateRate(state.sample_rate), buffer_write_event);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC,
                  "called, encoding={}, sample_rate={}, "
                  "audio_buffer_offset={}, audio_buffer_size={}, audio_buffer_loop={}",
                  static_cast<u32>(encoding), static_cast<u32>(sample_rate), audio_buffer_offset,
                  audio_buffer_size, audio_buffer_loop);
    }

    void AdjustSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x04, 1, 0};
        SampleRate sample_rate = rp.PopEnum<SampleRate>();
        mic->AdjustSampleRate(GetSampleRateInHz(sample_rate));

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "sample_rate={}", static_cast<u32>(sample_rate));
    }

    void StopSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x05, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        mic->StopSampling();
        timing.RemoveEvent(buffer_write_event);
        LOG_TRACE(Service_MIC, "called");
    }

    void IsSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x06, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        bool is_sampling = mic->IsSampling();
        rb.Push<bool>(is_sampling);
        LOG_TRACE(Service_MIC, "IsSampling: {}", is_sampling);
    }

    void GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x07, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(buffer_full_event);
        LOG_WARNING(Service_MIC, "(STUBBED) called");
    }

    void SetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x08, 1, 0};
        u8 gain = rp.Pop<u8>();
        mic->SetGain(gain);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "gain={}", gain);
    }

    void GetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x09, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        u8 gain = mic->GetGain();
        rb.Push<u8>(gain);
        LOG_TRACE(Service_MIC, "gain={}", gain);
    }

    void SetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0A, 1, 0};
        bool power = rp.Pop<bool>();
        mic->SetPower(power);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "mic_power={}", power);
    }

    void GetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0B, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        bool mic_power = mic->GetPower();
        rb.Push<u8>(mic_power);
        LOG_TRACE(Service_MIC, "called");
    }

    void SetIirFilterMic(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0C, 1, 2};
        const u32 size = rp.Pop<u32>();
        const Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(RESULT_SUCCESS);
        rb.PushMappedBuffer(buffer);
        LOG_WARNING(Service_MIC, "(STUBBED) called, size=0x{:X}, buffer=0x{:08X}", size,
                    buffer.GetId());
    }

    void SetClamp(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0D, 1, 0};
        clamp = rp.Pop<bool>();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_MIC, "(STUBBED) called, clamp={}", clamp);
    }

    void GetClamp(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0E, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        rb.Push<bool>(clamp);
        LOG_WARNING(Service_MIC, "(STUBBED) called");
    }

    void SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0F, 1, 0};
        allow_shell_closed = rp.Pop<bool>();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_MIC, "(STUBBED) called, allow_shell_closed={}", allow_shell_closed);
    }

    void SetClientVersion(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x10, 1, 0};
        const u32 version = rp.Pop<u32>();
        LOG_WARNING(Service_MIC, "(STUBBED) called, version: 0x{:08X}", version);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
    }

    void CreateMic() {
        std::unique_ptr<Frontend::Mic::Interface> new_mic;
        switch (Settings::values.mic_input_type) {
        case Settings::MicInputType::None:
            new_mic = std::make_unique<Frontend::Mic::NullMic>();
            break;
        case Settings::MicInputType::Real:
#if HAVE_CUBEB
            new_mic = std::make_unique<AudioCore::CubebInput>(Settings::values.mic_input_device);
#else
            new_mic = std::make_unique<Frontend::Mic::NullMic>();
#endif
            break;
        case Settings::MicInputType::Static:
            new_mic = std::make_unique<Frontend::Mic::StaticMic>();
            break;
        default:
            LOG_CRITICAL(Audio, "Mic type not found. Defaulting to null mic");
            new_mic = std::make_unique<Frontend::Mic::NullMic>();
        }
        // If theres already a mic, copy over any data to the new mic impl
        if (mic) {
            new_mic->SetGain(mic->GetGain());
            new_mic->SetPower(mic->GetPower());
            auto params = mic->GetParameters();
            if (mic->IsSampling()) {
                mic->StopSampling();
                new_mic->StartSampling(params);
            }
        }

        mic = std::move(new_mic);
        change_mic_impl_requested.store(false);
    }

    std::atomic<bool> change_mic_impl_requested = false;
    std::shared_ptr<Kernel::Event> buffer_full_event;
    Core::TimingEventType* buffer_write_event = nullptr;
    std::shared_ptr<Kernel::SharedMemory> shared_memory;
    u32 client_version = 0;
    bool allow_shell_closed = false;
    bool clamp = false;
    std::unique_ptr<Frontend::Mic::Interface> mic;
    Core::Timing& timing;
    State state{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& change_mic_impl_requested;
        ar& buffer_full_event;
        // buffer_write_event set in constructor
        ar& shared_memory;
        ar& client_version;
        ar& allow_shell_closed;
        ar& clamp;
        // mic interface set in constructor
        ar& state;
    }
    friend class boost::serialization::access;
};

void MIC_U::MapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->MapSharedMem(ctx);
}

void MIC_U::UnmapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->UnmapSharedMem(ctx);
}

void MIC_U::StartSampling(Kernel::HLERequestContext& ctx) {
    impl->StartSampling(ctx);
}

void MIC_U::AdjustSampling(Kernel::HLERequestContext& ctx) {
    impl->AdjustSampling(ctx);
}

void MIC_U::StopSampling(Kernel::HLERequestContext& ctx) {
    impl->StopSampling(ctx);
}

void MIC_U::IsSampling(Kernel::HLERequestContext& ctx) {
    impl->IsSampling(ctx);
}

void MIC_U::GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
    impl->GetBufferFullEvent(ctx);
}

void MIC_U::SetGain(Kernel::HLERequestContext& ctx) {
    impl->SetGain(ctx);
}

void MIC_U::GetGain(Kernel::HLERequestContext& ctx) {
    impl->GetGain(ctx);
}

void MIC_U::SetPower(Kernel::HLERequestContext& ctx) {
    impl->SetPower(ctx);
}

void MIC_U::GetPower(Kernel::HLERequestContext& ctx) {
    impl->GetPower(ctx);
}

void MIC_U::SetIirFilterMic(Kernel::HLERequestContext& ctx) {
    impl->SetIirFilterMic(ctx);
}

void MIC_U::SetClamp(Kernel::HLERequestContext& ctx) {
    impl->SetClamp(ctx);
}

void MIC_U::GetClamp(Kernel::HLERequestContext& ctx) {
    impl->GetClamp(ctx);
}

void MIC_U::SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
    impl->SetAllowShellClosed(ctx);
}

void MIC_U::SetClientVersion(Kernel::HLERequestContext& ctx) {
    impl->SetClientVersion(ctx);
}

MIC_U::MIC_U(Core::System& system)
    : ServiceFramework{"mic:u", 1}, impl{std::make_unique<Impl>(system)} {
    static const FunctionInfo functions[] = {
        {0x00010042, &MIC_U::MapSharedMem, "MapSharedMem"},
        {0x00020000, &MIC_U::UnmapSharedMem, "UnmapSharedMem"},
        {0x00030140, &MIC_U::StartSampling, "StartSampling"},
        {0x00040040, &MIC_U::AdjustSampling, "AdjustSampling"},
        {0x00050000, &MIC_U::StopSampling, "StopSampling"},
        {0x00060000, &MIC_U::IsSampling, "IsSampling"},
        {0x00070000, &MIC_U::GetBufferFullEvent, "GetBufferFullEvent"},
        {0x00080040, &MIC_U::SetGain, "SetGain"},
        {0x00090000, &MIC_U::GetGain, "GetGain"},
        {0x000A0040, &MIC_U::SetPower, "SetPower"},
        {0x000B0000, &MIC_U::GetPower, "GetPower"},
        {0x000C0042, &MIC_U::SetIirFilterMic, "SetIirFilterMic"},
        {0x000D0040, &MIC_U::SetClamp, "SetClamp"},
        {0x000E0000, &MIC_U::GetClamp, "GetClamp"},
        {0x000F0040, &MIC_U::SetAllowShellClosed, "SetAllowShellClosed"},
        {0x00100040, &MIC_U::SetClientVersion, "SetClientVersion"},
    };

    impl->CreateMic();
    RegisterHandlers(functions);
}

MIC_U::~MIC_U() {
    impl->mic->StopSampling();
}

void MIC_U::ReloadMic() {
    impl->change_mic_impl_requested.store(true);
}

void ReloadMic(Core::System& system) {
    auto micu = system.ServiceManager().GetService<Service::MIC::MIC_U>("mic:u");
    if (!micu)
        return;
    micu->ReloadMic();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<MIC_U>(system)->InstallAsService(service_manager);
}

} // namespace Service::MIC
