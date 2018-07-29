// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
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
    const auto path_type = static_cast<CecDataPathType>(rp.Pop<u32>());
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// File size?

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, static_cast<u32>(path_type), file_open_flag);
}

void Module::Interface::ReadRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 2);
    const u32 buffer_size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, buffer_size={:#010x}", buffer_size);
}

void Module::Interface::ReadMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto& message_id_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 4);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::ReadMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto& message_id_buffer = rp.PopMappedBuffer();
    const auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 6);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::WriteRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 2);
    const u32 buffer_size = rp.Pop<u32>();
    const auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, buffer_size={:#010x}", buffer_size);
}

void Module::Interface::WriteMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto& read_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::WriteMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const auto& read_buffer = rp.PopMappedBuffer();
    const auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 6);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_out_box={}",
                ncch_program_id, is_out_box);
}

void Module::Interface::Delete(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 4, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const auto path_type = static_cast<CecDataPathType>(rp.Pop<u32>());
    const bool is_out_box = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, is_out_box={}",
                ncch_program_id, static_cast<u32>(path_type), is_out_box);
}

void Module::Interface::GetSystemInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 3, 4);
    const u32 dest_buffer_size = rp.Pop<u32>();
    const u32 info_type = rp.Pop<u32>();
    const u32 param_buffer_size = rp.Pop<u32>();
    const auto& param_buffer = rp.PopMappedBuffer();
    auto& dest_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(param_buffer);
    rb.PushMappedBuffer(dest_buffer);

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
    const auto path_type = static_cast<CecDataPathType>(rp.Pop<u32>());
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();
    auto& read_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode write_mode = {};
    write_mode.create_flag.Assign(1);
    write_mode.write_flag.Assign(1);

    auto file_result = Service::FS::OpenFileFromArchive(cecd->cecd_system_save_data_archive, path,
                                                        write_mode);
    if (file_result.Succeeded()) {
        std::vector<u8> buffer(buffer_size);
        read_buffer.Read(buffer.data(), 0, buffer_size);
        const u32 bytes_written =
            file_result.Unwrap()->backend->Write(0, buffer_size, true, buffer.data()).Unwrap();

        rb.Push(RESULT_SUCCESS);
    } else {
        rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                           ErrorLevel::Status));
    }
    rb.PushMappedBuffer(read_buffer);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, static_cast<u32>(path_type), file_open_flag);
}

void Module::Interface::OpenAndRead(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 4, 4);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const auto path_type = static_cast<CecDataPathType>(rp.Pop<u32>());
    const u32 file_open_flag = rp.Pop<u32>();
    rp.PopPID();
    auto& write_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode read_mode = {};
    read_mode.read_flag.Assign(1);

    auto file_result = Service::FS::OpenFileFromArchive(cecd->cecd_system_save_data_archive, path,
                                                        read_mode);
    if (file_result.Succeeded()) {
        std::vector<u8> buffer(buffer_size);
        const u32 bytes_read =
            file_result.Unwrap()->backend->Read(0, buffer_size, buffer.data()).Unwrap();
        write_buffer.Write(buffer.data(), 0, buffer_size);

        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(bytes_read);
    } else {
        rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                           ErrorLevel::Status));
        rb.Push<u32>(0); /// No bytes read
    }
    rb.PushMappedBuffer(write_buffer);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#010x}, "
                "file_open_flag={:#010x}",
                ncch_program_id, static_cast<u32>(path_type), file_open_flag);
}

std::string Module::GetCecDataPathTypeAsString(const CecDataPathType type, const u32 program_id,
                                               const std::vector<u8>& msg_id) {
    switch(type) {
    case CecDataPathType::CEC_PATH_MBOX_LIST:
        return "/CEC/MBoxList____";
    case CecDataPathType::CEC_PATH_MBOX_INFO:
        return Common::StringFromFormat("/CEC/%08x/MBoxInfo____", program_id);
    case CecDataPathType::CEC_PATH_INBOX_INFO:
        return Common::StringFromFormat("/CEC/%08x/InBox___/BoxInfo_____", program_id);
    case CecDataPathType::CEC_PATH_OUTBOX_INFO:
        return Common::StringFromFormat("/CEC/%08x/OutBox__/BoxInfo_____", program_id);
    case CecDataPathType::CEC_PATH_OUTBOX_INDEX:
        return Common::StringFromFormat("/CEC/%08x/OutBox__/OBIndex_____", program_id);
    case CecDataPathType::CEC_PATH_INBOX_MSG:
        return Common::StringFromFormat("/CEC/%08x/InBox___/_%08x", program_id, msg_id.data());
    case CecDataPathType::CEC_PATH_OUTBOX_MSG:
        return Common::StringFromFormat("/CEC/%08x/OutBox__/_%08x", program_id, msg_id.data());
    case CecDataPathType::CEC_PATH_ROOT_DIR:
        return "/CEC";
    case CecDataPathType::CEC_PATH_MBOX_DIR:
        return Common::StringFromFormat("/CEC/%08x", program_id);
    case CecDataPathType::CEC_PATH_INBOX_DIR:
        return Common::StringFromFormat("/CEC/%08x/InBox___", program_id);
    case CecDataPathType::CEC_PATH_OUTBOX_DIR:
        return Common::StringFromFormat("/CEC/%08x/OutBox__", program_id);
    case CecDataPathType::CEC_MBOX_DATA:
    case CecDataPathType::CEC_MBOX_ICON:
    case CecDataPathType::CEC_MBOX_TITLE:
    default:
        return Common::StringFromFormat("/CEC/%08x/MBoxData.%03d", program_id,
                                        static_cast<u32>(type) - 100);
    }
}

Module::Interface::Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cecd(std::move(cecd)) {}

Module::Module() {
    using namespace Kernel;
    cecinfo_event = Event::Create(Kernel::ResetType::OneShot, "CECD::cecinfo_event");
    change_state_event = Event::Create(Kernel::ResetType::OneShot, "CECD::change_state_event");

    // Open the SystemSaveData archive 0x00010026
    FileSys::Path archive_path(cecd_system_savedata_id);
    auto archive_result =
        Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SystemSaveData,
                                   FileSys::ArchiveFormatInfo(), archive_path);

        // Open it again to get a valid archive now that the folder exists
        archive_result =
            Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);
    }

    ASSERT_MSG(archive_result.Succeeded(), "Could not open the CECD SystemSaveData archive!");

    cecd_system_save_data_archive = *archive_result;
}

Module::~Module() {
    if (cecd_system_save_data_archive) {
        Service::FS::CloseArchive(cecd_system_save_data_archive);
    }
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cecd = std::make_shared<Module>();
    std::make_shared<CECD_NDM>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_S>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_U>(cecd)->InstallAsService(service_manager);
}

} // namespace CECD
} // namespace Service
