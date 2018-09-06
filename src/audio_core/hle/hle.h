// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "audio_core/audio_types.h"
#include "audio_core/dsp_interface.h"
#include "common/common_types.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "core/memory.h"

namespace AudioCore {

class DspHle final : public DspInterface {
public:
    DspHle();
    ~DspHle();

    DspState GetDspState() const override;

    std::vector<u8> PipeRead(DspPipe pipe_number, u32 length) override;
    std::size_t GetPipeReadableSize(DspPipe pipe_number) const override;
    void PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) override;

    std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory() override;

    void SetServiceToInterrupt(std::weak_ptr<Service::DSP::DSP_DSP> dsp) override;

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore
