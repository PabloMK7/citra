// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/srv.h"
#include "core/hle/service/service.h"
#include "core/hle/kernel/event.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SRV

namespace SRV {

Handle g_event_handle = 0;

void Initialize(Service::Interface* self) {
    DEBUG_LOG(OSHLE, "called");
}

void GetProcSemaphore(Service::Interface* self) {
    DEBUG_LOG(OSHLE, "called");

    u32* cmd_buff = Service::GetCommandBuffer();

    // TODO(bunnei): Change to a semaphore once these have been implemented
    g_event_handle = Kernel::CreateEvent(RESETTYPE_ONESHOT, "SRV:Event");
    Kernel::SetEventLocked(g_event_handle, false);

    cmd_buff[1] = 0; // No error
    cmd_buff[3] = g_event_handle;
}

void GetServiceHandle(Service::Interface* self) {
    Result res = 0;
    u32* cmd_buff = Service::GetCommandBuffer();

    std::string port_name = std::string((const char*)&cmd_buff[1], 0, Service::kMaxPortSize);
    Service::Interface* service = Service::g_manager->FetchFromPortName(port_name);

    if (nullptr != service) {
        cmd_buff[3] = service->GetHandle();
        DEBUG_LOG(OSHLE, "called port=%s, handle=0x%08X", port_name.c_str(), cmd_buff[3]);
    } else {
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED) called port=%s", port_name.c_str());
        res = -1;
    }
    cmd_buff[1] = res;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, Initialize,        "Initialize"},
    {0x00020000, GetProcSemaphore,  "GetProcSemaphore"},
    {0x00030100, nullptr,           "RegisterService"},
    {0x000400C0, nullptr,           "UnregisterService"},
    {0x00050100, GetServiceHandle,  "GetServiceHandle"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
