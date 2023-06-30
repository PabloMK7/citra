// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nfc/nfc_u.h"

SERIALIZE_EXPORT_IMPL(Service::NFC::NFC_U)

namespace Service::NFC {

NFC_U::NFC_U(std::shared_ptr<Module> nfc) : Module::Interface(std::move(nfc), "nfc:u", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 0), &NFC_U::Initialize, "Initialize"},
        {IPC::MakeHeader(0x0002, 1, 0), &NFC_U::Shutdown, "Shutdown"},
        {IPC::MakeHeader(0x0003, 0, 0), &NFC_U::StartCommunication, "StartCommunication"},
        {IPC::MakeHeader(0x0004, 0, 0), &NFC_U::StopCommunication, "StopCommunication"},
        {IPC::MakeHeader(0x0005, 1, 0), &NFC_U::StartTagScanning, "StartTagScanning"},
        {IPC::MakeHeader(0x0006, 0, 0), &NFC_U::StopTagScanning, "StopTagScanning"},
        {IPC::MakeHeader(0x0007, 0, 0), &NFC_U::LoadAmiiboData, "LoadAmiiboData"},
        {IPC::MakeHeader(0x0008, 0, 0), &NFC_U::ResetTagScanState, "ResetTagScanState"},
        {IPC::MakeHeader(0x0009, 0, 2), &NFC_U::UpdateStoredAmiiboData, "UpdateStoredAmiiboData"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "Unknown0x0A"},
        {IPC::MakeHeader(0x000B, 0, 0), &NFC_U::GetTagInRangeEvent, "GetTagInRangeEvent"},
        {IPC::MakeHeader(0x000C, 0, 0), &NFC_U::GetTagOutOfRangeEvent, "GetTagOutOfRangeEvent"},
        {IPC::MakeHeader(0x000D, 0, 0), &NFC_U::GetTagState, "GetTagState"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "Unknown0x0E"},
        {IPC::MakeHeader(0x000F, 0, 0), &NFC_U::CommunicationGetStatus, "CommunicationGetStatus"},
        {IPC::MakeHeader(0x0010, 0, 0), &NFC_U::GetTagInfo2, "GetTagInfo2"},
        {IPC::MakeHeader(0x0011, 0, 0), &NFC_U::GetTagInfo, "GetTagInfo"},
        {IPC::MakeHeader(0x0012, 0, 0), &NFC_U::CommunicationGetResult, "CommunicationGetResult"},
        {IPC::MakeHeader(0x0013, 1, 0), &NFC_U::OpenAppData, "OpenAppData"},
        {IPC::MakeHeader(0x0014, 14, 4), &NFC_U::InitializeWriteAppData, "InitializeWriteAppData"},
        {IPC::MakeHeader(0x0015, 1, 0), &NFC_U::ReadAppData, "ReadAppData"},
        {IPC::MakeHeader(0x0016, 9, 2), &NFC_U::WriteAppData, "WriteAppData"},
        {IPC::MakeHeader(0x0017, 0, 0), &NFC_U::GetRegisterInfo, "GetRegisterInfo"},
        {IPC::MakeHeader(0x0018, 0, 0), &NFC_U::GetCommonInfo, "GetCommonInfo"},
        {IPC::MakeHeader(0x0019, 0, 0), &NFC_U::GetAppDataInitStruct, "GetAppDataInitStruct"},
        {IPC::MakeHeader(0x001A, 0, 0), &NFC_U::LoadAmiiboPartially, "LoadAmiiboPartially"},
        {IPC::MakeHeader(0x001B, 0, 0), &NFC_U::GetIdentificationBlock, "GetIdentificationBlock"},
        {IPC::MakeHeader(0x001C, 0, 0), nullptr, "Unknown0x1C"},
        {IPC::MakeHeader(0x001D, 0, 0), nullptr, "Unknown0x1D"},
        {IPC::MakeHeader(0x001E, 0, 0), nullptr, "Unknown0x1E"},
        {IPC::MakeHeader(0x001F, 0, 0), nullptr, "Unknown0x1F"},
        {IPC::MakeHeader(0x0020, 0, 0), nullptr, "Unknown0x20"},
        {IPC::MakeHeader(0x0021, 0, 0), nullptr, "Unknown0x21"},
        {IPC::MakeHeader(0x0022, 0, 0), nullptr, "Unknown0x22"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NFC
