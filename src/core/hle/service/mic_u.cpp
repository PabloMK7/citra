// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

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

namespace Service::MIC {

/// Microphone audio encodings.
enum class Encoding : u8 {
    PCM8 = 0,        ///< Unsigned 8-bit PCM.
    PCM16 = 1,       ///< Unsigned 16-bit PCM.
    PCM8Signed = 2,  ///< Signed 8-bit PCM.
    PCM16Signed = 3, ///< Signed 16-bit PCM.
};

/// Microphone audio sampling rates.
enum class SampleRate : u8 {
    Rate32730 = 0, ///< 32728.498 Hz
    Rate16360 = 1, ///< 16364.479 Hz
    Rate10910 = 2, ///< 10909.499 Hz
    Rate8180 = 3   ///< 8182.1245 Hz
};

constexpr u32 GetSampleRateInHz(SampleRate sample_rate) {
    switch (sample_rate) {
    case SampleRate::Rate8180:
        return 8180;
    case SampleRate::Rate10910:
        return 10910;
    case SampleRate::Rate16360:
        return 16360;
    case SampleRate::Rate32730:
        return 32730;
    default:
        UNREACHABLE();
    }
}

struct MIC_U::Impl {
    explicit Impl(Core::System& system) {
        buffer_full_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "MIC_U::buffer_full_event");
    }

    void MapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x01, 1, 2};
        const u32 size = rp.Pop<u32>();
        shared_memory = rp.PopObject<Kernel::SharedMemory>();

        if (shared_memory) {
            shared_memory->SetName("MIC_U:shared_memory");
        }

        mic->SetBackingMemory(shared_memory->GetPointer(), size);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);

        LOG_TRACE(Service_MIC, "MIC:U MapSharedMem called, size=0x{:X}", size);
    }

    void UnmapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x02, 0, 0};
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        shared_memory = nullptr;
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "MIC:U UnmapSharedMem called");
    }

    void StartSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x03, 5, 0};

        Encoding encoding = rp.PopEnum<Encoding>();
        SampleRate sample_rate = rp.PopEnum<SampleRate>();
        u32 audio_buffer_offset = rp.PopRaw<u32>();
        u32 audio_buffer_size = rp.Pop<u32>();
        bool audio_buffer_loop = rp.Pop<bool>();

        auto sign = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM16Signed
                        ? Frontend::Mic::Signedness::Signed
                        : Frontend::Mic::Signedness::Unsigned;
        u8 sample_size = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM8 ? 8 : 16;

        mic->StartSampling({sign, sample_size, audio_buffer_loop, GetSampleRateInHz(sample_rate),
                            audio_buffer_offset, audio_buffer_size});

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC,
                  "MIC:U StartSampling called, encoding={}, sample_rate={}, "
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
        LOG_TRACE(Service_MIC, "MIC:U AdjustSampling sample_rate={}",
                  static_cast<u32>(sample_rate));
    }

    void StopSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x05, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        mic->StopSampling();
        LOG_TRACE(Service_MIC, "MIC:U StopSampling called");
    }

    void IsSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x06, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        bool is_sampling = mic->IsSampling();
        rb.Push<bool>(is_sampling);
        LOG_TRACE(Service_MIC, "MIC:U IsSampling: {}", is_sampling);
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
        LOG_TRACE(Service_MIC, "MIC:U SetGain gain={}", gain);
    }

    void GetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x09, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        u8 gain = mic->GetGain();
        rb.Push<u8>(gain);
        LOG_TRACE(Service_MIC, "MIC:U GetGain gain={}", gain);
    }

    void SetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0A, 1, 0};
        bool power = rp.Pop<bool>();
        mic->SetPower(power);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_TRACE(Service_MIC, "MIC:U SetPower mic_power={}", power);
    }

    void GetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0B, 0, 0};

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        bool mic_power = mic->GetPower();
        rb.Push<u8>(mic_power);
        LOG_TRACE(Service_MIC, "MIC:U GetPower called");
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
    Kernel::SharedPtr<Kernel::Event> buffer_full_event =
        Core::System::GetInstance().Kernel().CreateEvent(Kernel::ResetType::OneShot,
                                                         "MIC_U::buffer_full_event");
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
    u32 client_version = 0;
    bool allow_shell_closed = false;
    bool clamp = false;
    std::shared_ptr<Frontend::Mic::Interface> mic;
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

    impl->mic = Frontend::GetCurrentMic();
    RegisterHandlers(functions);
}

MIC_U::~MIC_U() {
    impl->mic->StopSampling();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<MIC_U>(system)->InstallAsService(service_manager);
}

} // namespace Service::MIC
