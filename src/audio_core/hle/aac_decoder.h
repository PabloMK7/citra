// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/hle/decoder.h"

namespace AudioCore::HLE {

class AACDecoder final : public DecoderBase {
public:
    explicit AACDecoder(Memory::MemorySystem& memory);
    ~AACDecoder() override;
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore::HLE
