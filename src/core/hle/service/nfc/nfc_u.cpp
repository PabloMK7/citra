// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service {
namespace NFC {

const Interface::FunctionInfo FunctionTable[] = {
    // clang-format off
    {0x00010040, Initialize, "Initialize"},
    {0x00020040, Shutdown, "Shutdown"},
    {0x00030000, StartCommunication, "StartCommunication"},
    {0x00040000, StopCommunication, "StopCommunication"},
    {0x00050040, StartTagScanning, "StartTagScanning"},
    {0x00060000, StopTagScanning, "StopTagScanning"},
    {0x00070000, LoadAmiiboData, "LoadAmiiboData"},
    {0x00080000, ResetTagScanState, "ResetTagScanState"},
    {0x00090002, nullptr, "UpdateStoredAmiiboData"},
    {0x000B0000, GetTagInRangeEvent, "GetTagInRangeEvent"},
    {0x000C0000, GetTagOutOfRangeEvent, "GetTagOutOfRangeEvent"},
    {0x000D0000, GetTagState, "GetTagState"},
    {0x000F0000, CommunicationGetStatus, "CommunicationGetStatus"},
    {0x00100000, nullptr, "GetTagInfo2"},
    {0x00110000, nullptr, "GetTagInfo"},
    {0x00120000, nullptr, "CommunicationGetResult"},
    {0x00130040, nullptr, "OpenAppData"},
    {0x00140384, nullptr, "InitializeWriteAppData"},
    {0x00150040, nullptr, "ReadAppData"},
    {0x00160242, nullptr, "WriteAppData"},
    {0x00170000, nullptr, "GetAmiiboSettings"},
    {0x00180000, nullptr, "GetAmiiboConfig"},
    {0x00190000, nullptr, "GetAppDataInitStruct"},
    // clang-format on
};

NFC_U::NFC_U() {
    Register(FunctionTable);
}

} // namespace NFC
} // namespace Service
