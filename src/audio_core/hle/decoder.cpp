// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/decoder.h"

namespace AudioCore::HLE {

DecoderBase::~DecoderBase(){};

NullDecoder::NullDecoder() = default;

NullDecoder::~NullDecoder() = default;

std::optional<BinaryResponse> NullDecoder::ProcessRequest(const BinaryRequest& request) {
    BinaryResponse response;
    switch (request.cmd) {
    case DecoderCommand::Init:
    case DecoderCommand::Unknown:
        std::memcpy(&response, &request, sizeof(response));
        response.unknown1 = 0x0;
        return response;
    case DecoderCommand::Decode:
        response.codec = request.codec;
        response.cmd = DecoderCommand::Decode;
        response.num_channels = 2; // Just assume stereo here
        response.size = request.size;
        response.num_samples = 1024; // Just assume 1024 here
        return response;
    default:
        LOG_ERROR(Audio_DSP, "Got unknown binary request: {}", static_cast<u16>(request.cmd));
        return {};
    }
};
} // namespace AudioCore::HLE
