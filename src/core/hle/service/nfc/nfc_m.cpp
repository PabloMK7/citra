// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nfc/nfc_m.h"

SERIALIZE_EXPORT_IMPL(Service::NFC::NFC_M)

namespace Service::NFC {

NFC_M::NFC_M(std::shared_ptr<Module> nfc) : Module::Interface(std::move(nfc), "nfc:m", 1) {
    static const FunctionInfo functions[] = {
        // nfc:u shared commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 0), &NFC_M::Initialize, "Initialize"},
        {IPC::MakeHeader(0x0002, 1, 0), &NFC_M::Shutdown, "Shutdown"},
        {IPC::MakeHeader(0x0003, 0, 0), &NFC_M::StartCommunication, "StartCommunication"},
        {IPC::MakeHeader(0x0004, 0, 0), &NFC_M::StopCommunication, "StopCommunication"},
        {IPC::MakeHeader(0x0005, 1, 0), &NFC_M::StartTagScanning, "StartTagScanning"},
        {IPC::MakeHeader(0x0006, 0, 0), &NFC_M::StopTagScanning, "StopTagScanning"},
        {IPC::MakeHeader(0x0007, 0, 0), &NFC_M::LoadAmiiboData, "LoadAmiiboData"},
        {IPC::MakeHeader(0x0008, 0, 0), &NFC_M::ResetTagScanState, "ResetTagScanState"},
        {IPC::MakeHeader(0x0009, 0, 2), &NFC_M::UpdateStoredAmiiboData, "UpdateStoredAmiiboData"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "Unknown0x0A"},
        {IPC::MakeHeader(0x000B, 0, 0), &NFC_M::GetTagInRangeEvent, "GetTagInRangeEvent"},
        {IPC::MakeHeader(0x000C, 0, 0), &NFC_M::GetTagOutOfRangeEvent, "GetTagOutOfRangeEvent"},
        {IPC::MakeHeader(0x000D, 0, 0), &NFC_M::GetTagState, "GetTagState"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "Unknown0x0E"},
        {IPC::MakeHeader(0x000F, 0, 0), &NFC_M::CommunicationGetStatus, "CommunicationGetStatus"},
        {IPC::MakeHeader(0x0010, 0, 0), &NFC_M::GetTagInfo2, "GetTagInfo2"},
        {IPC::MakeHeader(0x0011, 0, 0), &NFC_M::GetTagInfo, "GetTagInfo"},
        {IPC::MakeHeader(0x0012, 0, 0), &NFC_M::CommunicationGetResult, "CommunicationGetResult"},
        {IPC::MakeHeader(0x0013, 1, 0), &NFC_M::OpenAppData, "OpenAppData"},
        {IPC::MakeHeader(0x0014, 14, 4), &NFC_M::InitializeWriteAppData, "InitializeWriteAppData"},
        {IPC::MakeHeader(0x0015, 1, 0), &NFC_M::ReadAppData, "ReadAppData"},
        {IPC::MakeHeader(0x0016, 9, 2), &NFC_M::WriteAppData, "WriteAppData"},
        {IPC::MakeHeader(0x0017, 0, 0), &NFC_M::GetRegisterInfo, "GetRegisterInfo"},
        {IPC::MakeHeader(0x0018, 0, 0), &NFC_M::GetCommonInfo, "GetCommonInfo"},
        {IPC::MakeHeader(0x0019, 0, 0), &NFC_M::GetAppDataInitStruct, "GetAppDataInitStruct"},
        {IPC::MakeHeader(0x001A, 0, 0), &NFC_M::LoadAmiiboPartially, "LoadAmiiboPartially"},
        {IPC::MakeHeader(0x001B, 0, 0), &NFC_M::GetIdentificationBlock, "GetIdentificationBlock"},
        // nfc:m
        {IPC::MakeHeader(0x0401, 3, 2), &NFC_M::Format, "Format"},
        {IPC::MakeHeader(0x0402, 0, 0), &NFC_M::GetAdminInfo, "GetAdminInfo"},
        {IPC::MakeHeader(0x0403, 0, 0), &NFC_M::GetEmptyRegisterInfo, "GetEmptyRegisterInfo"},
        {IPC::MakeHeader(0x0404, 41, 0), &NFC_M::SetRegisterInfo, "SetRegisterInfo"},
        {IPC::MakeHeader(0x0405, 0, 0), &NFC_M::DeleteRegisterInfo, "DeleteRegisterInfo"},
        {IPC::MakeHeader(0x0406, 0, 0), &NFC_M::DeleteApplicationArea, "DeleteApplicationArea"},
        {IPC::MakeHeader(0x0407, 0, 0), &NFC_M::ExistsApplicationArea, "ExistsApplicationArea"}
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NFC
