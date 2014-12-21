// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/soc_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SOC_U

namespace SOC_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, nullptr,                       "InitializeSockets"},
    {0x000200C2, nullptr,                       "socket"},
    {0x00030082, nullptr,                       "listen"},
    {0x00040082, nullptr,                       "accept"},
    {0x00050084, nullptr,                       "bind"},
    {0x00060084, nullptr,                       "connect"},
    {0x00070104, nullptr,                       "recvfrom_other"},
    {0x00080102, nullptr,                       "recvfrom"},
    {0x00090106, nullptr,                       "sendto_other"},
    {0x000A0106, nullptr,                       "sendto"},
    {0x000B0042, nullptr,                       "close"},
    {0x000C0082, nullptr,                       "shutdown"},
    {0x000D0082, nullptr,                       "gethostbyname"},
    {0x000E00C2, nullptr,                       "gethostbyaddr"},
    {0x000F0106, nullptr,                       "unknown_resolve_ip"},
    {0x00110102, nullptr,                       "getsockopt"},
    {0x00120104, nullptr,                       "setsockopt"},
    {0x001300C2, nullptr,                       "fcntl"},
    {0x00140084, nullptr,                       "poll"},
    {0x00150042, nullptr,                       "sockatmark"},
    {0x00160000, nullptr,                       "gethostid"},
    {0x00170082, nullptr,                       "getsockname"},
    {0x00180082, nullptr,                       "getpeername"},
    {0x00190000, nullptr,                       "ShutdownSockets"},
    {0x001A00C0, nullptr,                       "GetNetworkOpt"},
    {0x001B0040, nullptr,                       "ICMPSocket"},
    {0x001C0104, nullptr,                       "ICMPPing"},
    {0x001D0040, nullptr,                       "ICMPCancel"},
    {0x001E0040, nullptr,                       "ICMPClose"},
    {0x001F0040, nullptr,                       "GetResolverInfo"},
    {0x00210002, nullptr,                       "CloseSockets"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
