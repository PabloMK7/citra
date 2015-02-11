// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/ssl_c.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SSL_C

namespace SSL_C {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000200C2, nullptr,               "CreateContext"},
    {0x00050082, nullptr,               "AddTrustedRootCA"},
    {0x00150082, nullptr,               "Read"},
    {0x00170082, nullptr,               "Write"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
