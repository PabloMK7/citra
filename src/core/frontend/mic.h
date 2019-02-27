// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include "common/swap.h"

namespace Frontend {

namespace Mic {

enum class Signedness : u8 {
    Signed,
    Unsigned,
};

struct Parameters {
    Signedness sign;
    u8 sample_size;
    bool buffer_loop;
    u32 sample_rate;
    u32 buffer_offset;
    u32 buffer_size;
};

class Interface {
public:
    /// Starts the microphone. Called by Core
    virtual void StartSampling(Parameters params) = 0;

    /// Stops the microphone. Called by Core
    virtual void StopSampling() = 0;

    Interface() = default;

    Interface(const Interface& other)
        : gain(other.gain), powered(other.powered), backing_memory(other.backing_memory),
          backing_memory_size(other.backing_memory_size), parameters(other.parameters) {}

    /// Sets the backing memory that the mic should write raw samples into. Called by Core
    void SetBackingMemory(u8* pointer, u32 size) {
        backing_memory = pointer;
        backing_memory_size = size;
    }

    /// Adjusts the Parameters. Implementations should update the parameters field in addition to
    /// changing the mic to sample according to the new parameters. Called by Core
    virtual void AdjustSampleRate(u32 sample_rate) = 0;

    /// Value from 0 - 100 to adjust the mic gain setting. Called by Core
    virtual void SetGain(u8 mic_gain) {
        gain = mic_gain;
    }

    u8 GetGain() const {
        return gain;
    }

    void SetPower(bool power) {
        powered = power;
    }

    bool GetPower() const {
        return powered;
    }

    bool IsSampling() const {
        return is_sampling;
    }

    Parameters GetParameters() const {
        return parameters;
    }

protected:
    u8* backing_memory;
    u32 backing_memory_size;

    Parameters parameters;
    u8 gain = 0;
    bool is_sampling = false;
    bool powered = false;
};

class NullMic final : public Interface {
public:
    void StartSampling(Parameters params) override {
        parameters = params;
        is_sampling = true;
    }

    void StopSampling() override {
        is_sampling = false;
    }

    void AdjustSampleRate(u32 sample_rate) override {
        parameters.sample_rate = sample_rate;
    }
};

} // namespace Mic

void RegisterMic(std::shared_ptr<Mic::Interface> mic);

std::shared_ptr<Mic::Interface> GetCurrentMic();

} // namespace Frontend
