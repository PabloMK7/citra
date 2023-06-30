// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/lock.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

SERVICE_CONSTRUCT_IMPL(Service::NFC::Module)
SERIALIZE_EXPORT_IMPL(Service::NFC::Module)

namespace Service::NFC {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar& nfc_mode;
    ar& device;
}
SERIALIZE_IMPL(Module)

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 0);
    const auto communication_mode = rp.PopEnum<CommunicationMode>();

    LOG_INFO(Service_NFC, "called, communication_mode={}", communication_mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (nfc->nfc_mode != CommunicationMode::NotInitialized) {
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    ResultCode result = RESULT_SUCCESS;
    switch (communication_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        nfc->device->Initialize();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", communication_mode);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    if (result.IsSuccess()) {
        nfc->nfc_mode = communication_mode;
    }

    rb.Push(result);
}

void Module::Interface::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 0);
    const auto communication_mode = rp.PopEnum<CommunicationMode>();

    LOG_INFO(Service_NFC, "called, communication_mode={}", communication_mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (nfc->nfc_mode != communication_mode) {
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    ResultCode result = RESULT_SUCCESS;
    switch (communication_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        nfc->device->Finalize();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", communication_mode);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    if (result.IsSuccess()) {
        nfc->nfc_mode = CommunicationMode::NotInitialized;
    }

    rb.Push(result);
}

void Module::Interface::StartCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    LOG_WARNING(Service_NFC, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        rb.Push(RESULT_SUCCESS);
        return;
    }

    // TODO: call start communication instead
    const auto result = nfc->device->StartCommunication();
    rb.Push(result);
}

void Module::Interface::StopCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 0);

    LOG_WARNING(Service_NFC, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        rb.Push(RESULT_SUCCESS);
        return;
    }

    // TODO: call stop communication instead
    const auto result = nfc->device->StopCommunication();
    rb.Push(result);
}

void Module::Interface::StartTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 0);
    u16 in_val = rp.Pop<u16>();

    LOG_INFO(Service_NFC, "called, in_val={:04x}", in_val);

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        // in_val probably correlates to the tag protocol to be detected
        result = nfc->device->StartDetection(TagProtocol::All);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::StopTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 0, 0);

    LOG_INFO(Service_NFC, "called");

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        result = nfc->device->StopDetection();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        break;
    default:
        result = ResultCommandInvalidForState;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::LoadAmiiboData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 0, 0);

    LOG_INFO(Service_NFC, "called");

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        result = nfc->device->Mount();
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->MountAmiibo();
        break;
    default:
        result = ResultCommandInvalidForState;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::ResetTagScanState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    LOG_INFO(Service_NFC, "called");

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        result = nfc->device->ResetTagScanState();
        break;
    default:
        result = ResultCommandInvalidForState;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::UpdateStoredAmiiboData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 0, 0);

    LOG_INFO(Service_NFC, "called");

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->Flush();
        break;
    default:
        result = ResultCommandInvalidForState;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetTagInRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->device->GetActivateEvent());
}

void Module::Interface::GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->device->GetDeactivateEvent());
}

void Module::Interface::GetTagState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 0, 0);
    DeviceState state = DeviceState::NotInitialized;

    LOG_DEBUG(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
    } else {
        state = nfc->device->GetCurrentState();
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(state);
}

void Module::Interface::CommunicationGetStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    LOG_DEBUG(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        rb.PushEnum(CommunicationState::Idle);
        return;
    }

    CommunicationState status{};
    const auto result = nfc->device->GetCommunicationStatus(status);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(result);
    rb.PushEnum(status);
}

void Module::Interface::GetTagInfo2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(26, 0);
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<TagInfo2>({});
        return;
    }

    TagInfo2 tag_info{};
    const auto result = nfc->device->GetTagInfo2(tag_info);
    IPC::RequestBuilder rb = rp.MakeBuilder(26, 0);
    rb.Push(result);
    rb.PushRaw<TagInfo2>(tag_info);
}

void Module::Interface::GetTagInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(12, 0);
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<TagInfo>({});
        return;
    }

    TagInfo tag_info{};
    const auto result = nfc->device->GetTagInfo(tag_info);
    IPC::RequestBuilder rb = rp.MakeBuilder(12, 0);
    rb.Push(result);
    rb.PushRaw<TagInfo>(tag_info);
}

void Module::Interface::CommunicationGetResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::OpenAppData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 1, 0);
    u32 access_id = rp.Pop<u32>();

    LOG_INFO(Service_NFC, "called, access_id={}", access_id);

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->OpenApplicationArea(access_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::InitializeWriteAppData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 18, 2);
    u32 access_id = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_CRITICAL(Service_NFC, "called, size={}", size);

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->CreateApplicationArea(access_id, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::ReadAppData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    std::vector<u8> buffer(sizeof(ApplicationArea));
    const auto result = nfc->device->GetApplicationArea(buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushStaticBuffer(buffer, 0);
}

void Module::Interface::WriteAppData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 12, 2);
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    std::vector<u8> tag_uuid_info = rp.PopStaticBuffer();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_CRITICAL(Service_NFC, "called, size={}", size);

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->SetApplicationArea(buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    RegisterInfo settings_info{};
    const auto result = nfc->device->GetRegisterInfo(settings_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(43, 0);
    rb.Push(result);
    rb.PushRaw<RegisterInfo>(settings_info);
}

void Module::Interface::GetCommonInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    CommonInfo amiibo_config{};
    const auto result = nfc->device->GetCommonInfo(amiibo_config);

    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(result);
    rb.PushRaw<CommonInfo>(amiibo_config);
}

void Module::Interface::GetAppDataInitStruct(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x19, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    using InitialStruct = std::array<u8, 0x3c>;
    InitialStruct empty{};

    IPC::RequestBuilder rb = rp.MakeBuilder(16, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<InitialStruct>(empty);
}

void Module::Interface::LoadAmiiboPartially(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 0, 0);

    LOG_INFO(Service_NFC, "called");

    ResultCode result = RESULT_SUCCESS;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        result = nfc->device->PartiallyMount();
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->PartiallyMountAmiibo();
        break;
    default:
        result = ResultCommandInvalidForState;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetIdentificationBlock(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    ModelInfo model_info{};
    const auto result = nfc->device->GetModelInfo(model_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(0x1F, 0);
    rb.Push(result);
    rb.PushRaw<ModelInfo>(model_info);
}

void Module::Interface::Format(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x401, 3, 2);
    [[maybe_unused]] u32 unknown1 = rp.Pop<u32>();
    [[maybe_unused]] u32 unknown2 = rp.Pop<u32>();
    [[maybe_unused]] u32 unknown3 = rp.Pop<u32>();
    [[maybe_unused]] std::vector<u8> buffer = rp.PopStaticBuffer();

    const auto result = nfc->device->Format();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetAdminInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x402, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    AdminInfo admin_info{};
    const auto result = nfc->device->GetAdminInfo(admin_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(result);
    rb.PushRaw<AdminInfo>(admin_info);
}

void Module::Interface::GetEmptyRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x403, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(43, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<RegisterInfo>({});
}

void Module::Interface::SetRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x404, 41, 0);
    const auto register_info = rp.PopRaw<RegisterInfoPrivate>();

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->SetRegisterInfoPrivate(register_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::DeleteRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x405, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->DeleteRegisterInfo();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::DeleteApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x406, 0, 0);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    const auto result = nfc->device->DeleteApplicationArea();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::ExistsApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x407, 0, 0);

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCommandInvalidForState);
        return;
    }

    bool has_application_area = false;
    const auto result = nfc->device->ApplicationAreaExist(has_application_area);

    IPC::RequestBuilder rb = rp.MakeBuilder(0x2, 0);
    rb.Push(result);
    rb.Push(has_application_area);
    LOG_INFO(Service_NFC, "called");
}

std::shared_ptr<Module> Module::Interface::GetModule() const {
    return nfc;
}

bool Module::Interface::IsSearchingForAmiibos() {
    std::lock_guard lock(HLE::g_hle_lock);

    const auto state = nfc->device->GetCurrentState();
    return state == DeviceState::SearchingForTag;
}

bool Module::Interface::IsTagActive() {
    std::lock_guard lock(HLE::g_hle_lock);

    const auto state = nfc->device->GetCurrentState();
    return state == DeviceState::TagFound || state == DeviceState::TagMounted ||
           state == DeviceState::TagPartiallyMounted;
}
bool Module::Interface::LoadAmiibo(const std::string& fullpath) {
    std::lock_guard lock(HLE::g_hle_lock);
    return nfc->device->LoadAmiibo(fullpath);
}

void Module::Interface::RemoveAmiibo() {
    std::lock_guard lock(HLE::g_hle_lock);
    nfc->device->UnloadAmiibo();
}

Module::Interface::Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), nfc(std::move(nfc)) {}

Module::Interface::~Interface() = default;

Module::Module(Core::System& system) {
    device = std::make_shared<NfcDevice>(system);
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto nfc = std::make_shared<Module>(system);
    std::make_shared<NFC_M>(nfc)->InstallAsService(service_manager);
    std::make_shared<NFC_U>(nfc)->InstallAsService(service_manager);
}

} // namespace Service::NFC
