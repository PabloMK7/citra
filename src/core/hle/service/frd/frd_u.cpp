// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_u.h"

namespace Service {
namespace FRD {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00050000, nullptr,               "GetFriendKey"},
    {0x00080000, nullptr,               "GetMyPresence"},
    {0x00100040, nullptr,               "GetPassword"},
    {0x00190042, nullptr,               "GetFriendFavoriteGame"},
    {0x001A00C4, nullptr,               "GetFriendInfo"},
    {0x001B0080, nullptr,               "IsOnFriendList"},
    {0x001C0042, nullptr,               "DecodeLocalFriendCode"},
    {0x001D0002, nullptr,               "SetCurrentlyPlayingText"},
    {0x00320042, nullptr,               "SetClientSdkVersion"}
};

FRD_U_Interface::FRD_U_Interface() {
    Register(FunctionTable);
}

} // namespace FRD
} // namespace Service
