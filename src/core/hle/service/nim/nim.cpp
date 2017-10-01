// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_aoc.h"
#include "core/hle/service/nim/nim_s.h"
#include "core/hle/service/nim/nim_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace NIM {

static Kernel::SharedPtr<Kernel::Event> nim_system_update_event;

void CheckForSysUpdateEvent(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x5, 0, 0); // 0x50000
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(nim_system_update_event).Unwrap());
    LOG_TRACE(Service_NIM, "called");
}

void CheckSysUpdateAvailable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // No update available

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new NIM_AOC_Interface);
    AddService(new NIM_S_Interface);
    AddService(new NIM_U_Interface);

    nim_system_update_event = Kernel::Event::Create(ResetType::OneShot, "NIM System Update Event");
}

void Shutdown() {
    nim_system_update_event = nullptr;
}

} // namespace NIM

} // namespace Service
