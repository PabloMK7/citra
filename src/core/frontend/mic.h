// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/swap.h"
#include "common/threadsafe_queue.h"

namespace Frontend::Mic {

constexpr char default_device_name[] = "Default";

enum class Signedness : u8 {
    Signed,
    Unsigned,
};

using Samples = std::vector<u8>;

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

    virtual ~Interface();

    /// Starts the microphone. Called by Core
    virtual void StartSampling(const Parameters& params) = 0;

    /// Stops the microphone. Called by Core
    virtual void StopSampling() = 0;

    /**
     * Called from the actual event timing at a constant period under a given sample rate.
     * When sampling is enabled this function is expected to return a buffer of 16 samples in ideal
     * conditions, but can be lax if the data is coming in from another source like a real mic.
     */
    virtual Samples Read() = 0;

    /**
     * Adjusts the Parameters. Implementations should update the parameters field in addition to
     * changing the mic to sample according to the new parameters. Called by Core
     */
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

    const Parameters& GetParameters() const {
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
    void StartSampling(const Parameters& params) override;

    void StopSampling() override;

    void AdjustSampleRate(u32 sample_rate) override;

    Samples Read() override;
};

class StaticMic final : public Interface {
public:
    StaticMic();
    ~StaticMic() override;

    void StartSampling(const Parameters& params) override;
    void StopSampling() override;
    void AdjustSampleRate(u32 sample_rate) override;

    Samples Read() override;

private:
    u16 sample_rate = 0;
    u8 sample_size = 0;
    std::vector<u8> CACHE_8_BIT;
    std::vector<u8> CACHE_16_BIT;
};

/// Factory for creating a real Mic input device;
class RealMicFactory {
public:
    virtual ~RealMicFactory();

    virtual std::unique_ptr<Interface> Create(std::string mic_device_name) = 0;
};

class NullFactory final : public RealMicFactory {
public:
    ~NullFactory() override;

    std::unique_ptr<Interface> Create(std::string mic_device_name) override;
};

void RegisterRealMicFactory(std::unique_ptr<RealMicFactory> factory);

std::unique_ptr<Interface> CreateRealMic(std::string mic_device_name);

} // namespace Frontend::Mic
