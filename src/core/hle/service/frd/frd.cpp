// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace FRD {

static FriendKey my_friend_key = {0, 0, 0ull};
static MyPresence my_presence = {};

void GetMyPresence(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 shifted_out_size = cmd_buff[64];
    u32 my_presence_addr = cmd_buff[65];

    ASSERT(shifted_out_size == ((sizeof(MyPresence) << 14) | 2));

    Memory::WriteBlock(my_presence_addr, &my_presence, sizeof(MyPresence));

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetFriendKeyList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unknown = cmd_buff[1];
    u32 frd_count = cmd_buff[2];
    u32 frd_key_addr = cmd_buff[65];

    FriendKey zero_key = {};
    for (u32 i = 0; i < frd_count; ++i) {
        Memory::WriteBlock(frd_key_addr + i * sizeof(FriendKey), &zero_key, sizeof(FriendKey));
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // 0 friends
    LOG_WARNING(Service_FRD, "(STUBBED) called, unknown=%d, frd_count=%d, frd_key_addr=0x%08X",
                unknown, frd_count, frd_key_addr);
}

void GetFriendProfile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 count = cmd_buff[1];
    u32 frd_key_addr = cmd_buff[3];
    u32 profiles_addr = cmd_buff[65];

    Profile zero_profile = {};
    for (u32 i = 0; i < count; ++i) {
        Memory::WriteBlock(profiles_addr + i * sizeof(Profile), &zero_profile, sizeof(Profile));
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_FRD,
                "(STUBBED) called, count=%d, frd_key_addr=0x%08X, profiles_addr=0x%08X", count,
                frd_key_addr, profiles_addr);
}

void GetFriendAttributeFlags(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 count = cmd_buff[1];
    u32 frd_key_addr = cmd_buff[3];
    u32 attr_flags_addr = cmd_buff[65];

    for (u32 i = 0; i < count; ++i) {
        // TODO:(mailwl) figure out AttributeFlag size and zero all buffer. Assume 1 byte
        Memory::Write8(attr_flags_addr + i, 0);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_FRD,
                "(STUBBED) called, count=%d, frd_key_addr=0x%08X, attr_flags_addr=0x%08X", count,
                frd_key_addr, attr_flags_addr);
}

void GetMyFriendKey(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    std::memcpy(&cmd_buff[2], &my_friend_key, sizeof(FriendKey));
    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void GetMyScreenName(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    // TODO: (mailwl) get the name from config
    Common::UTF8ToUTF16("Citra").copy(reinterpret_cast<char16_t*>(&cmd_buff[2]), 11);
    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new FRD_A_Interface);
    AddService(new FRD_U_Interface);
}

void Shutdown() {}

} // namespace FRD

} // namespace Service
