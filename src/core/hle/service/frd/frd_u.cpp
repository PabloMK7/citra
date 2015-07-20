// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_u.h"

namespace Service {
namespace FRD {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr,               "HasLoggedIn"},
    {0x00030000, nullptr,               "Login"},
    {0x00040000, nullptr,               "Logout"},
    {0x00050000, nullptr,               "GetFriendKey"},
    {0x00080000, nullptr,               "GetMyPresence"},
    {0x00090000, nullptr,               "GetMyScreenName"},
    {0x00100040, nullptr,               "GetPassword"},
    {0x00110080, nullptr,               "GetFriendKeyList"},
    {0x00190042, nullptr,               "GetFriendFavoriteGame"},
    {0x001A00C4, nullptr,               "GetFriendInfo"},
    {0x001B0080, nullptr,               "IsOnFriendList"},
    {0x001C0042, nullptr,               "DecodeLocalFriendCode"},
    {0x001D0002, nullptr,               "SetCurrentlyPlayingText"},
    {0x00230000, nullptr,               "GetLastResponseResult"},
    {0x00270040, nullptr,               "ResultToErrorCode"},
    {0x00280244, nullptr,               "RequestGameAuthentication"},
    {0x00290000, nullptr,               "GetGameAuthenticationData"},
    {0x002A0204, nullptr,               "RequestServiceLocator"},
    {0x002B0000, nullptr,               "GetServiceLocatorData"},
    {0x00320042, nullptr,               "SetClientSdkVersion"},
};

FRD_U_Interface::FRD_U_Interface() {
    Register(FunctionTable);
}

} // namespace FRD
} // namespace Service
