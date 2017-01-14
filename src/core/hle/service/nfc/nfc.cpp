// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service {
namespace NFC {

static Kernel::SharedPtr<Kernel::Event> tag_in_range_event;
static Kernel::SharedPtr<Kernel::Event> tag_out_of_range_event;
static TagState nfc_tag_state = TagState::NotInitialized;
static CommunicationStatus nfc_status = CommunicationStatus::NfcInitialized;

void Initialize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 param = static_cast<u8>(cmd_buff[1] & 0xFF);

    nfc_tag_state = TagState::NotScanning;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called, param=%u", param);
}

void Shutdown(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 param = static_cast<u8>(cmd_buff[1] & 0xFF);
    nfc_tag_state = TagState::NotInitialized;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called, param=%u", param);
}

void StartCommunication(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void StopCommunication(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void StartTagScanning(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    nfc_tag_state = TagState::TagInRange;
    tag_in_range_event->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void StopTagScanning(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    nfc_tag_state = TagState::NotScanning;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void LoadAmiiboData(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    nfc_tag_state = TagState::TagDataLoaded;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void ResetTagScanState(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    nfc_tag_state = TagState::NotScanning;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void GetTagInRangeEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xB, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc();
    cmd_buff[3] = Kernel::g_handle_table.Create(tag_in_range_event).MoveFrom();
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void GetTagOutOfRangeEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xC, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc();
    cmd_buff[3] = Kernel::g_handle_table.Create(tag_out_of_range_event).MoveFrom();
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void GetTagState(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u8>(nfc_tag_state);
    LOG_DEBUG(Service_NFC, "(STUBBED) called");
}

void CommunicationGetStatus(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u8>(nfc_status);
    LOG_DEBUG(Service_NFC, "(STUBBED) called");
}

void Init() {
    AddService(new NFC_M());
    AddService(new NFC_U());

    tag_in_range_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
    tag_out_of_range_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NFC::tag_out_range_event");
    nfc_tag_state = TagState::NotInitialized;
}

void Shutdown() {
    tag_in_range_event = nullptr;
    tag_out_of_range_event = nullptr;
}

} // namespace NFC
} // namespace Service
