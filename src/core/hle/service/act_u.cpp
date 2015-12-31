// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/act_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace ACT_U

namespace ACT_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000600C2, nullptr, "GetAccountDataBlock"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
