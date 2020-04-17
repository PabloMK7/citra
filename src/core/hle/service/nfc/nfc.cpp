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
    ar& tag_in_range_event;
    ar& tag_out_of_range_event;
    ar& nfc_tag_state;
    ar& nfc_status;
    ar& amiibo_data;
    ar& amiibo_in_range;
}
SERIALIZE_IMPL(Module)

struct TagInfo {
    u16_le id_offset_size;
    u8 unk1;
    u8 unk2;
    std::array<u8, 7> uuid;
    INSERT_PADDING_BYTES(0x20);
};
static_assert(sizeof(TagInfo) == 0x2C, "TagInfo is an invalid size");

struct AmiiboConfig {
    u16_le lastwritedate_year;
    u8 lastwritedate_month;
    u8 lastwritedate_day;
    u16_le write_counter;
    std::array<u8, 3> characterID;
    u8 series;
    u16_le amiiboID;
    u8 type;
    u8 pagex4_byte3;
    u16_le appdata_size;
    INSERT_PADDING_BYTES(0x30);
};
static_assert(sizeof(AmiiboConfig) == 0x40, "AmiiboConfig is an invalid size");

struct IdentificationBlockReply {
    u16_le char_id;
    u8 char_variant;
    u8 series;
    u16_le model_number;
    u8 figure_type;
    INSERT_PADDING_BYTES(0x2F);
};
static_assert(sizeof(IdentificationBlockReply) == 0x36,
              "IdentificationBlockReply is an invalid size");

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 0);
    u8 param = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_tag_state != TagState::NotInitialized) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    nfc->nfc_tag_state = TagState::NotScanning;

    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called, param={}", param);
}

void Module::Interface::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 0);
    u8 param = rp.Pop<u8>();

    nfc->nfc_tag_state = TagState::NotInitialized;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called, param={}", param);
}

void Module::Interface::StartCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::StopCommunication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::StartTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 0); // 0x00050040
    u16 in_val = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_tag_state != TagState::NotScanning &&
        nfc->nfc_tag_state != TagState::TagOutOfRange) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    nfc->nfc_tag_state = TagState::Scanning;
    nfc->SyncTagState();

    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called, in_val={:04x}", in_val);
}

void Module::Interface::GetTagInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0);

    if (nfc->nfc_tag_state != TagState::TagInRange &&
        nfc->nfc_tag_state != TagState::TagDataLoaded && nfc->nfc_tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    TagInfo tag_info{};
    tag_info.uuid = nfc->amiibo_data.uuid;
    tag_info.id_offset_size = tag_info.uuid.size();
    tag_info.unk1 = 0x0;
    tag_info.unk2 = 0x2;

    IPC::RequestBuilder rb = rp.MakeBuilder(12, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<TagInfo>(tag_info);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetAmiiboConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 0, 0);

    AmiiboConfig amiibo_config{};
    amiibo_config.lastwritedate_year = 2017;
    amiibo_config.lastwritedate_month = 10;
    amiibo_config.lastwritedate_day = 10;
    amiibo_config.write_counter = 0x0;
    std::memcpy(amiibo_config.characterID.data(), &nfc->amiibo_data.char_id,
                sizeof(nfc->amiibo_data.char_id));
    amiibo_config.series = nfc->amiibo_data.series;
    amiibo_config.amiiboID = nfc->amiibo_data.model_number;
    amiibo_config.type = nfc->amiibo_data.figure_type;
    amiibo_config.pagex4_byte3 = 0x0;
    amiibo_config.appdata_size = 0xD8;

    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<AmiiboConfig>(amiibo_config);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::StopTagScanning(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_tag_state == TagState::NotInitialized ||
        nfc->nfc_tag_state == TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    nfc->nfc_tag_state = TagState::NotScanning;

    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::LoadAmiiboData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 0, 0);

    // TODO(FearlessTobi): Add state checking when this function gets properly implemented

    nfc->nfc_tag_state = TagState::TagDataLoaded;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::ResetTagScanState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_tag_state != TagState::TagDataLoaded && nfc->nfc_tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    nfc->nfc_tag_state = TagState::TagInRange;
    nfc->SyncTagState();

    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagInRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 0, 0);

    if (nfc->nfc_tag_state != TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_in_range_event);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 0, 0);

    if (nfc->nfc_tag_state != TagState::NotScanning) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nfc->tag_out_of_range_event);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetTagState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->nfc_tag_state);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::CommunicationGetStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nfc->nfc_status);
    LOG_DEBUG(Service_NFC, "(STUBBED) called");
}

void Module::Interface::Unknown0x1A(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_tag_state != TagState::TagInRange) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    nfc->nfc_tag_state = TagState::Unknown6;

    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_NFC, "called");
}

void Module::Interface::GetIdentificationBlock(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 0, 0);

    if (nfc->nfc_tag_state != TagState::TagDataLoaded && nfc->nfc_tag_state != TagState::Unknown6) {
        LOG_ERROR(Service_NFC, "Invalid TagState {}", static_cast<int>(nfc->nfc_tag_state));
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::CommandInvalidForState, ErrorModule::NFC,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    IdentificationBlockReply identification_block_reply{};
    identification_block_reply.char_id = nfc->amiibo_data.char_id;
    identification_block_reply.char_variant = nfc->amiibo_data.char_variant;
    identification_block_reply.series = nfc->amiibo_data.series;
    identification_block_reply.model_number = nfc->amiibo_data.model_number;
    identification_block_reply.figure_type = nfc->amiibo_data.figure_type;

    IPC::RequestBuilder rb = rp.MakeBuilder(0x1F, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<IdentificationBlockReply>(identification_block_reply);
    LOG_DEBUG(Service_NFC, "called");
}

std::shared_ptr<Module> Module::Interface::GetModule() const {
    return nfc;
}

void Module::Interface::LoadAmiibo(const AmiiboData& amiibo_data) {
    std::lock_guard lock(HLE::g_hle_lock);
    nfc->amiibo_data = amiibo_data;
    nfc->amiibo_in_range = true;
    nfc->SyncTagState();
}

void Module::Interface::RemoveAmiibo() {
    std::lock_guard lock(HLE::g_hle_lock);
    nfc->amiibo_in_range = false;
    nfc->SyncTagState();
}

void Module::SyncTagState() {
    if (amiibo_in_range &&
        (nfc_tag_state == TagState::TagOutOfRange || nfc_tag_state == TagState::Scanning)) {
        // TODO (wwylele): Should TagOutOfRange->TagInRange transition only happen on the same tag
        // detected on Scanning->TagInRange?
        nfc_tag_state = TagState::TagInRange;
        tag_in_range_event->Signal();
    } else if (!amiibo_in_range &&
               (nfc_tag_state == TagState::TagInRange || nfc_tag_state == TagState::TagDataLoaded ||
                nfc_tag_state == TagState::Unknown6)) {
        // TODO (wwylele): If a tag is removed during TagDataLoaded/Unknown6, should this event
        // signals early?
        nfc_tag_state = TagState::TagOutOfRange;
        tag_out_of_range_event->Signal();
    }
}

Module::Interface::Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), nfc(std::move(nfc)) {}

Module::Interface::~Interface() = default;

Module::Module(Core::System& system) {
    tag_in_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
    tag_out_of_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_out_range_event");
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto nfc = std::make_shared<Module>(system);
    std::make_shared<NFC_M>(nfc)->InstallAsService(service_manager);
    std::make_shared<NFC_U>(nfc)->InstallAsService(service_manager);
}

} // namespace Service::NFC
