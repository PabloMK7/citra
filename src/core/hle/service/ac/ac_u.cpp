// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ac/ac.h"
#include "core/hle/service/ac/ac_u.h"

namespace Service {
namespace AC {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, CreateDefaultConfig, "CreateDefaultConfig"},
    {0x00040006, ConnectAsync, "ConnectAsync"},
    {0x00050002, GetConnectResult, "GetConnectResult"},
    {0x00070002, nullptr, "CancelConnectAsync"},
    {0x00080004, CloseAsync, "CloseAsync"},
    {0x00090002, GetCloseResult, "GetCloseResult"},
    {0x000A0000, nullptr, "GetLastErrorCode"},
    {0x000C0000, nullptr, "GetStatus"},
    {0x000D0000, GetWifiStatus, "GetWifiStatus"},
    {0x000E0042, nullptr, "GetCurrentAPInfo"},
    {0x00100042, nullptr, "GetCurrentNZoneInfo"},
    {0x00110042, nullptr, "GetNZoneApNumService"},
    {0x001D0042, nullptr, "ScanAPs"},
    {0x00240042, nullptr, "AddDenyApType"},
    {0x00270002, GetInfraPriority, "GetInfraPriority"},
    {0x002D0082, SetRequestEulaVersion, "SetRequestEulaVersion"},
    {0x00300004, RegisterDisconnectEvent, "RegisterDisconnectEvent"},
    {0x003C0042, nullptr, "GetAPSSIDList"},
    {0x003E0042, IsConnected, "IsConnected"},
    {0x00400042, SetClientVersion, "SetClientVersion"},
};

AC_U::AC_U() {
    Register(FunctionTable);
}

} // namespace AC
} // namespace Service
