// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/swap.h"
#include "common/threadsafe_queue.h"

namespace Frontend::Mic {

enum class Signedness : u8 {
    Signed,
    Unsigned,
};

using Samples = std::vector<u8>;
using SampleQueue = Common::SPSCQueue<Samples>;

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
    Interface() = default;

    virtual ~Interface() = default;

    /// Starts the microphone. Called by Core
    virtual void StartSampling(Parameters params) = 0;

    /// Stops the microphone. Called by Core
    virtual void StopSampling() = 0;

    /// Called from the actual event timing read back. The frontend impl is responsible for wrapping
    /// up any data and returning them to the core so the core can write them to the sharedmem. If
    /// theres nothing to return just return an empty vector
    virtual Samples Read() = 0;

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

    Samples Read() override {
        return {};
    }
};

class StaticMic final : public Interface {
public:
    StaticMic();
    ~StaticMic() override;

    void StartSampling(Parameters params) override;
    void StopSampling() override;
    void AdjustSampleRate(u32 sample_rate) override;

    Samples Read() override;

private:
    u16 sample_rate = 0;
    u8 sample_size = 0;
    std::vector<u8> CACHE_8_BIT;
    std::vector<u8> CACHE_16_BIT;
};

void RegisterMic(std::shared_ptr<Mic::Interface> mic);

std::shared_ptr<Mic::Interface> GetCurrentMic();

} // namespace Frontend::Mic
