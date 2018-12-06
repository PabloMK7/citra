// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/lle/lle.h"
#include "teakra/teakra.h"

namespace AudioCore {

struct DspLle::Impl final {
    Teakra::Teakra teakra;
};

DspLle::DspLle() : impl(std::make_unique<Impl>()) {}
DspLle::~DspLle() = default;

} // namespace AudioCore
