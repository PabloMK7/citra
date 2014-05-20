// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <map>
#include <string>

#include "common/symbols.h"

#include "core/mem_map.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

#include "core/hle/function_wrappers.h"
#include "core/hle/svc.h"
#include "core/hle/service/service.h"
#include "core/hle/kernel/thread.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

namespace SVC {

enum ControlMemoryOperation {
    MEMORY_OPERATION_HEAP       = 0x00000003,
    MEMORY_OPERATION_GSP_HEAP   = 0x00010003,
};

enum MapMemoryPermission {
    MEMORY_PERMISSION_UNMAP     = 0x00000000,
    MEMORY_PERMISSION_NORMAL    = 0x00000001,
};

/// Map application or GSP heap memory
Result ControlMemory(void* _outaddr, u32 operation, u32 addr0, u32 addr1, u32 size, u32 permissions) {
    u32* outaddr = (u32*)_outaddr;
    u32 virtual_address = 0x00000000;

    DEBUG_LOG(SVC, "ControlMemory called operation=0x%08X, addr0=0x%08X, addr1=0x%08X, size=%08X, permissions=0x%08X", 
        operation, addr0, addr1, size, permissions);

    switch (operation) {

    // Map normal heap memory
    case MEMORY_OPERATION_HEAP:
        virtual_address = Memory::MapBlock_Heap(size, operation, permissions);
        break;

    // Map GSP heap memory
    case MEMORY_OPERATION_GSP_HEAP:
        virtual_address = Memory::MapBlock_HeapGSP(size, operation, permissions);
        break;

    // Unknown ControlMemory operation
    default:
        ERROR_LOG(SVC, "ControlMemory unknown operation=0x%08X", operation);
    }
    if (NULL != outaddr) {
        *outaddr = virtual_address;
    }
    Core::g_app_core->SetReg(1, virtual_address);

    return 0;
}

/// Maps a memory block to specified address
Result MapMemoryBlock(Handle memblock, u32 addr, u32 mypermissions, u32 otherpermission) {
    DEBUG_LOG(SVC, "MapMemoryBlock called memblock=0x08X, addr=0x%08X, mypermissions=0x%08X, otherpermission=%d", 
        memblock, addr, mypermissions, otherpermission);
    switch (mypermissions) {
    case MEMORY_PERMISSION_NORMAL:
    case MEMORY_PERMISSION_NORMAL + 1:
    case MEMORY_PERMISSION_NORMAL + 2:
        Memory::MapBlock_Shared(memblock, addr, mypermissions);
        break;
    default:
        ERROR_LOG(OSHLE, "MapMemoryBlock unknown permissions=0x%08X", mypermissions);
    }
    return 0;
}

/// Connect to an OS service given the port name, returns the handle to the port to out
Result ConnectToPort(void* out, const char* port_name) {
    Service::Interface* service = Service::g_manager->FetchFromPortName(port_name);
    Core::g_app_core->SetReg(1, service->GetHandle());
    DEBUG_LOG(SVC, "ConnectToPort called port_name=%s", port_name);
    return 0;
}

/// Synchronize to an OS service
Result SendSyncRequest(Handle handle) {
    DEBUG_LOG(SVC, "SendSyncRequest called handle=0x%08X");
    Service::Interface* service = Service::g_manager->FetchFromHandle(handle);
    service->Sync();
    return 0;
}

/// Close a handle
Result CloseHandle(Handle handle) {
    // ImplementMe
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) CloseHandle called handle=0x%08X", handle);
    return 0;
}

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
Result WaitSynchronization1(Handle handle, s64 nano_seconds) {
    // ImplementMe
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) WaitSynchronization1 called handle=0x%08X, nanoseconds=%d", 
        handle, nano_seconds);
    return 0;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
Result WaitSynchronizationN(void* _out, void* _handles, u32 handle_count, u32 wait_all, s64 nano_seconds) {
    s32* out = (s32*)_out;
    Handle* handles = (Handle*)_handles;
    // ImplementMe
    
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) WaitSynchronizationN called handle_count=%d, wait_all=%s, nanoseconds=%d %s", 
        handle_count, (wait_all ? "true" : "false"), nano_seconds);
    
    for (u32 i = 0; i < handle_count; i++) {
        DEBUG_LOG(SVC, "\thandle[%d]=0x%08X", i, handles[i]);
    }
    __KernelReschedule("WaitSynchronizationN");

    return 0;
}

/// Create an address arbiter (to allocate access to shared resources)
Result CreateAddressArbiter(void* arbiter) {
    // ImplementMe
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) CreateAddressArbiter called");
    Core::g_app_core->SetReg(1, 0xFABBDADD);
    return 0;
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
void OutputDebugString(const char* string) {
    NOTICE_LOG(SVC, "## OSDEBUG: %08X %s", Core::g_app_core->GetPC(), string);
}

/// Get resource limit
Result GetResourceLimit(void* resource_limit, Handle process) {
    // With regards to proceess values:
    // 0xFFFF8001 is a handle alias for the current KProcess, and 0xFFFF8000 is a handle alias for 
    // the current KThread.
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) GetResourceLimit called process=0x%08X", process);
    Core::g_app_core->SetReg(1, 0xDEADBEEF);
    return 0;
}

/// Get resource limit current values
Result GetResourceLimitCurrentValues(void* _values, Handle resource_limit, void* names, s32 name_count) {
    //s64* values = (s64*)_values;
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) GetResourceLimitCurrentValues called resource_limit=%08X, names=%s, name_count=%d",
        resource_limit, names, name_count);
    Memory::Write32(Core::g_app_core->GetReg(0), 0); // Normmatt: Set used memory to 0 for now
    return 0;
}

Result CreateThread(void* thread, u32 priority, u32 entry_point, u32 arg, u32 stack_top, 
    u32 processor_id) {
    std::string name;
    if (Symbols::HasSymbol(entry_point)) {
        TSymbol symbol = Symbols::GetSymbol(entry_point);
        name = symbol.name;
    } else {
        char buff[100];
        sprintf(buff, "%s", "unknown-%08X", entry_point);
        name = buff;
    }
    DEBUG_LOG(SVC, "CreateThread called entrypoint=0x%08X (%s), arg=0x%08X, stacktop=0x%08X, "
        "threadpriority=0x%08X, processorid=0x%08X", entry_point, name.c_str(), arg, stack_top,
        priority, processor_id);

    Handle handle = __KernelCreateThread(name.c_str(), entry_point, priority, processor_id, 
        stack_top);
    Core::g_app_core->SetReg(1, 0xFEEDDEAF);
    
    return 0;
}

Result CreateMutex(void* _mutex, u32 initial_locked) {
    Handle* mutex = (Handle*)_mutex;
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) CreateMutex called initial_locked=%s", 
        initial_locked ? "true" : "false");
    Core::g_app_core->SetReg(1, 0xF00D0BAD);
    return 0;
}

Result ReleaseMutex(Handle handle) {
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) ReleaseMutex called handle=0x%08X", handle);
    return 0;
}

Result GetThreadId(void* thread_id, u32 thread) {
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) GetThreadId called thread=0x%08X", thread);
    return 0;
}

Result QueryMemory(void *_info, void *_out, u32 addr) {
    MemoryInfo* info = (MemoryInfo*) _info;
    PageInfo* out = (PageInfo*) _out;
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) QueryMemory called addr=0x%08X", addr);
    return 0;
}

Result CreateEvent(void* _event, u32 reset_type) {
    Handle* event = (Handle*)_event;
    DEBUG_LOG(SVC, "(UNIMPLEMENTED) CreateEvent called reset_type=0x%08X", reset_type);
    Core::g_app_core->SetReg(1, 0xBADC0DE0);
    return 0;
}

const HLE::FunctionDef SVC_Table[] = {
    {0x00,  NULL,                                       "Unknown"},
    {0x01,  WrapI_VUUUUU<ControlMemory>,                "ControlMemory"},
    {0x02,  WrapI_VVU<QueryMemory>,                     "QueryMemory"},
    {0x03,  NULL,                                       "ExitProcess"},
    {0x04,  NULL,                                       "GetProcessAffinityMask"},
    {0x05,  NULL,                                       "SetProcessAffinityMask"},
    {0x06,  NULL,                                       "GetProcessIdealProcessor"},
    {0x07,  NULL,                                       "SetProcessIdealProcessor"},
    {0x08,  WrapI_VUUUUU<CreateThread>,                 "CreateThread"},
    {0x09,  NULL,                                       "ExitThread"},
    {0x0A,  NULL,                                       "SleepThread"},
    {0x0B,  NULL,                                       "GetThreadPriority"},
    {0x0C,  NULL,                                       "SetThreadPriority"},
    {0x0D,  NULL,                                       "GetThreadAffinityMask"},
    {0x0E,  NULL,                                       "SetThreadAffinityMask"},
    {0x0F,  NULL,                                       "GetThreadIdealProcessor"},
    {0x10,  NULL,                                       "SetThreadIdealProcessor"},
    {0x11,  NULL,                                       "GetCurrentProcessorNumber"},
    {0x12,  NULL,                                       "Run"},
    {0x13,  WrapI_VU<CreateMutex>,                      "CreateMutex"},
    {0x14,  WrapI_U<ReleaseMutex>,                      "ReleaseMutex"},
    {0x15,  NULL,                                       "CreateSemaphore"},
    {0x16,  NULL,                                       "ReleaseSemaphore"},
    {0x17,  WrapI_VU<CreateEvent>,                      "CreateEvent"},
    {0x18,  NULL,                                       "SignalEvent"},
    {0x19,  NULL,                                       "ClearEvent"},
    {0x1A,  NULL,                                       "CreateTimer"},
    {0x1B,  NULL,                                       "SetTimer"},
    {0x1C,  NULL,                                       "CancelTimer"},
    {0x1D,  NULL,                                       "ClearTimer"},
    {0x1E,  NULL,                                       "CreateMemoryBlock"},
    {0x1F,  WrapI_UUUU<MapMemoryBlock>,                 "MapMemoryBlock"},
    {0x20,  NULL,                                       "UnmapMemoryBlock"},
    {0x21,  WrapI_V<CreateAddressArbiter>,              "CreateAddressArbiter"},
    {0x22,  NULL,                                       "ArbitrateAddress"},
    {0x23,  WrapI_U<CloseHandle>,                       "CloseHandle"},
    {0x24,  WrapI_US64<WaitSynchronization1>,           "WaitSynchronization1"},
    {0x25,  WrapI_VVUUS64<WaitSynchronizationN>,        "WaitSynchronizationN"},
    {0x26,  NULL,                                       "SignalAndWait"},
    {0x27,  NULL,                                       "DuplicateHandle"},
    {0x28,  NULL,                                       "GetSystemTick"},
    {0x29,  NULL,                                       "GetHandleInfo"},
    {0x2A,  NULL,                                       "GetSystemInfo"},
    {0x2B,  NULL,                                       "GetProcessInfo"},
    {0x2C,  NULL,                                       "GetThreadInfo"},
    {0x2D,  WrapI_VC<ConnectToPort>,                    "ConnectToPort"},
    {0x2E,  NULL,                                       "SendSyncRequest1"},
    {0x2F,  NULL,                                       "SendSyncRequest2"},
    {0x30,  NULL,                                       "SendSyncRequest3"},
    {0x31,  NULL,                                       "SendSyncRequest4"},
    {0x32,  WrapI_U<SendSyncRequest>,                   "SendSyncRequest"},
    {0x33,  NULL,                                       "OpenProcess"},
    {0x34,  NULL,                                       "OpenThread"},
    {0x35,  NULL,                                       "GetProcessId"},
    {0x36,  NULL,                                       "GetProcessIdOfThread"},
    {0x37,  WrapI_VU<GetThreadId>,                      "GetThreadId"},
    {0x38,  WrapI_VU<GetResourceLimit>,                 "GetResourceLimit"},
    {0x39,  NULL,                                       "GetResourceLimitLimitValues"},
    {0x3A,  WrapI_VUVI<GetResourceLimitCurrentValues>,  "GetResourceLimitCurrentValues"},
    {0x3B,  NULL,                                       "GetThreadContext"},
    {0x3C,  NULL,                                       "Break"},
    {0x3D,  WrapV_C<OutputDebugString>,                 "OutputDebugString"},
    {0x3E,  NULL,                                       "ControlPerformanceCounter"},
    {0x3F,  NULL,                                       "Unknown"},
    {0x40,  NULL,                                       "Unknown"},
    {0x41,  NULL,                                       "Unknown"},
    {0x42,  NULL,                                       "Unknown"},
    {0x43,  NULL,                                       "Unknown"},
    {0x44,  NULL,                                       "Unknown"},
    {0x45,  NULL,                                       "Unknown"},
    {0x46,  NULL,                                       "Unknown"},
    {0x47,  NULL,                                       "CreatePort"},
    {0x48,  NULL,                                       "CreateSessionToPort"},
    {0x49,  NULL,                                       "CreateSession"},
    {0x4A,  NULL,                                       "AcceptSession"},
    {0x4B,  NULL,                                       "ReplyAndReceive1"},
    {0x4C,  NULL,                                       "ReplyAndReceive2"},
    {0x4D,  NULL,                                       "ReplyAndReceive3"},
    {0x4E,  NULL,                                       "ReplyAndReceive4"},
    {0x4F,  NULL,                                       "ReplyAndReceive"},
    {0x50,  NULL,                                       "BindInterrupt"},
    {0x51,  NULL,                                       "UnbindInterrupt"},
    {0x52,  NULL,                                       "InvalidateProcessDataCache"},
    {0x53,  NULL,                                       "StoreProcessDataCache"},
    {0x54,  NULL,                                       "FlushProcessDataCache"},
    {0x55,  NULL,                                       "StartInterProcessDma"},
    {0x56,  NULL,                                       "StopDma"},
    {0x57,  NULL,                                       "GetDmaState"},
    {0x58,  NULL,                                       "RestartDma"},
    {0x59,  NULL,                                       "Unknown"},
    {0x5A,  NULL,                                       "Unknown"},
    {0x5B,  NULL,                                       "Unknown"},
    {0x5C,  NULL,                                       "Unknown"},
    {0x5D,  NULL,                                       "Unknown"},
    {0x5E,  NULL,                                       "Unknown"},
    {0x5F,  NULL,                                       "Unknown"},
    {0x60,  NULL,                                       "DebugActiveProcess"},
    {0x61,  NULL,                                       "BreakDebugProcess"},
    {0x62,  NULL,                                       "TerminateDebugProcess"},
    {0x63,  NULL,                                       "GetProcessDebugEvent"},
    {0x64,  NULL,                                       "ContinueDebugEvent"},
    {0x65,  NULL,                                       "GetProcessList"},
    {0x66,  NULL,                                       "GetThreadList"},
    {0x67,  NULL,                                       "GetDebugThreadContext"},
    {0x68,  NULL,                                       "SetDebugThreadContext"},
    {0x69,  NULL,                                       "QueryDebugProcessMemory"},
    {0x6A,  NULL,                                       "ReadProcessMemory"},
    {0x6B,  NULL,                                       "WriteProcessMemory"},
    {0x6C,  NULL,                                       "SetHardwareBreakPoint"},
    {0x6D,  NULL,                                       "GetDebugThreadParam"},
    {0x6E,  NULL,                                       "Unknown"},
    {0x6F,  NULL,                                       "Unknown"},
    {0x70,  NULL,                                       "ControlProcessMemory"},
    {0x71,  NULL,                                       "MapProcessMemory"},
    {0x72,  NULL,                                       "UnmapProcessMemory"},
    {0x73,  NULL,                                       "Unknown"},
    {0x74,  NULL,                                       "Unknown"},
    {0x75,  NULL,                                       "Unknown"},
    {0x76,  NULL,                                       "TerminateProcess"},
    {0x77,  NULL,                                       "Unknown"},
    {0x78,  NULL,                                       "CreateResourceLimit"},
    {0x79,  NULL,                                       "Unknown"},
    {0x7A,  NULL,                                       "Unknown"},
    {0x7B,  NULL,                                       "Unknown"},
    {0x7C,  NULL,                                       "KernelSetState"},
    {0x7D,  NULL,                                       "QueryProcessMemory"},
};

void Register() {
    HLE::RegisterModule("SVC_Table", ARRAY_SIZE(SVC_Table), SVC_Table);
}

} // namespace
