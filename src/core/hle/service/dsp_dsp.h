// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "core/hle/service/service.h"

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

/// Signal all audio related interrupts.
void SignalAllInterrupts();

/**
 * Signal a specific audio related interrupt based on interrupt id and channel id.
 * @param interrupt_id The interrupt id
 * @param channel_id The channel id
 * The significance of various values of interrupt_id and channel_id is not yet known.
 */
void SignalInterrupt(u32 interrupt_id, u32 channel_id);

} // namespace
