// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"

namespace Service {
namespace FRD {

Module::Interface::Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), frd(std::move(frd)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetMyPresence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    std::vector<u8> buffer(sizeof(MyPresence));
    std::memcpy(buffer.data(), &frd->my_presence, buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetFriendKeyList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 2, 0);
    u32 unknown = rp.Pop<u32>();
    u32 frd_count = rp.Pop<u32>();

    std::vector<u8> buffer(sizeof(FriendKey) * frd_count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // 0 friends
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, unknown={}, frd_count={}", unknown, frd_count);
}

void Module::Interface::GetFriendProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 1, 2);
    u32 count = rp.Pop<u32>();
    std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    std::vector<u8> buffer(sizeof(Profile) * count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetFriendAttributeFlags(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 1, 2);
    u32 count = rp.Pop<u32>();
    std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    // TODO:(mailwl) figure out AttributeFlag size and zero all buffer. Assume 1 byte
    std::vector<u8> buffer(1 * count, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetMyFriendKey(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(frd->my_friend_key);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyScreenName(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);

    struct ScreenName {
        std::array<char16_t, 12> name;
    };

    // TODO: (mailwl) get the name from config
    ScreenName screen_name{u"Citra"};

    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(screen_name);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx) {
    const std::size_t scrambled_friend_code_size = 12;
    const std::size_t friend_code_size = 8;

    IPC::RequestParser rp(ctx, 0x1C, 1, 2);
    const u32 friend_code_count = rp.Pop<u32>();
    std::vector<u8> scrambled_friend_codes = rp.PopStaticBuffer();
    ASSERT_MSG(scrambled_friend_codes.size() == (friend_code_count * scrambled_friend_code_size),
               "Wrong input buffer size");

    std::vector<u8> unscrambled_friend_codes(friend_code_count * friend_code_size, 0);
    // TODO(B3N30): Unscramble the codes and compare them against the friend list
    //              Only write 0 if the code isn't in friend list, otherwise write the
    //              unscrambled one
    //
    // Code for unscrambling (should be compared to HW):
    // std::array<u16, 6> scambled_friend_code;
    // Memory::ReadBlock(scrambled_friend_codes+(current*scrambled_friend_code_size),
    // scambled_friend_code.data(), scrambled_friend_code_size); std::array<u16, 4>
    // unscrambled_friend_code; unscrambled_friend_code[0] = scambled_friend_code[0] ^
    // scambled_friend_code[5]; unscrambled_friend_code[1] = scambled_friend_code[1] ^
    // scambled_friend_code[5]; unscrambled_friend_code[2] = scambled_friend_code[2] ^
    // scambled_friend_code[5]; unscrambled_friend_code[3] = scambled_friend_code[3] ^
    // scambled_friend_code[5];

    LOG_WARNING(Service_FRD, "(STUBBED) called");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(unscrambled_friend_codes, 0);
}

void Module::Interface::SetClientSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x32, 1, 2);
    u32 version = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_FRD, "(STUBBED) called, version: 0x{:08X}", version);
}

Module::Module() = default;
Module::~Module() = default;

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto frd = std::make_shared<Module>();
    std::make_shared<FRD_U>(frd)->InstallAsService(service_manager);
    std::make_shared<FRD_A>(frd)->InstallAsService(service_manager);
}

} // namespace FRD

} // namespace Service
