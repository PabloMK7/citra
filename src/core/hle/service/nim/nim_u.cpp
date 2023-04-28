// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/nim/nim_u.h"

SERVICE_CONSTRUCT_IMPL(Service::NIM::NIM_U)
SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_U)

namespace Service::NIM {

enum class SystemUpdateState : u32 {
    NotInitialized,
    StartingSystemUpdate,
    FetchingHashAndAccountStatus,
    InstallingNewTickets,
    InstallingTitles,
    UpdateComplete,
    SystemUpdatesDisabled,
    Unknown7,
    Unknown8,
};

struct SystemUpdateProgress {
    SystemUpdateState state;
    u32 last_operation_result;
    u64 current_title_downloaded_bytes;
    u64 current_title_total_bytes;
    u64 titles_downloaded;
    u64 titles_total;
};

static_assert(sizeof(SystemUpdateProgress) == 0x28, "SystemUpdateProgress structure size is wrong");

enum class TitleDownloadState : u32 {
    NotInitialized,
    StartingTitleDownload,
    InstallingTmd,
    CommittingTmd,
    InstallingContents,
    ContentsInstalled,
    CommittingTitles,
    Finished,
    Unknown8,
    Unknown9,
    BackgroundDownloadFailed,
};

struct TitleDownloadProgress {
    TitleDownloadState state;
    u32 last_operation_result;
    u64 downloaded_bytes;
    u64 total_bytes;
};

static_assert(sizeof(TitleDownloadProgress) == 0x18,
              "TitleDownloadProgress structure size is wrong");

struct TitleDownloadConfig {
    u64 title_id;
    u32 title_version;
    u32 unknown_1;
    u8 age_rating;
    u8 media_type;
    INSERT_PADDING_BYTES(2);
    u32 unknown_2;
};

static_assert(sizeof(TitleDownloadConfig) == 0x18, "TitleDownloadConfig structure size is wrong");

#pragma pack(1)
struct BackgroundTitleDownloadConfig {
    TitleDownloadConfig base_config;
    u8 unknown_1;
    u8 unknown_2;
    INSERT_PADDING_BYTES(6);
    u64 requester_title_id;
    std::array<u16, 0x48> title_name;
    u16 title_name_terminator;
    std::array<u16, 0x24> developer_name;
    u16 developer_name_terminator;
};

static_assert(sizeof(BackgroundTitleDownloadConfig) == 0x104,
              "BackgroundTitleDownloadConfig structure size is wrong");

struct BackgroundTitleDownloadTaskInfo {
    BackgroundTitleDownloadConfig config;
    INSERT_PADDING_BYTES(4);
    TitleDownloadProgress progress;
};

static_assert(sizeof(BackgroundTitleDownloadTaskInfo) == 0x120,
              "BackgroundTitleDownloadTaskInfo structure size is wrong");

struct AutoTitleDownloadTaskInfo {
    u64 task_id;
    u64 title_id;
    u32 title_version;
    u8 unknown_4[0x14];
    u64 required_size;
    u32 last_operation_result_code;
    u32 last_operation_customer_support_code;
    std::array<u16, 0x48> title_name;
    u16 title_name_terminator;
    std::array<u16, 0x24> developer_name;
    u16 developer_name_terminator;
    u8 unknown_5[0x24];
};

static_assert(sizeof(AutoTitleDownloadTaskInfo) == 0x138,
              "AutoTitleDownloadTaskInfo structure size is wrong");

struct AutoDbgDat {
    u8 unknown_1[0x4];
    u32 num_auto_download_tasks;
    u8 unknown_2[0x100];
};

static_assert(sizeof(AutoDbgDat) == 0x108, "AutoDbgDat structure size is wrong");

NIM_U::NIM_U(Core::System& system) : ServiceFramework("nim:u", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 0), &NIM_U::StartNetworkUpdate, "StartNetworkUpdate"},
        {IPC::MakeHeader(0x0002, 0, 0), &NIM_U::GetProgress, "GetProgress"},
        {IPC::MakeHeader(0x0003, 0, 0), &NIM_U::Cancel, "Cancel"},
        {IPC::MakeHeader(0x0004, 0, 0), &NIM_U::CommitSystemTitles, "CommitSystemTitles"},
        {IPC::MakeHeader(0x0005, 0, 0), &NIM_U::GetBackgroundEventForMenu, "GetBackgroundEventForMenu"},
        {IPC::MakeHeader(0x0006, 0, 0), &NIM_U::GetBackgroundEventForNews, "GetBackgroundEventForNews"},
        {IPC::MakeHeader(0x0007, 0, 0), &NIM_U::FormatSaveData, "FormatSaveData"},
        {IPC::MakeHeader(0x0008, 0, 0), &NIM_U::GetCustomerSupportCode, "GetCustomerSupportCode"},
        {IPC::MakeHeader(0x0009, 0, 0), &NIM_U::IsCommittableAllSystemTitles, "IsCommittableAllSystemTitles"},
        {IPC::MakeHeader(0x000A, 0, 0), &NIM_U::GetBackgroundProgress, "GetBackgroundProgress"},
        {IPC::MakeHeader(0x000B, 0, 0), &NIM_U::GetSavedHash, "GetSavedHash"},
        {IPC::MakeHeader(0x000C, 2, 2), &NIM_U::UnregisterTask, "UnregisterTask"},
        {IPC::MakeHeader(0x000D, 2, 0), &NIM_U::IsRegistered, "IsRegistered"},
        {IPC::MakeHeader(0x000E, 2, 0), &NIM_U::FindTaskInfo, "FindTaskInfo"},
        {IPC::MakeHeader(0x000F, 1, 2), &NIM_U::GetTaskInfos, "GetTaskInfos"},
        {IPC::MakeHeader(0x0010, 0, 0), &NIM_U::DeleteUnmanagedContexts, "DeleteUnmanagedContexts"},
        {IPC::MakeHeader(0x0011, 0, 0), &NIM_U::UpdateAutoTitleDownloadTasksAsync, "UpdateAutoTitleDownloadTasksAsync"},
        {IPC::MakeHeader(0x0012, 0, 0), &NIM_U::StartPendingAutoTitleDownloadTasksAsync, "StartPendingAutoTitleDownloadTasksAsync"},
        {IPC::MakeHeader(0x0013, 0, 0), &NIM_U::GetAsyncResult, "GetAsyncResult"},
        {IPC::MakeHeader(0x0014, 0, 0), &NIM_U::CancelAsyncCall, "CancelAsyncCall"},
        {IPC::MakeHeader(0x0015, 0, 0), &NIM_U::IsPendingAutoTitleDownloadTasks, "IsPendingAutoTitleDownloadTasks"},
        {IPC::MakeHeader(0x0016, 0, 0), &NIM_U::GetNumAutoTitleDownloadTasks, "GetNumAutoTitleDownloadTasks"},
        {IPC::MakeHeader(0x0017, 1, 2), &NIM_U::GetAutoTitleDownloadTaskInfos, "GetAutoTitleDownloadTaskInfos"},
        {IPC::MakeHeader(0x0018, 2, 0), &NIM_U::CancelAutoTitleDownloadTask, "CancelAutoTitleDownloadTask"},
        {IPC::MakeHeader(0x0019, 0, 2), &NIM_U::SetAutoDbgDat, "SetAutoDbgDat"},
        {IPC::MakeHeader(0x001A, 0, 2), &NIM_U::GetAutoDbgDat, "GetAutoDbgDat"},
        {IPC::MakeHeader(0x001B, 1, 2), &NIM_U::SetDbgTasks, "SetDbgTasks"},
        {IPC::MakeHeader(0x001C, 1, 2), &NIM_U::GetDbgTasks, "GetDbgTasks"},
        {IPC::MakeHeader(0x001D, 0, 0), &NIM_U::DeleteDbgData, "DeleteDbgData"},
        {IPC::MakeHeader(0x001E, 1, 2), &NIM_U::SetTslXml, "SetTslXml"},
        {IPC::MakeHeader(0x001F, 0, 0), &NIM_U::GetTslXmlSize, "GetTslXmlSize"},
        {IPC::MakeHeader(0x0020, 1, 2), &NIM_U::GetTslXml, "GetTslXml"},
        {IPC::MakeHeader(0x0021, 0, 0), &NIM_U::DeleteTslXml, "DeleteTslXml"},
        {IPC::MakeHeader(0x0022, 1, 2), &NIM_U::SetDtlXml, "SetDtlXml"},
        {IPC::MakeHeader(0x0023, 0, 0), &NIM_U::GetDtlXmlSize, "GetDtlXmlSize"},
        {IPC::MakeHeader(0x0024, 1, 2), &NIM_U::GetDtlXml, "GetDtlXml"},
        {IPC::MakeHeader(0x0025, 0, 0), &NIM_U::UpdateAccountStatus, "UpdateAccountStatus"},
        {IPC::MakeHeader(0x0026, 6, 0), &NIM_U::StartTitleDownload, "StartTitleDownload"},
        {IPC::MakeHeader(0x0027, 0, 0), &NIM_U::StopTitleDownload, "StopTitleDownload"},
        {IPC::MakeHeader(0x0028, 0, 0), &NIM_U::GetTitleDownloadProgress, "GetTitleDownloadProgress"},
        {IPC::MakeHeader(0x0029, 9, 6), &NIM_U::RegisterTask, "RegisterTask"},
        {IPC::MakeHeader(0x002A, 0, 0), &NIM_U::IsSystemUpdateAvailable, "IsSystemUpdateAvailable"},
        {IPC::MakeHeader(0x002B, 0, 0), &NIM_U::Unknown2B, "Unknown2B"},
        {IPC::MakeHeader(0x002C, 0, 0), &NIM_U::UpdateTickets, "UpdateTickets"},
        {IPC::MakeHeader(0x002D, 3, 0), &NIM_U::DownloadTitleSeedAsync, "DownloadTitleSeedAsync"},
        {IPC::MakeHeader(0x002E, 0, 0), &NIM_U::DownloadMissingTitleSeedsAsync, "DownloadMissingTitleSeedsAsync"},
        // clang-format on
    };
    RegisterHandlers(functions);
    nim_system_update_event_for_menu =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NIM System Update Event (Menu)");
    nim_system_update_event_for_news =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NIM System Update Event (News)");
    nim_async_completion_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NIM Async Completion Event");
}

NIM_U::~NIM_U() = default;

void NIM_U::StartNetworkUpdate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 0, 0); // 0x10000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetProgress(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2, 0, 0); // 0x20000

    SystemUpdateProgress progress{};
    std::memset(&progress, 0, sizeof(progress));

    IPC::RequestBuilder rb = rp.MakeBuilder(13, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(progress);
    rb.Push(0);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::Cancel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3, 0, 0); // 0x30000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::CommitSystemTitles(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x4, 0, 0); // 0x40000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetBackgroundEventForMenu(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0); // 0x50000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_system_update_event_for_menu);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetBackgroundEventForNews(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x6, 0, 0); // 0x60000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_system_update_event_for_news);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::FormatSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x7, 0, 0); // 0x70000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetCustomerSupportCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x8, 0, 0); // 0x80000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0); // Customer support code

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::IsCommittableAllSystemTitles(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0); // 0x90000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetBackgroundProgress(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xA, 0, 0); // 0xA0000

    SystemUpdateProgress progress{};
    std::memset(&progress, 0, sizeof(progress));

    IPC::RequestBuilder rb = rp.MakeBuilder(13, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(progress);
    rb.Push(0);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetSavedHash(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 0, 0); // 0xB0000

    std::array<char, 0x24> hash{};
    std::memset(&hash, 0, sizeof(hash));

    IPC::RequestBuilder rb = rp.MakeBuilder(10, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(hash);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::UnregisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xC, 2, 2); // 0xC0082

    const u64 title_id = rp.Pop<u64>();
    const u32 process_id = rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::NIM, ErrorSummary::NotFound,
                       ErrorLevel::Status));

    LOG_WARNING(Service_NIM, "(STUBBED) called title_id={:016X}, process_id={:08X}", title_id,
                process_id);
}

void NIM_U::IsRegistered(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xD, 2, 0); // 0xD0080

    const u64 title_id = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_NIM, "(STUBBED) called title_id={:016X}", title_id);
}

void NIM_U::FindTaskInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xE, 2, 0); // 0xE0080

    const u64 title_id = rp.Pop<u64>();

    std::vector<u8> buffer(0x120, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::NIM, ErrorSummary::NotFound,
                       ErrorLevel::Status));
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_NIM, "(STUBBED) called title_id={:016X}", title_id);
}

void NIM_U::GetTaskInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xF, 1, 2); // 0xF0042

    const u64 max_task_infos = rp.Pop<u32>();
    auto& task_infos_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);
    rb.PushMappedBuffer(task_infos_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called max_task_infos={:08X}, task_infos_buffer=0x{:08X}",
                max_task_infos, task_infos_buffer.GetId());
}

void NIM_U::DeleteUnmanagedContexts(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 0, 0); // 0x100000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::UpdateAutoTitleDownloadTasksAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0); // 0x110000

    // Since this is a stub, signal the completion event so the caller won't get stuck waiting.
    nim_async_completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_async_completion_event);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::StartPendingAutoTitleDownloadTasksAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0); // 0x120000

    // Since this is a stub, signal the completion event so the caller won't get stuck waiting.
    nim_async_completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_async_completion_event);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetAsyncResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 0, 0); // 0x130000

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::CancelAsyncCall(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 0, 0); // 0x140000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::IsPendingAutoTitleDownloadTasks(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 0, 0); // 0x150000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetNumAutoTitleDownloadTasks(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 0, 0); // 0x160000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetAutoTitleDownloadTaskInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 1, 2); // 0x170042

    const u64 max_task_infos = rp.Pop<u32>();
    auto& task_infos_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);
    rb.PushMappedBuffer(task_infos_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called max_task_infos={:08X}, task_infos_buffer=0x{:08X}",
                max_task_infos, task_infos_buffer.GetId());
}

void NIM_U::CancelAutoTitleDownloadTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 2, 0); // 0x180080

    const u64 task_id = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called task_id={:016X}", task_id);
}

void NIM_U::SetAutoDbgDat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x19, 0, 2); // 0x190002

    auto& auto_dbg_dat_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(auto_dbg_dat_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called auto_dbg_dat_buffer=0x{:08X}",
                auto_dbg_dat_buffer.GetId());
}

void NIM_U::GetAutoDbgDat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 0, 2); // 0x1A0002

    auto& auto_dbg_dat_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(auto_dbg_dat_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called auto_dbg_dat_buffer=0x{:08X}",
                auto_dbg_dat_buffer.GetId());
}

void NIM_U::SetDbgTasks(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 1, 2); // 0x1B0042

    const u64 max_task_infos = rp.Pop<u32>();
    auto& task_infos_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(task_infos_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called max_task_infos={:08X}, task_infos_buffer=0x{:08X}",
                max_task_infos, task_infos_buffer.GetId());
}

void NIM_U::GetDbgTasks(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1C, 1, 2); // 0x1C0042

    const u64 max_task_infos = rp.Pop<u32>();
    auto& task_infos_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);
    rb.PushMappedBuffer(task_infos_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called max_task_infos={:08X}, task_infos_buffer=0x{:08X}",
                max_task_infos, task_infos_buffer.GetId());
}

void NIM_U::DeleteDbgData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1D, 0, 0); // 0x1D0000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::SetTslXml(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 1, 2); // 0x1E0042

    const u32 buffer_size = rp.Pop<u32>();
    auto& xml_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(xml_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called buffer_size={:08X}, xml_buffer=0x{:08X}",
                buffer_size, xml_buffer.GetId());
}

void NIM_U::GetTslXmlSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 0, 0); // 0x1F0000

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetTslXml(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x20, 1, 2); // 0x200042

    const u32 buffer_capacity = rp.Pop<u32>();
    auto& xml_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(xml_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called buffer_capacity={:08X}, xml_buffer=0x{:08X}",
                buffer_capacity, xml_buffer.GetId());
}

void NIM_U::DeleteTslXml(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x21, 0, 0); // 0x210000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::SetDtlXml(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x22, 1, 2); // 0x220042

    const u32 buffer_size = rp.Pop<u32>();
    auto& xml_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(xml_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called buffer_size={:08X}, xml_buffer=0x{:08X}",
                buffer_size, xml_buffer.GetId());
}

void NIM_U::GetDtlXmlSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x23, 0, 0); // 0x230000

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetDtlXml(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x24, 1, 2); // 0x240042

    const u32 buffer_capacity = rp.Pop<u32>();
    auto& xml_buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(xml_buffer);

    LOG_WARNING(Service_NIM, "(STUBBED) called buffer_capacity={:08X}, xml_buffer=0x{:08X}",
                buffer_capacity, xml_buffer.GetId());
}

void NIM_U::UpdateAccountStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x25, 0, 0); // 0x250000

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::StartTitleDownload(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x26, 6, 0); // 0x260180

    const auto& download_config = rp.PopRaw<TitleDownloadConfig>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called title_id={:016X}", download_config.title_id);
}

void NIM_U::StopTitleDownload(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x27, 0, 0); // 0x270000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::GetTitleDownloadProgress(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x28, 0, 0); // 0x280000

    TitleDownloadProgress progress{};
    std::memset(&progress, 0, sizeof(progress));

    IPC::RequestBuilder rb = rp.MakeBuilder(9, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(progress);
    rb.Push(0);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::RegisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x29, 9, 6); // 0x290246

    const auto& download_config = rp.PopRaw<TitleDownloadConfig>();
    const u32 unknown_1 = rp.Pop<u32>();
    const u32 unknown_2 = rp.Pop<u32>();
    const u32 unknown_3 = rp.Pop<u32>();
    const u32 pid = rp.PopPID();
    const auto& title_name = rp.PopStaticBuffer();
    const auto& developer_name = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    const auto title_name_end = std::find(title_name.begin(), title_name.end(), u'\0');
    const auto title_name_utf8 =
        Common::UTF16ToUTF8(std::u16string{title_name.begin(), title_name_end});

    const auto developer_name_end = std::find(developer_name.begin(), developer_name.end(), u'\0');
    const auto developer_name_utf8 =
        Common::UTF16ToUTF8(std::u16string{developer_name.begin(), developer_name_end});

    LOG_WARNING(Service_NIM,
                "(STUBBED) called title_id={:016X}, unknown_1={:08X}, unknown_2={:08X}, "
                "unknown_3={:08X}, pid={:08X}, title_name='{}', developer_name='{}'",
                download_config.title_id, unknown_1, unknown_2, unknown_3, pid, title_name_utf8,
                developer_name_utf8);
}

void NIM_U::IsSystemUpdateAvailable(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2A, 0, 0); // 0x2A0000

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);
    rb.Push(false);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::Unknown2B(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2B, 0, 0); // 0x2B0000

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::UpdateTickets(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2C, 0, 0); // 0x2C0000

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(RESULT_SUCCESS);
    rb.Push(0);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::DownloadTitleSeedAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2D, 3, 0); // 0x2D00C0

    const u64 title_id = rp.Pop<u64>();
    const u16 country_code = rp.Pop<u16>();

    // Since this is a stub, signal the completion event so the caller won't get stuck waiting.
    nim_async_completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_async_completion_event);

    LOG_WARNING(Service_NIM, "(STUBBED) called title_id={:016X}, country_code={:04X}", title_id,
                country_code);
}

void NIM_U::DownloadMissingTitleSeedsAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2E, 0, 0); // 0x2E0000

    // Since this is a stub, signal the completion event so the caller won't get stuck waiting.
    nim_async_completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_async_completion_event);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

} // namespace Service::NIM
