// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "core/frontend/mic.h"

#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif

namespace Frontend::Mic {

constexpr std::array<u8, 16> NOISE_SAMPLE_8_BIT = {0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                   0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF};

constexpr std::array<u8, 32> NOISE_SAMPLE_16_BIT = {
    0x64, 0x61, 0x74, 0x61, 0x56, 0xD7, 0x00, 0x00, 0x48, 0xF7, 0x86, 0x05, 0x77, 0x1A, 0xF4, 0x1F,
    0x28, 0x0F, 0x6B, 0xEB, 0x1C, 0xC0, 0xCB, 0x9D, 0x46, 0x90, 0xDF, 0x98, 0xEA, 0xAE, 0xB5, 0xC4};

Interface::~Interface() = default;

void NullMic::StartSampling(const Parameters& params) {
    parameters = params;
    is_sampling = true;
}

void NullMic::StopSampling() {
    is_sampling = false;
}

void NullMic::AdjustSampleRate(u32 sample_rate) {
    parameters.sample_rate = sample_rate;
}

Samples NullMic::Read() {
    return {};
}

StaticMic::StaticMic()
    : CACHE_8_BIT{NOISE_SAMPLE_8_BIT.begin(), NOISE_SAMPLE_8_BIT.end()},
      CACHE_16_BIT{NOISE_SAMPLE_16_BIT.begin(), NOISE_SAMPLE_16_BIT.end()} {}

StaticMic::~StaticMic() = default;

void StaticMic::StartSampling(const Parameters& params) {
    sample_rate = params.sample_rate;
    sample_size = params.sample_size;

    parameters = params;
    is_sampling = true;
}

void StaticMic::StopSampling() {
    is_sampling = false;
}

void StaticMic::AdjustSampleRate(u32 sample_rate) {}

Samples StaticMic::Read() {
    return (sample_size == 8) ? CACHE_8_BIT : CACHE_16_BIT;
}

RealMicFactory::~RealMicFactory() = default;

NullFactory::~NullFactory() = default;

std::unique_ptr<Interface> NullFactory::Create([[maybe_unused]] std::string mic_device_name) {
    return std::make_unique<NullMic>();
}

#ifdef HAVE_CUBEB
static std::unique_ptr<RealMicFactory> g_factory = std::make_unique<AudioCore::CubebFactory>();
#else
static std::unique_ptr<RealMicFactory> g_factory = std::make_unique<NullFactory>();
#endif

void RegisterRealMicFactory(std::unique_ptr<RealMicFactory> factory) {
    g_factory = std::move(factory);
}

std::unique_ptr<Interface> CreateRealMic(std::string mic_device_name) {
    return g_factory->Create(std::move(mic_device_name));
}

} // namespace Frontend::Mic
