// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/function_wrappers.h"
#include "core/hle/hle_syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////



Result SVC_ConnectToPort(void* out, const char* port_name) {
    NOTICE_LOG(OSHLE, "SVC_ConnectToPort called, port_name: %s", port_name);
    return 0;
}

const SysCall SysCallTable[] = {
    {0x2D, WrapI_VC<SVC_ConnectToPort>, "svcConnectToPort"},
};

void Register_SysCalls() {
}
