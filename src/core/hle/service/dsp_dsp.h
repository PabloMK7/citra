// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/hle/service/service.h"

namespace AudioCore {
enum class DspPipe;
}

namespace Service {
namespace DSP_DSP {

class Interface final : public Service::Interface {
public:
    Interface();
    ~Interface() override;

    std::string GetPortName() const override {
        return "dsp::DSP";
    }
};

/**
 * Signal a specific DSP related interrupt of type == InterruptType::Pipe, pipe == pipe.
 * @param pipe The DSP pipe for which to signal an interrupt for.
 */
void SignalPipeInterrupt(AudioCore::DspPipe pipe);

} // namespace DSP_DSP
} // namespace Service
