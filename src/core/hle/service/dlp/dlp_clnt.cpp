// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp_clnt.h"

namespace Service {
namespace DLP {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C3, nullptr, "Initialize"}, {0x00110000, nullptr, "GetWirelessRebootPassphrase"},
};

DLP_CLNT_Interface::DLP_CLNT_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
