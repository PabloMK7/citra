// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service {
namespace NFC {

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 0);
    u8 param = rp.Pop<u8>();

    nfc->nfc_tag_state = TagState::NotScanning;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called, param={}", param);
}

void Module::Interface::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 0);
    u8 param = rp.Pop<u8>();

    nfc->nfc_tag_state = TagState::NotInitialized;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called, param={}", param);
}

void Module::Interface::StartCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::StopCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::StartTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 0); // 0x00050040
    u16 in_val = rp.Pop<u16>();

    ResultCode result = RESULT_SUCCESS;

    // TODO(shinyquagsire23): Implement NFC tag detection, for now stub result
    result = ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                        ErrorSummary::InvalidState, ErrorLevel::Status);

    if (result == RESULT_SUCCESS) {
        nfc->nfc_tag_state = TagState::TagInRange;
        nfc->tag_in_range_event->Signal();
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called, in_val={:04x}", in_val);
}

void Module::Interface::StopTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 0, 0);

    nfc->nfc_tag_state = TagState::NotScanning;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::LoadAmiiboData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 0, 0);

    nfc->nfc_tag_state = TagState::TagDataLoaded;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::ResetTagScanState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    nfc->nfc_tag_state = TagState::NotScanning;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetTagInRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_in_range_event);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_out_of_range_event);
    NGLOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetTagState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->nfc_tag_state);
    NGLOG_DEBUG(Service_NFC, "(STUBBED) called");
}

void Module::Interface::CommunicationGetStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->nfc_status);
    NGLOG_DEBUG(Service_NFC, "(STUBBED) called");
}

Module::Interface::Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), nfc(std::move(nfc)) {}

Module::Interface::~Interface() = default;

Module::Module() {
    tag_in_range_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
    tag_out_of_range_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NFC::tag_out_range_event");
}

Module::~Module() = default;

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto nfc = std::make_shared<Module>();
    std::make_shared<NFC_M>(nfc)->InstallAsService(service_manager);
    std::make_shared<NFC_U>(nfc)->InstallAsService(service_manager);
}

} // namespace NFC
} // namespace Service
