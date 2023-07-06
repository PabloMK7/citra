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
        {IPC::MakeHeader(0x0002, 1, 0), &NFC_U::Finalize, "Finalize"},
        {IPC::MakeHeader(0x0003, 0, 0), &NFC_U::Connect, "Connect"},
        {IPC::MakeHeader(0x0004, 0, 0), &NFC_U::Disconnect, "Disconnect"},
        {IPC::MakeHeader(0x0005, 1, 0), &NFC_U::StartDetection, "StartDetection"},
        {IPC::MakeHeader(0x0006, 0, 0), &NFC_U::StopDetection, "StopDetection"},
        {IPC::MakeHeader(0x0007, 0, 0), &NFC_U::Mount, "Mount"},
        {IPC::MakeHeader(0x0008, 0, 0), &NFC_U::Unmount, "Unmount"},
        {IPC::MakeHeader(0x0009, 0, 2), &NFC_U::Flush, "Flush"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "Restore"},
        {IPC::MakeHeader(0x000B, 0, 0), &NFC_U::GetActivateEvent, "GetActivateEvent"},
        {IPC::MakeHeader(0x000C, 0, 0), &NFC_U::GetDeactivateEvent, "GetDeactivateEvent"},
        {IPC::MakeHeader(0x000D, 0, 0), &NFC_U::GetStatus, "GetStatus"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "Unknown0x0E"},
        {IPC::MakeHeader(0x000F, 0, 0), &NFC_U::GetTargetConnectionStatus, "GetTargetConnectionStatus"},
        {IPC::MakeHeader(0x0010, 0, 0), &NFC_U::GetTagInfo2, "GetTagInfo2"},
        {IPC::MakeHeader(0x0011, 0, 0), &NFC_U::GetTagInfo, "GetTagInfo"},
        {IPC::MakeHeader(0x0012, 0, 0), &NFC_U::GetConnectResult, "GetConnectResult"},
        {IPC::MakeHeader(0x0013, 1, 0), &NFC_U::OpenApplicationArea, "OpenApplicationArea"},
        {IPC::MakeHeader(0x0014, 14, 4), &NFC_U::CreateApplicationArea, "CreateApplicationArea"},
        {IPC::MakeHeader(0x0015, 1, 0), &NFC_U::ReadApplicationArea, "ReadApplicationArea"},
        {IPC::MakeHeader(0x0016, 9, 2), &NFC_U::WriteApplicationArea, "WriteApplicationArea"},
        {IPC::MakeHeader(0x0017, 0, 0), &NFC_U::GetNfpRegisterInfo, "GetNfpRegisterInfo"},
        {IPC::MakeHeader(0x0018, 0, 0), &NFC_U::GetNfpCommonInfo, "GetNfpCommonInfo"},
        {IPC::MakeHeader(0x0019, 0, 0), &NFC_U::InitializeCreateInfo, "InitializeCreateInfo"},
        {IPC::MakeHeader(0x001A, 0, 0), &NFC_U::MountRom, "MountRom"},
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
