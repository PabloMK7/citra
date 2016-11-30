// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace FRD {

struct FriendKey {
    u32 friend_id;
    u32 unknown;
    u64 friend_code;
};

struct MyPresence {
    u8 unknown[0x12C];
};

struct Profile {
    u8 region;
    u8 country;
    u8 area;
    u8 language;
    u32 unknown;
};

/**
 * FRD::GetMyPresence service function
 *  Inputs:
 *      64 : sizeof (MyPresence) << 14 | 2
 *      65 : Address of MyPresence structure
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetMyPresence(Service::Interface* self);

/**
 * FRD::GetFriendKeyList service function
 *  Inputs:
 *      1 : Unknown
 *      2 : Max friends count
 *      65 : Address of FriendKey List
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : FriendKey count filled
 */
void GetFriendKeyList(Service::Interface* self);

/**
 * FRD::GetFriendProfile service function
 *  Inputs:
 *      1 : Friends count
 *      2 : Friends count << 18 | 2
 *      3 : Address of FriendKey List
 *      64 : (count * sizeof (Profile)) << 10 | 2
 *      65 : Address of Profiles List
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetFriendProfile(Service::Interface* self);

/**
 * FRD::GetFriendAttributeFlags service function
 *  Inputs:
 *      1 : Friends count
 *      2 : Friends count << 18 | 2
 *      3 : Address of FriendKey List
 *      65 : Address of AttributeFlags
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetFriendAttributeFlags(Service::Interface* self);

/**
 * FRD::GetMyFriendKey service function
 *  Inputs:
 *      none
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-5 : FriendKey
 */
void GetMyFriendKey(Service::Interface* self);

/**
 * FRD::GetMyScreenName service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : UTF16 encoded name (max 11 symbols)
 */
void GetMyScreenName(Service::Interface* self);

/**
 * FRD::SetClientSdkVersion service function
 *  Inputs:
 *      1 : Used SDK Version
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetClientSdkVersion(Service::Interface* self);

/// Initialize FRD service(s)
void Init();

/// Shutdown FRD service(s)
void Shutdown();

} // namespace FRD
} // namespace Service
