// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

static Kernel::SharedPtr<Kernel::Event> cecinfo_event;
static Kernel::SharedPtr<Kernel::Event> change_state_event;

void GetCecStateAbbreviated(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(CecStateAbbreviated::CEC_STATE_ABBREV_IDLE);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void GetCecInfoEventHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;                                      // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(cecinfo_event).MoveFrom(); // Event handle

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void GetChangeStateEventHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;                                           // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(change_state_event).MoveFrom(); // Event handle

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Init() {
    AddService(new CECD_S_Interface);
    AddService(new CECD_U_Interface);

    cecinfo_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "CECD_U::cecinfo_event");
    change_state_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "CECD_U::change_state_event");
}

void Shutdown() {
    cecinfo_event = nullptr;
    change_state_event = nullptr;
}

} // namespace CECD

} // namespace Service
