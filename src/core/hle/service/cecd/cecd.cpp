// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_ndm.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

void Module::Interface::GetCecStateAbbreviated(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(CecStateAbbreviated::CEC_STATE_ABBREV_IDLE);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetCecInfoEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(cecd->cecinfo_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetChangeStateEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(cecd->change_state_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

Module::Interface::Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cecd(std::move(cecd)) {}

Module::Module() {
    using namespace Kernel;
    cecinfo_event = Event::Create(Kernel::ResetType::OneShot, "CECD::cecinfo_event");
    change_state_event = Event::Create(Kernel::ResetType::OneShot, "CECD::change_state_event");
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cecd = std::make_shared<Module>();
    std::make_shared<CECD_NDM>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_S>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_U>(cecd)->InstallAsService(service_manager);
}

} // namespace CECD
} // namespace Service
