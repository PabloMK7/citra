// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "ndm_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NDM_U

namespace NDM_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, nullptr,                 "EnterExclusiveState"},
    {0x00020002, nullptr,                 "LeaveExclusiveState"},
    {0x00030000, nullptr,                 "QueryExclusiveMode"},
    {0x00060040, nullptr,                 "SuspendDaemons"},
    {0x00080040, nullptr,                 "DisableWifiUsage"},
    {0x00090000, nullptr,                 "EnableWifiUsage"},
    {0x00140040, nullptr,                 "OverrideDefaultDaemons"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
