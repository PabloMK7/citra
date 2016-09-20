// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/hle/service/service.h"

namespace DSP {
namespace HLE {
enum class DspPipe;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace DSP_DSP

namespace DSP_DSP {

class Interface : public Service::Interface {
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
void SignalPipeInterrupt(DSP::HLE::DspPipe pipe);

} // namespace DSP_DSP
