// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/function_wrappers.h"
#include "core/hle/hle_syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef u32 Handle;
typedef s32 Result;

Result SVC_ConnectToPort(void* out, const char* port_name) {
    NOTICE_LOG(OSHLE, "svcConnectToPort called, port_name: %s", port_name);
    return 0;
}

const HLEFunction SysCallTable[] = {
    {0x2D, WrapI_VC<SVC_ConnectToPort>, "svcConnectToPort"},
};

void Register_SysCall() {
    HLE::RegisterModule("SysCallTable", ARRAY_SIZE(SysCallTable), SysCallTable);
}
