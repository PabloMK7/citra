// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ptm/ptm_sets.h"

namespace Service {
namespace PTM {

const Interface::FunctionInfo FunctionTable[] = {
    // Note that this service does not have access to ptm:u's common commands
    {0x00010080, nullptr, "SetSystemTime"},
};

PTM_Sets::PTM_Sets() {
    Register(FunctionTable);
}

} // namespace PTM
} // namespace Service
