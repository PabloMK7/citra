// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ac/ac_i.h"

namespace Service::AC {

AC_I::AC_I(std::shared_ptr<Module> ac) : Module::Interface(std::move(ac), "ac:i", 10) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 0), &AC_I::CreateDefaultConfig, "CreateDefaultConfig"},
        {IPC::MakeHeader(0x0004, 0, 6), &AC_I::ConnectAsync, "ConnectAsync"},
        {IPC::MakeHeader(0x0005, 0, 2), &AC_I::GetConnectResult, "GetConnectResult"},
        {IPC::MakeHeader(0x0007, 0, 2), nullptr, "CancelConnectAsync"},
        {IPC::MakeHeader(0x0008, 0, 4), &AC_I::CloseAsync, "CloseAsync"},
        {IPC::MakeHeader(0x0009, 0, 2), &AC_I::GetCloseResult, "GetCloseResult"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetLastErrorCode"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "GetStatus"},
        {IPC::MakeHeader(0x000D, 0, 0), &AC_I::GetWifiStatus, "GetWifiStatus"},
        {IPC::MakeHeader(0x000E, 1, 2), nullptr, "GetCurrentAPInfo"},
        {IPC::MakeHeader(0x0010, 1, 2), nullptr, "GetCurrentNZoneInfo"},
        {IPC::MakeHeader(0x0011, 1, 2), nullptr, "GetNZoneApNumService"},
        {IPC::MakeHeader(0x001D, 1, 2), nullptr, "ScanAPs"},
        {IPC::MakeHeader(0x0024, 1, 2), nullptr, "AddDenyApType"},
        {IPC::MakeHeader(0x0027, 0, 2), &AC_I::GetInfraPriority, "GetInfraPriority"},
        {IPC::MakeHeader(0x002D, 2, 2), &AC_I::SetRequestEulaVersion, "SetRequestEulaVersion"},
        {IPC::MakeHeader(0x0030, 0, 4), &AC_I::RegisterDisconnectEvent, "RegisterDisconnectEvent"},
        {IPC::MakeHeader(0x003C, 1, 2), nullptr, "GetAPSSIDList"},
        {IPC::MakeHeader(0x003E, 1, 2), &AC_I::IsConnected, "IsConnected"},
        {IPC::MakeHeader(0x0040, 1, 2), &AC_I::SetClientVersion, "SetClientVersion"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AC

SERIALIZE_EXPORT_IMPL(Service::AC::AC_I)
