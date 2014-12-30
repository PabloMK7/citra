// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/frd_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FRD_U

namespace FRD_U {

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

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
