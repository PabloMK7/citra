// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/hle/function_wrappers.h"
#include "core/hle/hle_syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef u32 Handle;
typedef s32 Result;

////////////////////////////////////////////////////////////////////////////////////////////////////

Result SVC_ConnectToPort(void* out, const char* port_name) {
    NOTICE_LOG(OSHLE, "svcConnectToPort called, port_name: %s", port_name);
    return 0;
}

const HLE::FunctionDef Syscall_Table[] = {
    {0x00,  NULL,                           "Unknown"},
    {0x01,  NULL,                           "svcControlMemory"},
    {0x02,  NULL,                           "svcQueryMemory"},
    {0x03,  NULL,                           "svcExitProcess"},
    {0x04,  NULL,                           "svcGetProcessAffinityMask"},
    {0x05,  NULL,                           "svcSetProcessAffinityMask"},
    {0x06,  NULL,                           "svcGetProcessIdealProcessor"},
    {0x07,  NULL,                           "svcSetProcessIdealProcessor"},
    {0x08,  NULL,                           "svcCreateThread"},
    {0x09,  NULL,                           "svcExitThread"},
    {0x0A,  NULL,                           "svcSleepThread"},
    {0x0B,  NULL,                           "svcGetThreadPriority"},
    {0x0C,  NULL,                           "svcSetThreadPriority"},
    {0x0D,  NULL,                           "svcGetThreadAffinityMask"},
    {0x0E,  NULL,                           "svcSetThreadAffinityMask"},
    {0x0F,  NULL,                           "svcGetThreadIdealProcessor"},
    {0x10,  NULL,                           "svcSetThreadIdealProcessor"},
    {0x11,  NULL,                           "svcGetCurrentProcessorNumber"},
    {0x12,  NULL,                           "svcRun"},
    {0x13,  NULL,                           "svcCreateMutex"},
    {0x14,  NULL,                           "svcReleaseMutex"},
    {0x15,  NULL,                           "svcCreateSemaphore"},
    {0x16,  NULL,                           "svcReleaseSemaphore"},
    {0x17,  NULL,                           "svcCreateEvent"},
    {0x18,  NULL,                           "svcSignalEvent"},
    {0x19,  NULL,                           "svcClearEvent"},
    {0x1A,  NULL,                           "svcCreateTimer"},
    {0x1B,  NULL,                           "svcSetTimer"},
    {0x1C,  NULL,                           "svcCancelTimer"},
    {0x1D,  NULL,                           "svcClearTimer"},
    {0x1E,  NULL,                           "svcCreateMemoryBlock"},
    {0x1F,  NULL,                           "svcMapMemoryBlock"},
    {0x20,  NULL,                           "svcUnmapMemoryBlock"},
    {0x21,  NULL,                           "svcCreateAddressArbiter"},
    {0x22,  NULL,                           "svcArbitrateAddress"},
    {0x23,  NULL,                           "svcCloseHandle"},
    {0x24,  NULL,                           "svcWaitSynchronization1"},
    {0x25,  NULL,                           "svcWaitSynchronizationN"},
    {0x26,  NULL,                           "svcSignalAndWait"},
    {0x27,  NULL,                           "svcDuplicateHandle"},
    {0x28,  NULL,                           "svcGetSystemTick"},
    {0x29,  NULL,                           "svcGetHandleInfo"},
    {0x2A,  NULL,                           "svcGetSystemInfo"},
    {0x2B,  NULL,                           "svcGetProcessInfo"},
    {0x2C,  NULL,                           "svcGetThreadInfo"},
    {0x2D,  WrapI_VC<SVC_ConnectToPort>,    "svcConnectToPort"},
};

void Register_Syscall() {
    HLE::RegisterModule("SyscallTable", ARRAY_SIZE(Syscall_Table), Syscall_Table);
}
