// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nwm/nwm_ext.h"

namespace Service {
namespace NWM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00080040, nullptr, "ControlWirelessEnabled"},
};

NWM_EXT::NWM_EXT() {
    Register(FunctionTable);
}

} // namespace NWM
} // namespace Service
