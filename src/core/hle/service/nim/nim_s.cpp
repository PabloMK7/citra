// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_s.h"

namespace Service {
namespace NIM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, nullptr,                      "CheckSysupdateAvailableSOAP"},
};

NIM_S_Interface::NIM_S_Interface() {
    Register(FunctionTable);
}

} // namespace NIM
} // namespace Service

