// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/dsp_interface.h"

namespace AudioCore {

class DspLle final : public DspInterface {
public:
    explicit DspLle(Memory::MemorySystem& memory, bool multithread);
    ~DspLle() override;

    u16 RecvData(u32 register_number) override;
    bool RecvDataIsReady(u32 register_number) const override;
    void SetSemaphore(u16 semaphore_value) override;
    std::vector<u8> PipeRead(DspPipe pipe_number, u32 length) override;
    std::size_t GetPipeReadableSize(DspPipe pipe_number) const override;
    void PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) override;

    std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory() override;

    void SetServiceToInterrupt(std::weak_ptr<Service::DSP::DSP_DSP> dsp) override;

    void LoadComponent(const std::vector<u8>& buffer) override;
    void UnloadComponent() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore
