// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_ndm.h"

namespace Service {
namespace CECD {

static const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr, "Initialize"},
    {0x00020000, nullptr, "Deinitialize"},
    {0x00030000, nullptr, "ResumeDaemon"},
    {0x00040040, nullptr, "SuspendDaemon"},
};

CECD_NDM::CECD_NDM() {
    Register(FunctionTable);
}

} // namespace CECD
} // namespace Service
