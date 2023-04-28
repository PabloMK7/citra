// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ac/ac_u.h"

namespace Service::AC {

AC_U::AC_U(std::shared_ptr<Module> ac) : Module::Interface(std::move(ac), "ac:u", 10) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 0), &AC_U::CreateDefaultConfig, "CreateDefaultConfig"},
        {IPC::MakeHeader(0x0004, 0, 6), &AC_U::ConnectAsync, "ConnectAsync"},
        {IPC::MakeHeader(0x0005, 0, 2), &AC_U::GetConnectResult, "GetConnectResult"},
        {IPC::MakeHeader(0x0007, 0, 2), nullptr, "CancelConnectAsync"},
        {IPC::MakeHeader(0x0008, 0, 4), &AC_U::CloseAsync, "CloseAsync"},
        {IPC::MakeHeader(0x0009, 0, 2), &AC_U::GetCloseResult, "GetCloseResult"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetLastErrorCode"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "GetStatus"},
        {IPC::MakeHeader(0x000D, 0, 0), &AC_U::GetWifiStatus, "GetWifiStatus"},
        {IPC::MakeHeader(0x000E, 1, 2), nullptr, "GetCurrentAPInfo"},
        {IPC::MakeHeader(0x0010, 1, 2), nullptr, "GetCurrentNZoneInfo"},
        {IPC::MakeHeader(0x0011, 1, 2), nullptr, "GetNZoneApNumService"},
        {IPC::MakeHeader(0x001D, 1, 2), nullptr, "ScanAPs"},
        {IPC::MakeHeader(0x0024, 1, 2), nullptr, "AddDenyApType"},
        {IPC::MakeHeader(0x0027, 0, 2), &AC_U::GetInfraPriority, "GetInfraPriority"},
        {IPC::MakeHeader(0x002D, 2, 2), &AC_U::SetRequestEulaVersion, "SetRequestEulaVersion"},
        {IPC::MakeHeader(0x0030, 0, 4), &AC_U::RegisterDisconnectEvent, "RegisterDisconnectEvent"},
        {IPC::MakeHeader(0x003C, 1, 2), nullptr, "GetAPSSIDList"},
        {IPC::MakeHeader(0x003E, 1, 2), &AC_U::IsConnected, "IsConnected"},
        {IPC::MakeHeader(0x0040, 1, 2), &AC_U::SetClientVersion, "SetClientVersion"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AC

SERIALIZE_EXPORT_IMPL(Service::AC::AC_U)
