// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/pm_app.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace PM_APP

namespace PM_APP {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010140, nullptr,                 "LaunchTitle"},
    {0x00020082, nullptr,                 "LaunchFIRMSetParams"},
    {0x00030080, nullptr,                 "TerminateProcesse"},
    {0x00040100, nullptr,                 "TerminateProcessTID"},
    {0x000500C0, nullptr,                 "TerminateProcessTID_unknown"},
    {0x00070042, nullptr,                 "GetFIRMLaunchParams"},
    {0x00080100, nullptr,                 "GetTitleExheaderFlags"},
    {0x00090042, nullptr,                 "SetFIRMLaunchParams"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
