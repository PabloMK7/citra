// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/map.hpp>
#include "common/archives.h"
#include "common/file_util.h"
#include "common/serialization/boost_std_variant.hpp"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/boss/online_service.h"

namespace Service::BOSS {

OnlineService::OnlineService(u64 program_id_, u64 extdata_id_)
    : program_id(program_id_), extdata_id(extdata_id_) {}

template <class Archive>
void OnlineService::serialize(Archive& ar, const unsigned int) {
    ar& current_props;
    ar& task_id_list;
    ar& program_id;
    ar& extdata_id;
}
SERIALIZE_IMPL(OnlineService)

template <class Archive>
void BossTaskProperties::serialize(Archive& ar, const unsigned int) {
    ar& task_result;
    ar& properties;
}
SERIALIZE_IMPL(BossTaskProperties)

ResultCode OnlineService::InitializeSession(u64 init_program_id) {
    // The BOSS service uses three databases:
    // BOSS_A: Archive? A list of program ids and some properties that are keyed on program
    // BOSS_SS: Saved Strings? Includes the url and the other string properties, and also some other
    // properties?, keyed on task_id
    // BOSS_SV: Saved Values? Includes task id and most properties keyed on task_id

    // It saves data in its databases in the following format:
    // A four byte header (always 00 80 34 12?) followed by any number of 0x800(BOSS_A) and
    // 0xC00(BOSS_SS and BOSS_SV) entries.

    current_props = BossTaskProperties();

    if (init_program_id == 0) {
        init_program_id = program_id;
    }

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010034
    const FileSys::Path archive_path(boss_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    std::unique_ptr<FileSys::ArchiveBackend> boss_system_save_data_archive;
    if (archive_result.Succeeded()) {
        boss_system_save_data_archive = std::move(archive_result).Unwrap();
    } else if (archive_result.Code() == FileSys::ERROR_NOT_FOUND) {
        // If the archive didn't exist, create the files inside
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);

        // Open it again to get a valid archive now that the folder exists
        auto create_archive_result = systemsavedata_factory.Open(archive_path, 0);
        if (!create_archive_result.Succeeded()) {
            LOG_ERROR(Service_BOSS, "Could not open BOSS savedata");
            return ResultCode(1);
        }
        boss_system_save_data_archive = std::move(create_archive_result).Unwrap();
    } else {
        LOG_ERROR(Service_BOSS, "Could not open BOSS savedata");
        return ResultCode(1);
    }

    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    // TODO: Read data from BOSS_A.db

    auto boss_sv_result =
        boss_system_save_data_archive->OpenFile(FileSys::Path("/BOSS_SV.db"), open_mode);
    auto boss_ss_result =
        boss_system_save_data_archive->OpenFile(FileSys::Path("/BOSS_SS.db"), open_mode);
    if (!boss_sv_result.Succeeded() || !boss_ss_result.Succeeded()) {
        LOG_ERROR(Service_BOSS, "Could not open BOSS database.");
        return RESULT_SUCCESS;
    }

    auto boss_sv = std::move(boss_sv_result).Unwrap();
    auto boss_ss = std::move(boss_ss_result).Unwrap();
    if (!(boss_sv->GetSize() > BOSS_SAVE_HEADER_SIZE &&
          ((boss_sv->GetSize() - BOSS_SAVE_HEADER_SIZE) % BOSS_S_ENTRY_SIZE) == 0 &&
          boss_sv->GetSize() == boss_ss->GetSize())) {
        LOG_ERROR(Service_BOSS, "BOSS database has incorrect size.");
        return RESULT_SUCCESS;
    }

    // Read the files if they already exist
    const u64 num_entries = (boss_sv->GetSize() - BOSS_SAVE_HEADER_SIZE) / BOSS_S_ENTRY_SIZE;
    for (u64 i = 0; i < num_entries; i++) {
        const u64 entry_offset = i * BOSS_S_ENTRY_SIZE + BOSS_SAVE_HEADER_SIZE;

        BossSVData sv_data;
        boss_sv->Read(entry_offset, sizeof(BossSVData), reinterpret_cast<u8*>(&sv_data));

        BossSSData ss_data;
        boss_ss->Read(entry_offset, sizeof(BossSSData), reinterpret_cast<u8*>(&ss_data));

        if (sv_data.program_id != init_program_id) {
            continue;
        }

        std::vector<u8> url_vector(URL_SIZE);
        std::memcpy(url_vector.data(), ss_data.url.data(), URL_SIZE);
        current_props.properties[PropertyID::Url] = url_vector;

        const auto task_id = std::string(sv_data.task_id.data());
        if (task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "TaskId already in list, will be replaced");
            task_id_list.erase(task_id);
        }

        task_id_list.emplace(task_id, std::move(current_props));
        current_props = BossTaskProperties();
    }

    return RESULT_SUCCESS;
}

void OnlineService::RegisterTask(const u32 size, Kernel::MappedBuffer& buffer) {
    std::string task_id(size, 0);
    buffer.Read(task_id.data(), 0, size);

    if (task_id_list.contains(task_id)) {
        LOG_WARNING(Service_BOSS, "TaskId already in list, will be replaced");
        task_id_list.erase(task_id);
    }
    task_id_list.emplace(task_id, std::move(current_props));
    current_props = BossTaskProperties();
}

ResultCode OnlineService::UnregisterTask(const u32 size, Kernel::MappedBuffer& buffer) {
    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "TaskId cannot be longer than 8");
        return ResultCode(1);
    }

    std::string task_id(size, 0);
    buffer.Read(task_id.data(), 0, size);
    if (task_id_list.erase(task_id) == 0) {
        LOG_WARNING(Service_BOSS, "TaskId not in list");
        return ResultCode(1);
    }

    return RESULT_SUCCESS;
}

void OnlineService::GetTaskIdList() {
    current_props.properties[PropertyID::TotalTasks] = static_cast<u16>(task_id_list.size());

    const auto num_task_ids = TASKIDLIST_SIZE / TASK_ID_SIZE;
    std::vector<std::array<u8, TASK_ID_SIZE>> task_ids(num_task_ids);

    u16 num_returned_task_ids = 0;
    for (const auto& iter : task_id_list) {
        const std::string current_task_id = iter.first;
        if (current_task_id.size() > TASK_ID_SIZE || num_returned_task_ids >= num_task_ids) {
            LOG_WARNING(Service_BOSS, "TaskId {} too long or too many TaskIds", current_task_id);
            continue;
        }
        std::memcpy(task_ids[num_returned_task_ids++].data(), current_task_id.data(), TASK_ID_SIZE);
    }

    auto* task_list_prop =
        std::get_if<std::vector<u8>>(&current_props.properties[PropertyID::TaskIdList]);
    if (task_list_prop && task_list_prop->size() == TASKIDLIST_SIZE) {
        std::memcpy(task_list_prop->data(), task_ids.data(), TASKIDLIST_SIZE);
    }
}

std::vector<FileSys::Entry> OnlineService::GetBossExtDataFiles() {
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), FileSys::ExtSaveDataType::Boss);
    const FileSys::Path boss_path{GetBossDataDir()};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return {};
    }

    auto boss_archive = std::move(archive_result).Unwrap();
    auto dir_result = boss_archive->OpenDirectory("/");
    if (!dir_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata directory opening failed");
        return {};
    }
    auto dir = std::move(dir_result).Unwrap();

    // Keep reading the directory 32 files at a time until all files have been checked
    std::vector<FileSys::Entry> boss_files;
    constexpr u32 files_to_read = 32;
    u32 entry_count = 0;
    std::size_t i = 0;
    do {
        boss_files.resize(boss_files.size() + files_to_read);
        entry_count = dir->Read(files_to_read, boss_files.data() + (i++ * files_to_read));
    } while (files_to_read <= entry_count);

    boss_files.resize(i * files_to_read + entry_count);
    return boss_files;
}

FileSys::Path OnlineService::GetBossDataDir() {
    const u32 high = static_cast<u32>(extdata_id >> 32);
    const u32 low = static_cast<u32>(extdata_id & 0xFFFFFFFF);
    return FileSys::ConstructExtDataBinaryPath(1, high, low);
}

std::vector<NsDataEntry> OnlineService::GetNsDataEntries() {
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), FileSys::ExtSaveDataType::Boss);
    const FileSys::Path boss_path{GetBossDataDir()};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return {};
    }
    auto boss_archive = std::move(archive_result).Unwrap().get();

    std::vector<NsDataEntry> ns_data;
    std::vector<FileSys::Entry> boss_files = GetBossExtDataFiles();
    for (const auto& current_file : boss_files) {
        constexpr u32 boss_header_length = 0x34;
        if (current_file.is_directory || current_file.file_size < boss_header_length) {
            LOG_WARNING(Service_BOSS, "SpotPass extdata contains directory or file is too short");
            continue;
        }

        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        NsDataEntry entry{.filename = Common::UTF16ToUTF8(current_file.filename)};
        auto file_result = boss_archive->OpenFile("/" + entry.filename, mode);
        if (!file_result.Succeeded()) {
            LOG_WARNING(Service_BOSS, "Opening SpotPass file failed.");
            continue;
        }

        auto file = std::move(file_result).Unwrap();
        file->Read(0, boss_header_length, reinterpret_cast<u8*>(&entry.header));
        if (entry.header.header_length != BOSS_EXTDATA_HEADER_LENGTH) {
            LOG_WARNING(
                Service_BOSS,
                "Incorrect header length or non-SpotPass file; expected {:#010x}, found {:#010x}",
                BOSS_EXTDATA_HEADER_LENGTH, entry.header.header_length);
            continue;
        }

        if (entry.header.program_id != program_id) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched program ID in SpotPass data. Was expecting "
                        "{:#018x}, found {:#018x}",
                        program_id, static_cast<u64>(entry.header.program_id));
            continue;
        }

        // Check the payload size is correct, excluding header
        if (entry.header.payload_size != (current_file.file_size - boss_header_length)) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched file size, was expecting {:#010x}, found {:#010x}",
                        static_cast<u32>(entry.header.payload_size),
                        current_file.file_size - boss_header_length);
            continue;
        }

        ns_data.push_back(entry);
    }

    return ns_data;
}

u16 OnlineService::GetNsDataIdList(const u32 filter, const u32 max_entries,
                                   Kernel::MappedBuffer& buffer) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries();
    std::vector<u32> output_entries;
    for (const auto& current_entry : ns_data) {
        const u32 datatype_raw = static_cast<u32>(current_entry.header.datatype);
        const u16 datatype_high = static_cast<u16>(datatype_raw >> 16);
        const u16 datatype_low = static_cast<u16>(datatype_raw & 0xFFFF);
        const u16 filter_high = static_cast<u16>(filter >> 16);
        const u16 filter_low = static_cast<u16>(filter & 0xFFFF);
        if (filter != 0xFFFFFFFF &&
            (filter_high != datatype_high || (filter_low & datatype_low) == 0)) {
            continue;
        }

        if (output_entries.size() >= max_entries) {
            LOG_WARNING(Service_BOSS, "Reached maximum number of entries");
            break;
        }

        output_entries.push_back(current_entry.header.ns_data_id);
    }

    buffer.Write(output_entries.data(), 0, sizeof(u32) * output_entries.size());
    return static_cast<u16>(output_entries.size());
}

template <class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

ResultCode OnlineService::SendProperty(const u16 id, const u32 size, Kernel::MappedBuffer& buffer) {
    const auto property_id = static_cast<PropertyID>(id);
    if (!current_props.properties.contains(property_id)) {
        LOG_ERROR(Service_BOSS, "Unknown property with id {:#06x}", property_id);
        return ResultCode(1);
    }

    auto& prop = current_props.properties[property_id];
    auto read_pod = [&]<typename T>(T& old_prop) {
        static_assert(std::is_trivial<T>::value, "Only trivial types are allowed for read_pod");
        if (size != sizeof(old_prop)) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, sizeof(old_prop), size);
        }
        T cur_prop = 0;
        buffer.Read(&cur_prop, 0, size);
        prop = cur_prop;
    };

    auto read_vector = [&]<typename T>(std::vector<T>& old_prop) {
        if (size != old_prop.size()) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, old_prop.size(), size);
        }
        std::vector<T> cur_prop(size);
        buffer.Read(cur_prop.data(), 0, size);
        prop = cur_prop;
    };

    std::visit(overload{[&](u8& cur_prop) { read_pod(cur_prop); },
                        [&](u16& cur_prop) { read_pod(cur_prop); },
                        [&](u32& cur_prop) { read_pod(cur_prop); },
                        [&](std::vector<u8>& cur_prop) { read_vector(cur_prop); },
                        [&](std::vector<u32>& cur_prop) { read_vector(cur_prop); }},
               prop);

    return RESULT_SUCCESS;
}

ResultCode OnlineService::ReceiveProperty(const u16 id, const u32 size,
                                          Kernel::MappedBuffer& buffer) {
    const auto property_id = static_cast<PropertyID>(id);
    if (!current_props.properties.contains(property_id)) {
        LOG_ERROR(Service_BOSS, "Unknown property with id {:#06x}", property_id);
        return ResultCode(1);
    }

    auto write_pod = [&]<typename T>(T& cur_prop) {
        static_assert(std::is_trivial<T>::value, "Only trivial types are allowed for write_pod");
        if (size != sizeof(cur_prop)) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, sizeof(cur_prop), size);
        }
        buffer.Write(&cur_prop, 0, size);
    };

    auto write_vector = [&]<typename T>(std::vector<T>& cur_prop) {
        if (size != cur_prop.size()) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, cur_prop.size(), size);
        }
        buffer.Write(cur_prop.data(), 0, size);
    };

    auto& prop = current_props.properties[property_id];
    std::visit(overload{[&](u8& cur_prop) { write_pod(cur_prop); },
                        [&](u16& cur_prop) { write_pod(cur_prop); },
                        [&](u32& cur_prop) { write_pod(cur_prop); },
                        [&](std::vector<u8>& cur_prop) { write_vector(cur_prop); },
                        [&](std::vector<u32>& cur_prop) { write_vector(cur_prop); }},
               prop);

    return RESULT_SUCCESS;
}

} // namespace Service::BOSS
