// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/mic_u.h"

namespace Service {
namespace MIC {

enum class Encoding : u8 {
    PCM8 = 0,
    PCM16 = 1,
    PCM8Signed = 2,
    PCM16Signed = 3,
};

enum class SampleRate : u8 {
    SampleRate32730 = 0,
    SampleRate16360 = 1,
    SampleRate10910 = 2,
    SampleRate8180 = 3
};

static Kernel::SharedPtr<Kernel::Event> buffer_full_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static u8 mic_gain = 0;
static bool mic_power = false;
static bool is_sampling = false;
static bool allow_shell_closed;
static bool clamp = false;
static Encoding encoding;
static SampleRate sample_rate;
static s32 audio_buffer_offset;
static u32 audio_buffer_size;
static bool audio_buffer_loop;

/**
 * MIC::MapSharedMem service function
 *  Inputs:
 *      0 : Header Code[0x00010042]
 *      1 : Shared-mem size
 *      2 : CopyHandleDesc
 *      3 : Shared-mem handle
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void MapSharedMem(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 size = cmd_buff[1];
    Kernel::Handle mem_handle = cmd_buff[3];
    shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(mem_handle);
    if (shared_memory) {
        shared_memory->name = "MIC_U:shared_memory";
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "called, size=0x%X, mem_handle=0x%08X", size, mem_handle);
}

/**
 * MIC::UnmapSharedMem service function
 *  Inputs:
 *      0 : Header Code[0x00020000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UnmapSharedMem(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "called");
}

/**
 * MIC::StartSampling service function
 *  Inputs:
 *      0 : Header Code[0x00030140]
 *      1 : Encoding
 *      2 : SampleRate
 *      3 : Base offset for audio data in sharedmem
 *      4 : Size of the audio data in sharedmem
 *      5 : Loop at end of buffer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void StartSampling(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    encoding = static_cast<Encoding>(cmd_buff[1] & 0xFF);
    sample_rate = static_cast<SampleRate>(cmd_buff[2] & 0xFF);
    audio_buffer_offset = cmd_buff[3];
    audio_buffer_size = cmd_buff[4];
    audio_buffer_loop = static_cast<bool>(cmd_buff[5] & 0xFF);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    is_sampling = true;
    LOG_WARNING(Service_MIC, "(STUBBED) called, encoding=%u, sample_rate=%u, "
                             "audio_buffer_offset=%d, audio_buffer_size=%u, audio_buffer_loop=%u",
                static_cast<u32>(encoding), static_cast<u32>(sample_rate), audio_buffer_offset,
                audio_buffer_size, audio_buffer_loop);
}

/**
 * MIC::AdjustSampling service function
 *  Inputs:
 *      0 : Header Code[0x00040040]
 *      1 : SampleRate
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void AdjustSampling(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    sample_rate = static_cast<SampleRate>(cmd_buff[1] & 0xFF);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, sample_rate=%u", static_cast<u32>(sample_rate));
}

/**
 * MIC::StopSampling service function
 *  Inputs:
 *      0 : Header Code[0x00050000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void StopSampling(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    is_sampling = false;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::IsSampling service function
 *  Inputs:
 *      0 : Header Code[0x00060000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 0 = sampling, non-zero = sampling
 */
static void IsSampling(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = is_sampling;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::GetBufferFullEvent service function
 *  Inputs:
 *      0 : Header Code[0x00070000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : Event handle
 */
static void GetBufferFullEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(buffer_full_event).MoveFrom();
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::SetGain service function
 *  Inputs:
 *      0 : Header Code[0x00080040]
 *      1 : Gain
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetGain(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    mic_gain = cmd_buff[1] & 0xFF;
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, mic_gain=%u", mic_gain);
}

/**
 * MIC::GetGain service function
 *  Inputs:
 *      0 : Header Code[0x00090000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Gain
 */
static void GetGain(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = mic_gain;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::SetPower service function
 *  Inputs:
 *      0 : Header Code[0x000A0040]
 *      1 : Power (0 = off, 1 = on)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetPower(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    mic_power = static_cast<bool>(cmd_buff[1] & 0xFF);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, mic_power=%u", mic_power);
}

/**
 * MIC::GetPower service function
 *  Inputs:
 *      0 : Header Code[0x000B0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Power
 */
static void GetPower(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = mic_power;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::SetIirFilterMic service function
 *  Inputs:
 *      0 : Header Code[0x000C0042]
 *      1 : Size
 *      2 : (Size << 4) | 0xA
 *      3 : Pointer to IIR Filter Data
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetIirFilterMic(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    VAddr buffer = cmd_buff[3];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, size=0x%X, buffer=0x%08X", size, buffer);
}

/**
 * MIC::SetClamp service function
 *  Inputs:
 *      0 : Header Code[0x000D0040]
 *      1 : Clamp (0 = don't clamp, non-zero = clamp)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetClamp(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    clamp = static_cast<bool>(cmd_buff[1] & 0xFF);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, clamp=%u", clamp);
}

/**
 * MIC::GetClamp service function
 *  Inputs:
 *      0 : Header Code[0x000E0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Clamp (0 = don't clamp, non-zero = clamp)
 */
static void GetClamp(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = clamp;
    LOG_WARNING(Service_MIC, "(STUBBED) called");
}

/**
 * MIC::SetAllowShellClosed service function
 *  Inputs:
 *      0 : Header Code[0x000D0040]
 *      1 : Sampling allowed while shell closed (0 = disallow, non-zero = allow)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetAllowShellClosed(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    allow_shell_closed = static_cast<bool>(cmd_buff[1] & 0xFF);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_MIC, "(STUBBED) called, allow_shell_closed=%u", allow_shell_closed);
}

/**
 * MIC_U::SetClientVersion service function
 *  Inputs:
 *      1 : Used SDK Version
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetClientVersion(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 version = cmd_buff[1];
    self->SetVersion(version);

    LOG_WARNING(Service_MIC, "(STUBBED) called, version: 0x%08X", version);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, MapSharedMem, "MapSharedMem"},
    {0x00020000, UnmapSharedMem, "UnmapSharedMem"},
    {0x00030140, StartSampling, "StartSampling"},
    {0x00040040, AdjustSampling, "AdjustSampling"},
    {0x00050000, StopSampling, "StopSampling"},
    {0x00060000, IsSampling, "IsSampling"},
    {0x00070000, GetBufferFullEvent, "GetBufferFullEvent"},
    {0x00080040, SetGain, "SetGain"},
    {0x00090000, GetGain, "GetGain"},
    {0x000A0040, SetPower, "SetPower"},
    {0x000B0000, GetPower, "GetPower"},
    {0x000C0042, SetIirFilterMic, "SetIirFilterMic"},
    {0x000D0040, SetClamp, "SetClamp"},
    {0x000E0000, GetClamp, "GetClamp"},
    {0x000F0040, SetAllowShellClosed, "SetAllowShellClosed"},
    {0x00100040, SetClientVersion, "SetClientVersion"},
};

MIC_U::MIC_U() {
    Register(FunctionTable);
    shared_memory = nullptr;
    buffer_full_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "MIC_U::buffer_full_event");
    mic_gain = 0;
    mic_power = false;
    is_sampling = false;
    clamp = false;
}

MIC_U::~MIC_U() {
    shared_memory = nullptr;
    buffer_full_event = nullptr;
}

} // namespace MIC
} // namespace Service
