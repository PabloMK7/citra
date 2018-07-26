// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_ndm.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

void Module::Interface::OpenRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 3, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 path_type = rp.Pop<u32>();
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// File size?

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, path_type, file_open_flag);
}

void Module::Interface::ReadRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 2);
    const u32 buffer_size = rp.Pop<u32>();
    auto buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_CECD, "(STUBBED) called, buffer_size={:#010x}", buffer_size);
}

void Module::Interface::ReadMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto message_id_buffer = rp.PopStaticBuffer();
    auto write_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 4);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushStaticBuffer(message_id_buffer, 0);
    rb.PushStaticBuffer(write_buffer, 1);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::ReadMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto message_id_buffer = rp.PopStaticBuffer();
    const auto hmac_key_buffer = rp.PopStaticBuffer();
    auto write_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 6);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushStaticBuffer(message_id_buffer, 0);
    rb.PushStaticBuffer(hmac_key_buffer, 1);
    rb.PushStaticBuffer(write_buffer, 2);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::WriteRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 2);
    const u32 buffer_size = rp.Pop<u32>();
    const auto buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(buffer, 0);

    LOG_WARNING(Service_CECD, "(STUBBED) called, buffer_size={:#010x}", buffer_size);
}

void Module::Interface::WriteMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto read_buffer = rp.PopStaticBuffer();
    auto message_id_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(read_buffer, 0);
    rb.PushStaticBuffer(message_id_buffer, 1);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::WriteMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto read_buffer = rp.PopStaticBuffer();
    const auto hmac_key_buffer = rp.PopStaticBuffer();
    auto message_id_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 6);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(read_buffer, 0);
    rb.PushStaticBuffer(hmac_key_buffer, 1);
    rb.PushStaticBuffer(message_id_buffer, 2);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::Delete(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 4, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 path_type = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const auto message_id_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(message_id_buffer, 0);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, is_out_box={}",
                ncch_program_id, path_type, is_out_box);
}

void Module::Interface::GetSystemInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 3, 4);
    const u32 dest_buffer_size = rp.Pop<u32>();
    const u32 info_type = rp.Pop<u32>();
    const u32 param_buffer_size = rp.Pop<u32>();
    const auto param_buffer = rp.PopStaticBuffer();
    auto dest_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(param_buffer, 0);
    rb.PushStaticBuffer(dest_buffer, 1);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, dest_buffer_size={:#010x}, info_type={:#010x}, "
                "param_buffer_size={:#010x}",
                dest_buffer_size, info_type, param_buffer_size);
}

void Module::Interface::GetCecStateAbbreviated(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(CecStateAbbreviated::CEC_STATE_ABBREV_IDLE);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetCecInfoEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(cecd->cecinfo_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetChangeStateEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(cecd->change_state_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::OpenAndWrite(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 4, 4);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 path_type = rp.Pop<u32>();
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();
    const auto read_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(read_buffer, 0);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, path_type, file_open_flag);
}

void Module::Interface::OpenAndRead(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 4, 4);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 path_type = rp.Pop<u32>();
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();
    auto write_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(write_buffer, 0);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, path_type, file_open_flag);
}

Module::Interface::Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cecd(std::move(cecd)) {}

Module::Module() {
    using namespace Kernel;
    cecinfo_event = Event::Create(Kernel::ResetType::OneShot, "CECD::cecinfo_event");
    change_state_event = Event::Create(Kernel::ResetType::OneShot, "CECD::change_state_event");
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cecd = std::make_shared<Module>();
    std::make_shared<CECD_NDM>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_S>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_U>(cecd)->InstallAsService(service_manager);
}

} // namespace CECD
} // namespace Service
