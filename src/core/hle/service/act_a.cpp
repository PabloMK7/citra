// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/act_a.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace ACT_A

namespace ACT_A {

const Interface::FunctionInfo FunctionTable[] = {
    {0x041300C2, nullptr, "UpdateMiiImage"},
    {0x041B0142, nullptr, "AgreeEula"},
    {0x04210042, nullptr, "UploadMii"},
    {0x04230082, nullptr, "ValidateMailAddress"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
