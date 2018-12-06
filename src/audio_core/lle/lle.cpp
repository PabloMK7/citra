// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/lle/lle.h"
#include "teakra/teakra.h"

namespace AudioCore {

struct DspLle::Impl final {
    Teakra::Teakra teakra;

    static constexpr unsigned TeakraSlice = 20000;
    void RunTeakraSlice() {
        teakra.Run(TeakraSlice);
    }
};

u16 DspLle::RecvData(u32 register_number) {
    while (!impl->teakra.RecvDataIsReady(register_number)) {
        impl->RunTeakraSlice();
    }
    return impl->teakra.RecvData(static_cast<u8>(register_number));
}

bool DspLle::RecvDataIsReady(u32 register_number) const {
    return impl->teakra.RecvDataIsReady(register_number);
}

void DspLle::SetSemaphore(u16 semaphore_value) {
    impl->teakra.SetSemaphore(semaphore_value);
}

DspLle::DspLle() : impl(std::make_unique<Impl>()) {}
DspLle::~DspLle() = default;

} // namespace AudioCore
