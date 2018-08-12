// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_ndm.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

using CecDataPathType = Module::CecDataPathType;
using CecOpenMode = Module::CecOpenMode;
using CecSystemInfoType = Module::CecSystemInfoType;

void Module::Interface::OpenRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 3, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.read_flag.Assign(1);
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->ncch_program_id = ncch_program_id;
    session_data->open_mode.raw = open_mode.raw;
    session_data->data_path_type = path_type;
    session_data->path = path;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    switch (path_type) {
    case CecDataPathType::CEC_PATH_ROOT_DIR:
    case CecDataPathType::CEC_PATH_MBOX_DIR:
    case CecDataPathType::CEC_PATH_INBOX_DIR:
    case CecDataPathType::CEC_PATH_OUTBOX_DIR: {
        auto dir_result =
            Service::FS::OpenDirectoryFromArchive(cecd->cecd_system_save_data_archive, path);
        if (dir_result.Failed()) {
            if (open_mode.make_dir) {
                Service::FS::CreateDirectoryFromArchive(cecd->cecd_system_save_data_archive, path);
                rb.Push(RESULT_SUCCESS);
            } else {
                LOG_ERROR(Service_CECD, "Failed to open directory: {}", path.AsString());
                rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC,
                                   ErrorSummary::NotFound, ErrorLevel::Status));
            }
            rb.Push<u32>(0); /// Zero entries
        } else {
            auto directory = dir_result.Unwrap();
            rb.Push(RESULT_SUCCESS);
            rb.Push<u32>(directory->backend->Read(0, nullptr)); /// Entry count
        }
        break;
    }
    default: { /// If not directory, then it is a file
        auto file_result =
            Service::FS::OpenFileFromArchive(cecd->cecd_system_save_data_archive, path, mode);
        if (file_result.Failed()) {
            LOG_ERROR(Service_CECD, "Failed to open file: {}", path.AsString());
            rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                               ErrorLevel::Status));
            rb.Push<u32>(0); /// No file size
        } else {
            session_data->file = std::move(file_result.Unwrap());
            rb.Push(RESULT_SUCCESS);
            rb.Push<u32>(session_data->file->backend->GetSize()); /// Return file size
        }

        if (path_type == CecDataPathType::CEC_MBOX_PROGRAM_ID) {
            std::vector<u8> program_id(8);
            u64_le le_program_id = Kernel::g_current_process->codeset->program_id;
            std::memcpy(program_id.data(), &le_program_id, sizeof(u64));
            session_data->file->backend->Write(0, sizeof(u64), true, program_id.data());
        }
    }
    }

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, open_mode={:#010x}, path={}",
              ncch_program_id, static_cast<u32>(path_type), open_mode.raw, path.AsString());
}

void Module::Interface::ReadRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 2);
    const u32 write_buffer_size = rp.Pop<u32>();
    auto& write_buffer = rp.PopMappedBuffer();

    SessionData* session_data = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    switch (session_data->data_path_type) {
    case CecDataPathType::CEC_PATH_ROOT_DIR:
    case CecDataPathType::CEC_PATH_MBOX_DIR:
    case CecDataPathType::CEC_PATH_INBOX_DIR:
    case CecDataPathType::CEC_PATH_OUTBOX_DIR:
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::CEC,
                           ErrorSummary::NotFound, ErrorLevel::Status));
        rb.Push<u32>(0); /// No bytes read
        break;
    default: /// If not directory, then it is a file
        std::vector<u8> buffer(write_buffer_size);
        const u32 bytes_read =
            session_data->file->backend->Read(0, write_buffer_size, buffer.data()).Unwrap();

        write_buffer.Write(buffer.data(), 0, write_buffer_size);
        session_data->file->backend->Close();

        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(bytes_read);
    }
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(Service_CECD, "called, write_buffer_size={:#010x}, path={}", write_buffer_size,
              session_data->path.AsString());
}

void Module::Interface::ReadMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 4);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_outbox={}",
                ncch_program_id, is_outbox);
}

void Module::Interface::ReadMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();
    auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 6);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// Read size
    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_outbox={}",
                ncch_program_id, is_outbox);
}

void Module::Interface::WriteRawFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 2);
    const u32 read_buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();

    SessionData* session_data = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    switch (session_data->data_path_type) {
    case CecDataPathType::CEC_PATH_ROOT_DIR:
    case CecDataPathType::CEC_PATH_MBOX_DIR:
    case CecDataPathType::CEC_PATH_INBOX_DIR:
    case CecDataPathType::CEC_PATH_OUTBOX_DIR:
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::CEC,
                           ErrorSummary::NotFound, ErrorLevel::Status));
        break;
    default: /// If not directory, then it is a file
        std::vector<u8> buffer(read_buffer_size);
        read_buffer.Read(buffer.data(), 0, read_buffer_size);

        const u32 bytes_written =
            session_data->file->backend->Write(0, read_buffer_size, true, buffer.data()).Unwrap();
        session_data->file->backend->Close();

        rb.Push(RESULT_SUCCESS);
    }
    rb.PushMappedBuffer(read_buffer);

    LOG_DEBUG(Service_CECD, "called, read_buffer_size={:#010x}", read_buffer_size);
}

void Module::Interface::WriteMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 4, 4);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_outbox={}",
                ncch_program_id, is_outbox);
}

void Module::Interface::WriteMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 4, 6);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();
    auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 6);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD, "(STUBBED) called, ncch_program_id={:#010x}, is_outbox={}",
                ncch_program_id, is_outbox);
}

void Module::Interface::Delete(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 4, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, path_type={:#04x}, is_outbox={}",
                ncch_program_id, static_cast<u32>(path_type), is_outbox);
}

void Module::Interface::Cecd_0x000900C2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 3, 2);
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    const u32 option = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_WARNING(Service_CECD,
                "(STUBBED) called, ncch_program_id={:#010x}, size={:#010x}, option={:#010x}",
                ncch_program_id, size, option);
}

void Module::Interface::GetSystemInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0A, 3, 4);
    const u32 dest_buffer_size = rp.Pop<u32>();
    const CecSystemInfoType info_type = rp.PopEnum<CecSystemInfoType>();
    const u32 param_buffer_size = rp.Pop<u32>();
    auto& param_buffer = rp.PopMappedBuffer();
    auto& dest_buffer = rp.PopMappedBuffer();

    /// TODO: Other CecSystemInfoTypes
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    std::vector<u8> buffer;
    switch (info_type) {
    case CecSystemInfoType::EulaVersion: /// TODO: Read config Eula version
        buffer = {0xFF, 0xFF};
        dest_buffer.Write(buffer.data(), 0, buffer.size());
        break;
    case CecSystemInfoType::Eula:
        buffer = {0x01}; /// Eula agreed
        dest_buffer.Write(buffer.data(), 0, buffer.size());
        break;
    case CecSystemInfoType::ParentControl:
        buffer = {0x00}; /// No parent control
        dest_buffer.Write(buffer.data(), 0, buffer.size());
        break;
    default:
        LOG_ERROR(Service_CECD, "Unknown system info type={}", static_cast<u32>(info_type));
    }

    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(param_buffer);
    rb.PushMappedBuffer(dest_buffer);

    LOG_DEBUG(Service_CECD,
              "(STUBBED) called, dest_buffer_size={:#010x}, info_type={:#010x}, "
              "param_buffer_size={:#010x}",
              dest_buffer_size, static_cast<u32>(info_type), param_buffer_size);
}

void Module::Interface::RunCommand(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 1, 0);
    const CecCommand command = rp.PopEnum<CecCommand>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CECD, "(STUBBED) called, command={}", static_cast<u32>(command));
}

void Module::Interface::RunCommandAlt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 1, 0);
    const CecCommand command = rp.PopEnum<CecCommand>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CECD, "(STUBBED) called, command={}", static_cast<u32>(command));
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
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();
    auto& read_buffer = rp.PopMappedBuffer();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    switch (path_type) {
    case CecDataPathType::CEC_PATH_ROOT_DIR:
    case CecDataPathType::CEC_PATH_MBOX_DIR:
    case CecDataPathType::CEC_PATH_INBOX_DIR:
    case CecDataPathType::CEC_PATH_OUTBOX_DIR:
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::CEC,
                           ErrorSummary::NotFound, ErrorLevel::Status));
        break;
    default: /// If not directory, then it is a file
        auto file_result =
            Service::FS::OpenFileFromArchive(cecd->cecd_system_save_data_archive, path, mode);
        if (file_result.Succeeded()) {
            auto file = file_result.Unwrap();
            std::vector<u8> buffer(buffer_size);

            read_buffer.Read(buffer.data(), 0, buffer_size);
            const u32 bytes_written =
                file->backend->Write(0, buffer_size, true, buffer.data()).Unwrap();
            file->backend->Close();

            rb.Push(RESULT_SUCCESS);
        } else {
            LOG_ERROR(Service_CECD, "Failed to open file: {}", path.AsString());
            rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                               ErrorLevel::Status));
        }
    }
    rb.PushMappedBuffer(read_buffer);

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, open_mode={:#010x}, path={}",
              ncch_program_id, static_cast<u32>(path_type), open_mode.raw, path.AsString());
}

void Module::Interface::OpenAndRead(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 4, 4);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();
    auto& write_buffer = rp.PopMappedBuffer();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.read_flag.Assign(1);
    mode.create_flag.Assign(1);
    mode.write_flag.Assign(1);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    switch (path_type) {
    case CecDataPathType::CEC_PATH_ROOT_DIR:
    case CecDataPathType::CEC_PATH_MBOX_DIR:
    case CecDataPathType::CEC_PATH_INBOX_DIR:
    case CecDataPathType::CEC_PATH_OUTBOX_DIR:
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::CEC,
                           ErrorSummary::NotFound, ErrorLevel::Status));
        rb.Push<u32>(0); /// No bytes read
        break;
    default: /// If not directory, then it is a file
        auto file_result =
            Service::FS::OpenFileFromArchive(cecd->cecd_system_save_data_archive, path, mode);
        if (file_result.Succeeded()) {
            auto file = file_result.Unwrap();
            std::vector<u8> buffer(buffer_size);

            const u32 bytes_read = file->backend->Read(0, buffer_size, buffer.data()).Unwrap();
            write_buffer.Write(buffer.data(), 0, buffer_size);
            file->backend->Close();

            rb.Push(RESULT_SUCCESS);
            rb.Push<u32>(bytes_read);
        } else {
            LOG_ERROR(Service_CECD, "Failed to open file: {}", path.AsString());

            if (path_type == CecDataPathType::CEC_PATH_MBOX_INFO) {
                const FileSys::Path root_dir_path(
                    cecd->GetCecDataPathTypeAsString(CecDataPathType::CEC_PATH_ROOT_DIR,
                                                     ncch_program_id)
                        .data());
                const FileSys::Path mbox_path(
                    cecd->GetCecDataPathTypeAsString(CecDataPathType::CEC_PATH_MBOX_DIR,
                                                     ncch_program_id)
                        .data());

                /// Just in case the root dir /CEC doesn't exist, we try to create it first
                Service::FS::CreateDirectoryFromArchive(cecd->cecd_system_save_data_archive,
                                                        root_dir_path);
                Service::FS::CreateDirectoryFromArchive(cecd->cecd_system_save_data_archive,
                                                        mbox_path);
            }
            rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                               ErrorLevel::Status));
            rb.Push<u32>(0); /// No bytes read
        }
    }
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, open_mode={:#010x}, path={}",
              ncch_program_id, static_cast<u32>(path_type), open_mode.raw, path.AsString());
}

std::string Module::EncodeBase64(const std::vector<u8>& in, const std::string& dictionary) const {
    std::string out;
    out.reserve((in.size() * 4) / 3);
    int b;
    for (int i = 0; i < in.size(); i += 3) {
        b = (in[i] & 0xFC) >> 2;
        out += dictionary[b];
        b = (in[i] & 0x03) << 4;
        if (i + 1 < in.size()) {
            b |= (in[i + 1] & 0xF0) >> 4;
            out += dictionary[b];
            b = (in[i + 1] & 0x0F) << 2;
            if (i + 2 < in.size()) {
                b |= (in[i + 2] & 0xC0) >> 6;
                out += dictionary[b];
                b = in[i + 2] & 0x3F;
                out += dictionary[b];
            } else {
                out += dictionary[b];
            }
        } else {
            out += dictionary[b];
        }
    }
    return out;
}

std::string Module::GetCecDataPathTypeAsString(const CecDataPathType type, const u32 program_id,
                                               const std::vector<u8>& msg_id) const {
    switch (type) {
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
        return Common::StringFromFormat("/CEC/%08x/InBox___/_%08x", program_id,
                                        EncodeBase64(msg_id, base64_dict).data());
    case CecDataPathType::CEC_PATH_OUTBOX_MSG:
        return Common::StringFromFormat("/CEC/%08x/OutBox__/_%08x", program_id,
                                        EncodeBase64(msg_id, base64_dict).data());
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

Module::SessionData::SessionData() {}

Module::SessionData::~SessionData() {
    if (file)
        file->backend->Close();
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

        /// Now that the archive is formatted, we need to create the root CEC directory,
        /// eventlog.dat, and CEC/MBoxList____
        const FileSys::Path root_dir_path(
            GetCecDataPathTypeAsString(CecDataPathType::CEC_PATH_ROOT_DIR, 0).data());
        Service::FS::CreateDirectoryFromArchive(*archive_result, root_dir_path);

        FileSys::Mode mode;
        mode.write_flag.Assign(1);
        mode.create_flag.Assign(1);

        /// eventlog.dat resides in the root of the archive beside the CEC directory
        /// Initially created, at offset 0x0, are bytes 0x01 0x41 0x12, followed by
        /// zeroes until offset 0x1000, where it changes to 0xDD until the end of file
        /// at offset 0x30d53
        FileSys::Path eventlog_path("/eventlog.dat");

        auto eventlog_result =
            Service::FS::OpenFileFromArchive(*archive_result, eventlog_path, mode);

        constexpr u32 eventlog_size = 0x30d54;
        auto eventlog = eventlog_result.Unwrap();
        std::vector<u8> eventlog_buffer(eventlog_size);

        std::memset(&eventlog_buffer[0], 0, 0x1000);
        eventlog_buffer[0] = 0x01;
        eventlog_buffer[1] = 0x41;
        eventlog_buffer[2] = 0x12;
        std::memset(&eventlog_buffer[0x1000], 0xDD, eventlog_size - 0x1000);

        eventlog->backend->Write(0, eventlog_size, true, eventlog_buffer.data()).Unwrap();
        eventlog->backend->Close();

        /// MBoxList____ resides within the root CEC/ directory.
        /// Initially created, at offset 0x0, are bytes 0x68 0x68 0x00 0x00 0x01, with 0x6868 'hh',
        /// being the magic number. The rest of the file is filled with zeroes, until the end of
        /// file at offset 0x18b
        FileSys::Path mboxlist_path(
            GetCecDataPathTypeAsString(CecDataPathType::CEC_PATH_MBOX_LIST, 0).data());

        auto mboxlist_result =
            Service::FS::OpenFileFromArchive(*archive_result, mboxlist_path, mode);

        constexpr u32 mboxlist_size = 0x18c;
        auto mboxlist = mboxlist_result.Unwrap();
        std::vector<u8> mboxlist_buffer(mboxlist_size);

        std::memset(&mboxlist_buffer[0], 0, mboxlist_size);
        mboxlist_buffer[0] = 0x68;
        mboxlist_buffer[1] = 0x68;
        /// mboxlist_buffer[2-3] are already zeroed
        mboxlist_buffer[4] = 0x01;

        mboxlist->backend->Write(0, mboxlist_size, true, mboxlist_buffer.data()).Unwrap();
        mboxlist->backend->Close();
    }
    ASSERT_MSG(archive_result.Succeeded(), "Could not open the CECD SystemSaveData archive!");

    cecd_system_save_data_archive = *archive_result;
}

Module::~Module() {
    if (cecd_system_save_data_archive)
        Service::FS::CloseArchive(cecd_system_save_data_archive);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cecd = std::make_shared<Module>();
    std::make_shared<CECD_NDM>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_S>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_U>(cecd)->InstallAsService(service_manager);
}

} // namespace CECD
} // namespace Service
