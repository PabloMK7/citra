// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/ndm.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NDM_U

namespace NDM_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00060040, NULL, "SuspendDaemons"},
    {0x00080040, NULL, "DisableWifiUsage"},
    {0x00090000, NULL, "EnableWifiUsage"},
    {0x00140040, NULL, "OverrideDefaultDaemons"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
