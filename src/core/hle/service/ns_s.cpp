// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.


#include "core/hle/hle.h"
#include "core/hle/service/ns_s.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NS_S

namespace NS_S {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000200C0, nullptr,                      "LaunchTitle"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
