// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp_fkcl.h"

namespace Service {
namespace DLP {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010083, nullptr, "Initialize"},
    {0x000F0000, nullptr, "GetWirelessRebootPassphrase"},
};

DLP_FKCL_Interface::DLP_FKCL_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
