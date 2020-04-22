// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/core.h"

namespace AudioCore::HLE {

enum class DecoderCommand : u16 {
    Init,
    Decode,
    Unknown,
};

enum class DecoderCodec : u16 {
    None,
    AAC,
};

// TODO(xperia64): I'm guessing that this is a u32 (from when it was an unknown)
// but it could be a u16 or u8 I suppose
enum class DecoderSampleRate : u32 {
    Rate48000 = 0,
    Rate44100 = 1,
    Rate32000 = 2,
    Rate24000 = 3,
    Rate22050 = 4,
    Rate16000 = 5,
    Rate12000 = 6,
    Rate11025 = 7,
    Rate8000 = 8
};

struct BinaryRequest {
    enum_le<DecoderCodec> codec =
        DecoderCodec::None; // this is a guess. until now only 0x1 was observed here
    enum_le<DecoderCommand> cmd = DecoderCommand::Init;
    u32_le fixed = 0;
    u32_le src_addr = 0;
    u32_le size = 0;
    u32_le dst_addr_ch0 = 0;
    u32_le dst_addr_ch1 = 0;
    u32_le unknown1 = 0;
    u32_le unknown2 = 0;
};
static_assert(sizeof(BinaryRequest) == 32, "Unexpected struct size for BinaryRequest");

struct BinaryResponse {
    enum_le<DecoderCodec> codec =
        DecoderCodec::None; // this could be something else. until now only 0x1 was observed here
    enum_le<DecoderCommand> cmd = DecoderCommand::Init;
    u32_le unknown1 = 0;
    enum_le<DecoderSampleRate> sample_rate;
    u32_le num_channels = 0; // this is a guess, so far I only observed 2 here
    u32_le size = 0;
    u32_le unknown3 = 0;
    u32_le unknown4 = 0;
    u32_le num_samples = 0; // this is a guess, so far I only observed 1024 here
};
static_assert(sizeof(BinaryResponse) == 32, "Unexpected struct size for BinaryResponse");

enum_le<DecoderSampleRate> GetSampleRateEnum(u32 sample_rate);

class DecoderBase {
public:
    virtual ~DecoderBase();
    virtual std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request) = 0;
    /// Return true if this Decoder can be loaded. Return false if the system cannot create the
    /// decoder
    virtual bool IsValid() const = 0;
};

class NullDecoder final : public DecoderBase {
public:
    NullDecoder();
    ~NullDecoder() override;
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request) override;
    bool IsValid() const override {
        return true;
    }
};

} // namespace AudioCore::HLE
