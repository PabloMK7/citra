// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ptm/ptm_gets.h"

namespace Service {
namespace PTM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x04010000, nullptr, "GetSystemTime"},
};

PTM_Gets::PTM_Gets() {
    Register(FunctionTable);
}

} // namespace PTM
} // namespace Service
