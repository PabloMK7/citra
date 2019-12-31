// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nfc/nfc_m.h"

SERIALIZE_EXPORT_IMPL(Service::NFC::NFC_M)

namespace Service::NFC {

NFC_M::NFC_M(std::shared_ptr<Module> nfc) : Module::Interface(std::move(nfc), "nfc:m", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        // nfc:u shared commands
        {0x00010040, &NFC_M::Initialize, "Initialize"},
        {0x00020040, &NFC_M::Shutdown, "Shutdown"},
        {0x00030000, &NFC_M::StartCommunication, "StartCommunication"},
        {0x00040000, &NFC_M::StopCommunication, "StopCommunication"},
        {0x00050040, &NFC_M::StartTagScanning, "StartTagScanning"},
        {0x00060000, &NFC_M::StopTagScanning, "StopTagScanning"},
        {0x00070000, &NFC_M::LoadAmiiboData, "LoadAmiiboData"},
        {0x00080000, &NFC_M::ResetTagScanState, "ResetTagScanState"},
        {0x00090002, nullptr, "UpdateStoredAmiiboData"},
        {0x000B0000, &NFC_M::GetTagInRangeEvent, "GetTagInRangeEvent"},
        {0x000C0000, &NFC_M::GetTagOutOfRangeEvent, "GetTagOutOfRangeEvent"},
        {0x000D0000, &NFC_M::GetTagState, "GetTagState"},
        {0x000F0000, &NFC_M::CommunicationGetStatus, "CommunicationGetStatus"},
        {0x00100000, nullptr, "GetTagInfo2"},
        {0x00110000, &NFC_M::GetTagInfo, "GetTagInfo"},
        {0x00120000, nullptr, "CommunicationGetResult"},
        {0x00130040, nullptr, "OpenAppData"},
        {0x00140384, nullptr, "InitializeWriteAppData"},
        {0x00150040, nullptr, "ReadAppData"},
        {0x00160242, nullptr, "WriteAppData"},
        {0x00170000, nullptr, "GetAmiiboSettings"},
        {0x00180000, &NFC_M::GetAmiiboConfig, "GetAmiiboConfig"},
        {0x00190000, nullptr, "GetAppDataInitStruct"},
        {0x001A0000, &NFC_M::Unknown0x1A, "Unknown0x1A"},
        {0x001B0000, &NFC_M::GetIdentificationBlock, "GetIdentificationBlock"},
        // nfc:m
        {0x04040A40, nullptr, "SetAmiiboSettings"}
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NFC
