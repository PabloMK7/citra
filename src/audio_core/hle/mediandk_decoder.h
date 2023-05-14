// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

#include "audio_core/hle/decoder.h"

namespace AudioCore::HLE {

class MediaNDKDecoder final : public DecoderBase {
public:
    explicit MediaNDKDecoder(Memory::MemorySystem& memory);
    ~MediaNDKDecoder() override;
    std::optional<BinaryMessage> ProcessRequest(const BinaryMessage& request) override;
    bool IsValid() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore::HLE
