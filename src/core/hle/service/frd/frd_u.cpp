// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/frd/frd_u.h"

namespace Service::FRD {

FRD_U::FRD_U(std::shared_ptr<Module> frd) : Module::Interface(std::move(frd), "frd:u", 8) {
    static const FunctionInfo functions[] = {
        {0x00010000, nullptr, "HasLoggedIn"},
        {0x00020000, nullptr, "IsOnline"},
        {0x00030000, nullptr, "Login"},
        {0x00040000, nullptr, "Logout"},
        {0x00050000, &FRD_U::GetMyFriendKey, "GetMyFriendKey"},
        {0x00060000, nullptr, "GetMyPreference"},
        {0x00070000, nullptr, "GetMyProfile"},
        {0x00080000, &FRD_U::GetMyPresence, "GetMyPresence"},
        {0x00090000, &FRD_U::GetMyScreenName, "GetMyScreenName"},
        {0x000A0000, nullptr, "GetMyMii"},
        {0x000B0000, nullptr, "GetMyLocalAccountId"},
        {0x000C0000, nullptr, "GetMyPlayingGame"},
        {0x000D0000, nullptr, "GetMyFavoriteGame"},
        {0x000E0000, nullptr, "GetMyNcPrincipalId"},
        {0x000F0000, nullptr, "GetMyComment"},
        {0x00100040, nullptr, "GetMyPassword"},
        {0x00110080, &FRD_U::GetFriendKeyList, "GetFriendKeyList"},
        {0x00120042, nullptr, "GetFriendPresence"},
        {0x00130142, nullptr, "GetFriendScreenName"},
        {0x00140044, nullptr, "GetFriendMii"},
        {0x00150042, &FRD_U::GetFriendProfile, "GetFriendProfile"},
        {0x00160042, nullptr, "GetFriendRelationship"},
        {0x00170042, &FRD_U::GetFriendAttributeFlags, "GetFriendAttributeFlags"},
        {0x00180044, nullptr, "GetFriendPlayingGame"},
        {0x00190042, nullptr, "GetFriendFavoriteGame"},
        {0x001A00C4, nullptr, "GetFriendInfo"},
        {0x001B0080, nullptr, "IsIncludedInFriendList"},
        {0x001C0042, &FRD_U::UnscrambleLocalFriendCode, "UnscrambleLocalFriendCode"},
        {0x001D0002, nullptr, "UpdateGameModeDescription"},
        {0x001E02C2, nullptr, "UpdateGameMode"},
        {0x001F0042, nullptr, "SendInvitation"},
        {0x00200002, nullptr, "AttachToEventNotification"},
        {0x00210040, nullptr, "SetNotificationMask"},
        {0x00220040, nullptr, "GetEventNotification"},
        {0x00230000, nullptr, "GetLastResponseResult"},
        {0x00240040, nullptr, "PrincipalIdToFriendCode"},
        {0x00250080, nullptr, "FriendCodeToPrincipalId"},
        {0x00260080, nullptr, "IsValidFriendCode"},
        {0x00270040, nullptr, "ResultToErrorCode"},
        {0x00280244, nullptr, "RequestGameAuthentication"},
        {0x00290000, nullptr, "GetGameAuthenticationData"},
        {0x002A0204, nullptr, "RequestServiceLocator"},
        {0x002B0000, nullptr, "GetServiceLocatorData"},
        {0x002C0002, nullptr, "DetectNatProperties"},
        {0x002D0000, nullptr, "GetNatProperties"},
        {0x002E0000, nullptr, "GetServerTimeInterval"},
        {0x002F0040, nullptr, "AllowHalfAwake"},
        {0x00300000, nullptr, "GetServerTypes"},
        {0x00310082, nullptr, "GetFriendComment"},
        {0x00320042, &FRD_U::SetClientSdkVersion, "SetClientSdkVersion"},
        {0x00330000, nullptr, "GetMyApproachContext"},
        {0x00340046, nullptr, "AddFriendWithApproach"},
        {0x00350082, nullptr, "DecryptApproachContext"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::FRD
