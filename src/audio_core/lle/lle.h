// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/dsp_interface.h"

namespace AudioCore {

class DspLle final : public DspInterface {
public:
    DspLle();
    ~DspLle();

    u16 RecvData(u32 register_number) override;
    bool RecvDataIsReady(u32 register_number) const override;
    void SetSemaphore(u16 semaphore_value) override;

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore
