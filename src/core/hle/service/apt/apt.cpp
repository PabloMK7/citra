// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/romfs.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"
#include "core/hle/service/apt/apt_s.h"
#include "core/hle/service/apt/apt_u.h"
#include "core/hle/service/apt/bcfnt/bcfnt.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/service.h"
#include "core/hw/aes/ccm.h"
#include "core/hw/aes/key.h"

namespace Service {
namespace APT {

/// Handle to shared memory region designated to for shared system font
static Kernel::SharedPtr<Kernel::SharedMemory> shared_font_mem;
static bool shared_font_loaded = false;
static bool shared_font_relocated = false;

static Kernel::SharedPtr<Kernel::Mutex> lock;

static u32 cpu_percent; ///< CPU time available to the running application

// APT::CheckNew3DSApp will check this unknown_ns_state_field to determine processing mode
static u8 unknown_ns_state_field;

static ScreencapPostPermission screen_capture_post_permission;

static std::shared_ptr<AppletManager> applet_manager;

void Initialize(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2, 2, 0); // 0x20080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 attributes = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called app_id=0x%08X, attributes=0x%08X", static_cast<u32>(app_id),
              attributes);

    auto result = applet_manager->Initialize(app_id, attributes);
    if (result.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(result.Code());
    } else {
        auto events = std::move(result).Unwrap();
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyHandles(Kernel::g_handle_table.Create(events.notification_event).Unwrap(),
                           Kernel::g_handle_table.Create(events.parameter_event).Unwrap());
    }
}

static u32 DecompressLZ11(const u8* in, u8* out) {
    u32_le decompressed_size;
    memcpy(&decompressed_size, in, sizeof(u32));
    in += 4;

    u8 type = decompressed_size & 0xFF;
    ASSERT(type == 0x11);
    decompressed_size >>= 8;

    u32 current_out_size = 0;
    u8 flags = 0, mask = 1;
    while (current_out_size < decompressed_size) {
        if (mask == 1) {
            flags = *(in++);
            mask = 0x80;
        } else {
            mask >>= 1;
        }

        if (flags & mask) {
            u8 byte1 = *(in++);
            u32 length = byte1 >> 4;
            u32 offset;
            if (length == 0) {
                u8 byte2 = *(in++);
                u8 byte3 = *(in++);
                length = (((byte1 & 0x0F) << 4) | (byte2 >> 4)) + 0x11;
                offset = (((byte2 & 0x0F) << 8) | byte3) + 0x1;
            } else if (length == 1) {
                u8 byte2 = *(in++);
                u8 byte3 = *(in++);
                u8 byte4 = *(in++);
                length = (((byte1 & 0x0F) << 12) | (byte2 << 4) | (byte3 >> 4)) + 0x111;
                offset = (((byte3 & 0x0F) << 8) | byte4) + 0x1;
            } else {
                u8 byte2 = *(in++);
                length = (byte1 >> 4) + 0x1;
                offset = (((byte1 & 0x0F) << 8) | byte2) + 0x1;
            }

            for (u32 i = 0; i < length; i++) {
                *out = *(out - offset);
                ++out;
            }

            current_out_size += length;
        } else {
            *(out++) = *(in++);
            current_out_size++;
        }
    }
    return decompressed_size;
}

static bool LoadSharedFont() {
    u8 font_region_code;
    switch (CFG::GetRegionValue()) {
    case 4: // CHN
        font_region_code = 2;
        break;
    case 5: // KOR
        font_region_code = 3;
        break;
    case 6: // TWN
        font_region_code = 4;
        break;
    default: // JPN/EUR/USA
        font_region_code = 1;
        break;
    }

    const u64_le shared_font_archive_id_low = 0x0004009b00014002 | ((font_region_code - 1) << 8);
    const u64_le shared_font_archive_id_high = 0x00000001ffffff00;
    std::vector<u8> shared_font_archive_id(16);
    std::memcpy(&shared_font_archive_id[0], &shared_font_archive_id_low, sizeof(u64));
    std::memcpy(&shared_font_archive_id[8], &shared_font_archive_id_high, sizeof(u64));
    FileSys::Path archive_path(shared_font_archive_id);
    auto archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::NCCH, archive_path);
    if (archive_result.Failed())
        return false;

    std::vector<u8> romfs_path(20, 0); // 20-byte all zero path for opening RomFS
    FileSys::Path file_path(romfs_path);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = Service::FS::OpenFileFromArchive(*archive_result, file_path, open_mode);
    if (file_result.Failed())
        return false;

    auto romfs = std::move(file_result).Unwrap();
    std::vector<u8> romfs_buffer(romfs->backend->GetSize());
    romfs->backend->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->backend->Close();

    const char16_t* file_name[4] = {u"cbf_std.bcfnt.lz", u"cbf_zh-Hans-CN.bcfnt.lz",
                                    u"cbf_ko-Hang-KR.bcfnt.lz", u"cbf_zh-Hant-TW.bcfnt.lz"};
    const u8* font_file =
        RomFS::GetFilePointer(romfs_buffer.data(), {file_name[font_region_code - 1]});
    if (font_file == nullptr)
        return false;

    struct {
        u32_le status;
        u32_le region;
        u32_le decompressed_size;
        INSERT_PADDING_WORDS(0x1D);
    } shared_font_header{};
    static_assert(sizeof(shared_font_header) == 0x80, "shared_font_header has incorrect size");

    shared_font_header.status = 2; // successfully loaded
    shared_font_header.region = font_region_code;
    shared_font_header.decompressed_size =
        DecompressLZ11(font_file, shared_font_mem->GetPointer(0x80));
    std::memcpy(shared_font_mem->GetPointer(), &shared_font_header, sizeof(shared_font_header));
    *shared_font_mem->GetPointer(0x83) = 'U'; // Change the magic from "CFNT" to "CFNU"

    return true;
}

static bool LoadLegacySharedFont() {
    // This is the legacy method to load shared font.
    // The expected format is a decrypted, uncompressed BCFNT file with the 0x80 byte header
    // generated by the APT:U service. The best way to get is by dumping it from RAM. We've provided
    // a homebrew app to do this: https://github.com/citra-emu/3dsutils. Put the resulting file
    // "shared_font.bin" in the Citra "sysdata" directory.
    std::string filepath = FileUtil::GetUserPath(D_SYSDATA_IDX) + SHARED_FONT;

    FileUtil::CreateFullPath(filepath); // Create path if not already created
    FileUtil::IOFile file(filepath, "rb");
    if (file.IsOpen()) {
        file.ReadBytes(shared_font_mem->GetPointer(), file.GetSize());
        return true;
    }

    return false;
}

void GetSharedFont(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x44, 0, 0); // 0x00440000
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    // Log in telemetry if the game uses the shared font
    Core::Telemetry().AddField(Telemetry::FieldType::Session, "RequiresSharedFont", true);

    if (!shared_font_loaded) {
        // On real 3DS, font loading happens on booting. However, we load it on demand to coordinate
        // with CFG region auto configuration, which happens later than APT initialization.
        if (LoadSharedFont()) {
            shared_font_loaded = true;
        } else if (LoadLegacySharedFont()) {
            LOG_WARNING(Service_APT, "Loaded shared font by legacy method");
            shared_font_loaded = true;
        } else {
            LOG_ERROR(Service_APT, "shared font file missing - go dump it from your 3ds");
            rb.Push<u32>(-1); // TODO: Find the right error code
            rb.Skip(1 + 2, true);
            Core::System::GetInstance().SetStatus(Core::System::ResultStatus::ErrorSharedFont);
            return;
        }
    }

    // The shared font has to be relocated to the new address before being passed to the
    // application.
    VAddr target_address =
        Memory::PhysicalToVirtualAddress(shared_font_mem->linear_heap_phys_address).value();
    if (!shared_font_relocated) {
        BCFNT::RelocateSharedFont(shared_font_mem, target_address);
        shared_font_relocated = true;
    }

    rb.Push(RESULT_SUCCESS); // No error
    // Since the SharedMemory interface doesn't provide the address at which the memory was
    // allocated, the real APT service calculates this address by scanning the entire address space
    // (using svcQueryMemory) and searches for an allocation of the same size as the Shared Font.
    rb.Push(target_address);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(shared_font_mem).Unwrap());
}

void NotifyToWait(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x43, 1, 0); // 0x430040
    u32 app_id = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id=%u", app_id);
}

void GetLockHandle(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1, 1, 0); // 0x10040

    // Bits [0:2] are the applet type (System, Library, etc)
    // Bit 5 tells the application that there's a pending APT parameter,
    // this will cause the app to wait until parameter_event is signaled.
    u32 applet_attributes = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS); // No error

    // TODO(Subv): The output attributes should have an AppletPos of either Library or System |
    // Library (depending on the type of the last launched applet) if the input attributes'
    // AppletPos has the Library bit set.

    rb.Push(applet_attributes); // Applet Attributes, this value is passed to Enable.
    rb.Push<u32>(0);            // Least significant bit = power button state
    Kernel::Handle handle_copy = Kernel::g_handle_table.Create(lock).Unwrap();
    rb.PushCopyHandles(handle_copy);

    LOG_WARNING(Service_APT, "(STUBBED) called handle=0x%08X applet_attributes=0x%08X", handle_copy,
                applet_attributes);
}

void Enable(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x3, 1, 0); // 0x30040
    u32 attributes = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called attributes=0x%08X", attributes);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(applet_manager->Enable(attributes));
}

void GetAppletManInfo(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x5, 1, 0); // 0x50040
    u32 unk = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push(static_cast<u32>(AppletId::HomeMenu));    // Home menu AppID
    rb.Push(static_cast<u32>(AppletId::Application)); // TODO(purpasmart96): Do this correctly

    LOG_WARNING(Service_APT, "(STUBBED) called unk=0x%08X", unk);
}

void IsRegistered(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x9, 1, 0); // 0x90040
    AppletId app_id = static_cast<AppletId>(rp.Pop<u32>());
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(applet_manager->IsRegistered(app_id));

    LOG_DEBUG(Service_APT, "called app_id=0x%08X", static_cast<u32>(app_id));
}

void InquireNotification(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xB, 1, 0); // 0xB0040
    u32 app_id = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);                     // No error
    rb.Push(static_cast<u32>(SignalType::None)); // Signal type
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void SendParameter(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xC, 4, 4); // 0xC0104
    AppletId src_app_id = rp.PopEnum<AppletId>();
    AppletId dst_app_id = rp.PopEnum<AppletId>();
    SignalType signal_type = rp.PopEnum<SignalType>();
    u32 buffer_size = rp.Pop<u32>();
    Kernel::Handle handle = rp.PopHandle();
    size_t size;
    VAddr buffer = rp.PopStaticBuffer(&size);

    LOG_DEBUG(Service_APT,
              "called src_app_id=0x%08X, dst_app_id=0x%08X, signal_type=0x%08X,"
              "buffer_size=0x%08X, handle=0x%08X, size=0x%08zX, in_param_buffer_ptr=0x%08X",
              static_cast<u32>(src_app_id), static_cast<u32>(dst_app_id),
              static_cast<u32>(signal_type), buffer_size, handle, size, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    MessageParameter param;
    param.destination_id = dst_app_id;
    param.sender_id = src_app_id;
    param.object = Kernel::g_handle_table.GetGeneric(handle);
    param.signal = signal_type;
    param.buffer.resize(buffer_size);
    Memory::ReadBlock(buffer, param.buffer.data(), param.buffer.size());

    rb.Push(applet_manager->SendParameter(param));
}

void ReceiveParameter(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xD, 2, 0); // 0xD0080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 buffer_size = rp.Pop<u32>();

    size_t static_buff_size;
    VAddr buffer = rp.PeekStaticBuffer(0, &static_buff_size);
    if (buffer_size > static_buff_size)
        LOG_WARNING(
            Service_APT,
            "buffer_size is bigger than the size in the buffer descriptor (0x%08X > 0x%08zX)",
            buffer_size, static_buff_size);

    LOG_DEBUG(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", static_cast<u32>(app_id),
              buffer_size);

    auto next_parameter = applet_manager->ReceiveParameter(app_id);

    if (next_parameter.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(next_parameter.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 4);

    rb.Push(RESULT_SUCCESS); // No error
    rb.PushEnum(next_parameter->sender_id);
    rb.PushEnum(next_parameter->signal); // Signal type
    ASSERT_MSG(next_parameter->buffer.size() <= buffer_size, "Input static buffer is too small !");
    rb.Push(static_cast<u32>(next_parameter->buffer.size())); // Parameter buffer size

    rb.PushMoveHandles((next_parameter->object != nullptr)
                           ? Kernel::g_handle_table.Create(next_parameter->object).Unwrap()
                           : 0);

    rb.PushStaticBuffer(buffer, next_parameter->buffer.size(), 0);

    Memory::WriteBlock(buffer, next_parameter->buffer.data(), next_parameter->buffer.size());
}

void GlanceParameter(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xE, 2, 0); // 0xE0080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 buffer_size = rp.Pop<u32>();

    size_t static_buff_size;
    VAddr buffer = rp.PeekStaticBuffer(0, &static_buff_size);
    if (buffer_size > static_buff_size)
        LOG_WARNING(
            Service_APT,
            "buffer_size is bigger than the size in the buffer descriptor (0x%08X > 0x%08zX)",
            buffer_size, static_buff_size);

    LOG_DEBUG(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", static_cast<u32>(app_id),
              buffer_size);

    auto next_parameter = applet_manager->GlanceParameter(app_id);

    if (next_parameter.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(next_parameter.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 4);
    rb.Push(RESULT_SUCCESS); // No error
    rb.PushEnum(next_parameter->sender_id);
    rb.PushEnum(next_parameter->signal); // Signal type
    ASSERT_MSG(next_parameter->buffer.size() <= buffer_size, "Input static buffer is too small !");
    rb.Push(static_cast<u32>(next_parameter->buffer.size())); // Parameter buffer size

    rb.PushMoveHandles((next_parameter->object != nullptr)
                           ? Kernel::g_handle_table.Create(next_parameter->object).Unwrap()
                           : 0);

    rb.PushStaticBuffer(buffer, next_parameter->buffer.size(), 0);

    Memory::WriteBlock(buffer, next_parameter->buffer.data(), next_parameter->buffer.size());
}

void CancelParameter(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xF, 4, 0); // 0xF0100

    bool check_sender = rp.Pop<bool>();
    AppletId sender_appid = rp.PopEnum<AppletId>();
    bool check_receiver = rp.Pop<bool>();
    AppletId receiver_appid = rp.PopEnum<AppletId>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(applet_manager->CancelParameter(check_sender, sender_appid, check_receiver,
                                            receiver_appid));

    LOG_DEBUG(Service_APT, "called check_sender=%u, sender_appid=0x%08X, "
                           "check_receiver=%u, receiver_appid=0x%08X",
              check_sender, static_cast<u32>(sender_appid), check_receiver,
              static_cast<u32>(receiver_appid));
}

void PrepareToStartApplication(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x15, 5, 0); // 0x00150140
    u32 title_info1 = rp.Pop<u32>();
    u32 title_info2 = rp.Pop<u32>();
    u32 title_info3 = rp.Pop<u32>();
    u32 title_info4 = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();

    if (flags & 0x00000100) {
        unknown_ns_state_field = 1;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called title_info1=0x%08X, title_info2=0x%08X, title_info3=0x%08X,"
                "title_info4=0x%08X, flags=0x%08X",
                title_info1, title_info2, title_info3, title_info4, flags);
}

void StartApplication(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1B, 3, 4); // 0x001B00C4
    u32 buffer1_size = rp.Pop<u32>();
    u32 buffer2_size = rp.Pop<u32>();
    u32 flag = rp.Pop<u32>();
    size_t size1;
    VAddr buffer1_ptr = rp.PopStaticBuffer(&size1);
    size_t size2;
    VAddr buffer2_ptr = rp.PopStaticBuffer(&size2);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called buffer1_size=0x%08X, buffer2_size=0x%08X, flag=0x%08X,"
                "size1=0x%08zX, buffer1_ptr=0x%08X, size2=0x%08zX, buffer2_ptr=0x%08X",
                buffer1_size, buffer2_size, flag, size1, buffer1_ptr, size2, buffer2_ptr);
}

void AppletUtility(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x4B, 3, 2); // 0x004B00C2

    // These are from 3dbrew - I'm not really sure what they're used for.
    u32 utility_command = rp.Pop<u32>();
    u32 input_size = rp.Pop<u32>();
    u32 output_size = rp.Pop<u32>();
    VAddr input_addr = rp.PopStaticBuffer(nullptr);

    VAddr output_addr = rp.PeekStaticBuffer(0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called command=0x%08X, input_size=0x%08X, output_size=0x%08X, "
                "input_addr=0x%08X, output_addr=0x%08X",
                utility_command, input_size, output_size, input_addr, output_addr);
}

void SetAppCpuTimeLimit(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x4F, 2, 0); // 0x4F0080
    u32 value = rp.Pop<u32>();
    cpu_percent = rp.Pop<u32>();

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT, "(STUBBED) called cpu_percent=%u, value=%u", cpu_percent, value);
}

void GetAppCpuTimeLimit(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x50, 1, 0); // 0x500040
    u32 value = rp.Pop<u32>();

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(cpu_percent);

    LOG_WARNING(Service_APT, "(STUBBED) called value=%u", value);
}

void PrepareToStartLibraryApplet(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x18, 1, 0); // 0x180040
    AppletId applet_id = static_cast<AppletId>(rp.Pop<u32>());

    LOG_DEBUG(Service_APT, "called applet_id=%08X", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(applet_manager->PrepareToStartLibraryApplet(applet_id));
}

void PrepareToStartNewestHomeMenu(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 0, 0); // 0x1A0000
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    // TODO(Subv): This command can only be called by a System Applet (return 0xC8A0CC04 otherwise).

    // This command must return an error when called, otherwise the Home Menu will try to reboot the
    // system.
    rb.Push(ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                       ErrorSummary::InvalidState, ErrorLevel::Status));

    LOG_DEBUG(Service_APT, "called");
}

void PreloadLibraryApplet(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x16, 1, 0); // 0x160040
    AppletId applet_id = static_cast<AppletId>(rp.Pop<u32>());

    LOG_DEBUG(Service_APT, "called applet_id=%08X", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(applet_manager->PreloadLibraryApplet(applet_id));
}

void FinishPreloadingLibraryApplet(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x17, 1, 0); // 0x00170040
    AppletId applet_id = static_cast<AppletId>(rp.Pop<u32>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(applet_manager->FinishPreloadingLibraryApplet(applet_id));

    LOG_WARNING(Service_APT, "(STUBBED) called applet_id=%03X", static_cast<u32>(applet_id));
}

void StartLibraryApplet(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1E, 2, 4); // 0x1E0084
    AppletId applet_id = static_cast<AppletId>(rp.Pop<u32>());

    size_t buffer_size = rp.Pop<u32>();
    Kernel::Handle handle = rp.PopHandle();
    VAddr buffer_addr = rp.PopStaticBuffer(nullptr);

    LOG_DEBUG(Service_APT, "called applet_id=%08X", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    std::vector<u8> buffer(buffer_size);
    Memory::ReadBlock(buffer_addr, buffer.data(), buffer.size());

    rb.Push(applet_manager->StartLibraryApplet(applet_id, Kernel::g_handle_table.GetGeneric(handle),
                                               buffer));
}

void CancelLibraryApplet(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x3B, 1, 0); // 0x003B0040
    bool exiting = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push<u32>(1); // TODO: Find the return code meaning

    LOG_WARNING(Service_APT, "(STUBBED) called exiting=%d", exiting);
}

void SetScreenCapPostPermission(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x55, 1, 0); // 0x00550040

    screen_capture_post_permission = static_cast<ScreencapPostPermission>(rp.Pop<u32>() & 0xF);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error
    LOG_WARNING(Service_APT, "(STUBBED) screen_capture_post_permission=%u",
                static_cast<u32>(screen_capture_post_permission));
}

void GetScreenCapPostPermission(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x56, 0, 0); // 0x00560000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(static_cast<u32>(screen_capture_post_permission));
    LOG_WARNING(Service_APT, "(STUBBED) screen_capture_post_permission=%u",
                static_cast<u32>(screen_capture_post_permission));
}

void GetAppletInfo(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x6, 1, 0); // 0x60040
    auto app_id = static_cast<AppletId>(rp.Pop<u32>());

    LOG_DEBUG(Service_APT, "called appid=%u", static_cast<u32>(app_id));

    auto info = applet_manager->GetAppletInfo(app_id);
    if (info.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(info.Code());
    } else {
        IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);
        rb.Push(RESULT_SUCCESS);
        rb.Push(info->title_id);
        rb.Push(static_cast<u8>(info->media_type));
        rb.Push(info->registered);
        rb.Push(info->loaded);
        rb.Push(info->attributes);
    }
}

void GetStartupArgument(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x51, 2, 0); // 0x00510080
    u32 parameter_size = rp.Pop<u32>();
    StartupArgumentType startup_argument_type = static_cast<StartupArgumentType>(rp.Pop<u8>());

    if (parameter_size >= 0x300) {
        LOG_ERROR(
            Service_APT,
            "Parameter size is outside the valid range (capped to 0x300): parameter_size=0x%08x",
            parameter_size);
        return;
    }

    size_t static_buff_size;
    VAddr addr = rp.PeekStaticBuffer(0, &static_buff_size);
    if (parameter_size > static_buff_size)
        LOG_WARNING(
            Service_APT,
            "parameter_size is bigger than the size in the buffer descriptor (0x%08X > 0x%08zX)",
            parameter_size, static_buff_size);

    if (addr && parameter_size) {
        Memory::ZeroBlock(addr, parameter_size);
    }

    LOG_WARNING(Service_APT, "(stubbed) called startup_argument_type=%u , parameter_size=0x%08x",
                static_cast<u32>(startup_argument_type), parameter_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    rb.PushStaticBuffer(addr, parameter_size, 0);
}

void Wrap(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x46, 4, 4);
    const u32 output_size = rp.Pop<u32>();
    const u32 input_size = rp.Pop<u32>();
    const u32 nonce_offset = rp.Pop<u32>();
    u32 nonce_size = rp.Pop<u32>();
    size_t desc_size;
    IPC::MappedBufferPermissions desc_permission;
    const VAddr input = rp.PopMappedBuffer(&desc_size, &desc_permission);
    ASSERT(desc_size == input_size && desc_permission == IPC::MappedBufferPermissions::R);
    const VAddr output = rp.PopMappedBuffer(&desc_size, &desc_permission);
    ASSERT(desc_size == output_size && desc_permission == IPC::MappedBufferPermissions::W);

    // Note: real 3DS still returns SUCCESS when the sizes don't match. It seems that it doesn't
    // check the buffer size and writes data with potential overflow.
    ASSERT_MSG(output_size == input_size + HW::AES::CCM_MAC_SIZE,
               "input_size (%d) doesn't match to output_size (%d)", input_size, output_size);

    LOG_DEBUG(Service_APT, "called, output_size=%u, input_size=%u, nonce_offset=%u, nonce_size=%u",
              output_size, input_size, nonce_offset, nonce_size);

    // Note: This weird nonce size modification is verified against real 3DS
    nonce_size = std::min<u32>(nonce_size & ~3, HW::AES::CCM_NONCE_SIZE);

    // Reads nonce and concatenates the rest of the input as plaintext
    HW::AES::CCMNonce nonce{};
    Memory::ReadBlock(input + nonce_offset, nonce.data(), nonce_size);
    u32 pdata_size = input_size - nonce_size;
    std::vector<u8> pdata(pdata_size);
    Memory::ReadBlock(input, pdata.data(), nonce_offset);
    Memory::ReadBlock(input + nonce_offset + nonce_size, pdata.data() + nonce_offset,
                      pdata_size - nonce_offset);

    // Encrypts the plaintext using AES-CCM
    auto cipher = HW::AES::EncryptSignCCM(pdata, nonce, HW::AES::KeySlotID::APTWrap);

    // Puts the nonce to the beginning of the output, with ciphertext followed
    Memory::WriteBlock(output, nonce.data(), nonce_size);
    Memory::WriteBlock(output + nonce_size, cipher.data(), cipher.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);

    // Unmap buffer
    rb.PushMappedBuffer(input, input_size, IPC::MappedBufferPermissions::R);
    rb.PushMappedBuffer(output, output_size, IPC::MappedBufferPermissions::W);
}

void Unwrap(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x47, 4, 4);
    const u32 output_size = rp.Pop<u32>();
    const u32 input_size = rp.Pop<u32>();
    const u32 nonce_offset = rp.Pop<u32>();
    u32 nonce_size = rp.Pop<u32>();
    size_t desc_size;
    IPC::MappedBufferPermissions desc_permission;
    const VAddr input = rp.PopMappedBuffer(&desc_size, &desc_permission);
    ASSERT(desc_size == input_size && desc_permission == IPC::MappedBufferPermissions::R);
    const VAddr output = rp.PopMappedBuffer(&desc_size, &desc_permission);
    ASSERT(desc_size == output_size && desc_permission == IPC::MappedBufferPermissions::W);

    // Note: real 3DS still returns SUCCESS when the sizes don't match. It seems that it doesn't
    // check the buffer size and writes data with potential overflow.
    ASSERT_MSG(output_size == input_size - HW::AES::CCM_MAC_SIZE,
               "input_size (%d) doesn't match to output_size (%d)", input_size, output_size);

    LOG_DEBUG(Service_APT, "called, output_size=%u, input_size=%u, nonce_offset=%u, nonce_size=%u",
              output_size, input_size, nonce_offset, nonce_size);

    // Note: This weird nonce size modification is verified against real 3DS
    nonce_size = std::min<u32>(nonce_size & ~3, HW::AES::CCM_NONCE_SIZE);

    // Reads nonce and cipher text
    HW::AES::CCMNonce nonce{};
    Memory::ReadBlock(input, nonce.data(), nonce_size);
    u32 cipher_size = input_size - nonce_size;
    std::vector<u8> cipher(cipher_size);
    Memory::ReadBlock(input + nonce_size, cipher.data(), cipher_size);

    // Decrypts the ciphertext using AES-CCM
    auto pdata = HW::AES::DecryptVerifyCCM(cipher, nonce, HW::AES::KeySlotID::APTWrap);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    if (!pdata.empty()) {
        // Splits the plaintext and put the nonce in between
        Memory::WriteBlock(output, pdata.data(), nonce_offset);
        Memory::WriteBlock(output + nonce_offset, nonce.data(), nonce_size);
        Memory::WriteBlock(output + nonce_offset + nonce_size, pdata.data() + nonce_offset,
                           pdata.size() - nonce_offset);
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_APT, "Failed to decrypt data");
        rb.Push(ResultCode(static_cast<ErrorDescription>(1), ErrorModule::PS,
                           ErrorSummary::WrongArgument, ErrorLevel::Status));
    }

    // Unmap buffer
    rb.PushMappedBuffer(input, input_size, IPC::MappedBufferPermissions::R);
    rb.PushMappedBuffer(output, output_size, IPC::MappedBufferPermissions::W);
}

void CheckNew3DSApp(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x101, 0, 0); // 0x01010000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (unknown_ns_state_field) {
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
    } else {
        PTM::CheckNew3DS(rb);
    }

    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void CheckNew3DS(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x102, 0, 0); // 0x01020000
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    PTM::CheckNew3DS(rb);

    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void Init() {
    AddService(new APT_A_Interface);
    AddService(new APT_S_Interface);
    AddService(new APT_U_Interface);

    applet_manager = std::make_shared<AppletManager>();

    using Kernel::MemoryPermission;
    shared_font_mem =
        Kernel::SharedMemory::Create(nullptr, 0x332000, // 3272 KB
                                     MemoryPermission::ReadWrite, MemoryPermission::Read, 0,
                                     Kernel::MemoryRegion::SYSTEM, "APT:SharedFont");

    lock = Kernel::Mutex::Create(false, "APT_U:Lock");

    cpu_percent = 0;
    unknown_ns_state_field = 0;
    screen_capture_post_permission =
        ScreencapPostPermission::CleanThePermission; // TODO(JamePeng): verify the initial value
}

void Shutdown() {
    shared_font_mem = nullptr;
    shared_font_loaded = false;
    shared_font_relocated = false;
    lock = nullptr;

    applet_manager = nullptr;
}

} // namespace APT
} // namespace Service
