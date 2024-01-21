#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Settings {
enum class AudioEmulation : u32;
}

class ServiceFixture {
public:
    typedef int Handle;

    static constexpr int PIPE2_IRQ_HANDLE = 0x919E;
    static constexpr int DSP_SEMAPHORE_HANDLE = 0xD59;

    // SVC
    Result svcCreateEvent(Handle* event, u32 reset_type) {
        return ResultSuccess;
    }

    Result svcWaitSynchronization(Handle handle, s64 nanoseconds);

    Result svcClearEvent(Handle handle);

    Result svcSignalEvent(Handle handle);

    Result svcCloseHandle(Handle handle) {
        return ResultSuccess;
    }

    // dsp::DSP
    Result DSP_LoadComponent(const u8* dspfirm_data, size_t size, u8 progmask, u8 datamask,
                             bool* dspfirm_loaded);

    Result DSP_UnloadComponent();

    Result DSP_RegisterInterruptEvents(Handle /*handle*/, u32 /*interrupt*/, u32 /*channel*/) {
        return ResultSuccess;
    };

    Result DSP_GetSemaphoreHandle(Handle* handle);

    Result DSP_SetSemaphoreMask(u32 /*mask*/) {
        return ResultSuccess;
    }

    Result DSP_SetSemaphore(u32 semaphore);

    Result DSP_ReadPipeIfPossible(u32 channel, u32 peer, void* out_buffer, u32 size, u16* out_size);

    Result DSP_ConvertProcessAddressFromDspDram(u32 dsp_address, u16** host_address);

    Result DSP_WriteProcessPipe(u32 channel, const void* buffer, u32 length);

    Result DSP_FlushDataCache(void*, size_t) {
        return ResultSuccess;
    }

    // libctru
    u32 osConvertVirtToPhys(void* vaddr);

    void* linearAlloc(size_t size);

    Result dspInit();

    Result dspExit() {
        return ResultSuccess;
    }

    // Selects the DPS Emulation to use with the fixture
    void InitDspCore(Settings::AudioEmulation dsp_core);

private:
    Core::System system{};
    Memory::MemorySystem memory{system};
    Core::Timing core_timing{1, 100};
    std::unique_ptr<AudioCore::DspInterface> dsp{};

    std::array<std::array<bool, 3>, 4> interrupts_fired = {};

    std::size_t linear_alloc_offset = 0;
};
