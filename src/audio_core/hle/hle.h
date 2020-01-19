// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include <boost/serialization/export.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/dsp_interface.h"
#include "common/common_types.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "core/memory.h"

namespace Memory {
class MemorySystem;
}

namespace AudioCore {

class DspHle final : public DspInterface {
public:
    explicit DspHle(Memory::MemorySystem& memory);
    ~DspHle();

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
    friend struct Impl;
    std::unique_ptr<Impl> impl;

    DspHle();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace AudioCore

BOOST_CLASS_EXPORT_KEY(AudioCore::DspHle)
