// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/mutex.h"
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

namespace Service::APT {

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2, 2, 0); // 0x20080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 attributes = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called app_id={:#010X}, attributes={:#010X}", static_cast<u32>(app_id),
              attributes);

    auto result = apt->applet_manager->Initialize(app_id, attributes);
    if (result.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(result.Code());
    } else {
        auto events = std::move(result).Unwrap();
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(events.notification_event, events.parameter_event);
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

bool Module::LoadSharedFont() {
    u8 font_region_code;
    auto cfg = Service::CFG::GetModule(Core::System::GetInstance());
    ASSERT_MSG(cfg, "CFG Module missing!");
    switch (cfg->GetRegionValue()) {
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

    FileSys::NCCHArchive archive(shared_font_archive_id_low, Service::FS::MediaType::NAND);
    std::vector<u8> romfs_path(20, 0); // 20-byte all zero path for opening RomFS
    FileSys::Path file_path(romfs_path);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode);
    if (file_result.Failed())
        return false;

    auto romfs = std::move(file_result).Unwrap();
    std::vector<u8> romfs_buffer(romfs->GetSize());
    romfs->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->Close();

    const char16_t* file_name[4] = {u"cbf_std.bcfnt.lz", u"cbf_zh-Hans-CN.bcfnt.lz",
                                    u"cbf_ko-Hang-KR.bcfnt.lz", u"cbf_zh-Hant-TW.bcfnt.lz"};
    const RomFS::RomFSFile font_file =
        RomFS::GetFile(romfs_buffer.data(), {file_name[font_region_code - 1]});
    if (font_file.Data() == nullptr)
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
        DecompressLZ11(font_file.Data(), shared_font_mem->GetPointer(0x80));
    std::memcpy(shared_font_mem->GetPointer(), &shared_font_header, sizeof(shared_font_header));
    *shared_font_mem->GetPointer(0x83) = 'U'; // Change the magic from "CFNT" to "CFNU"

    return true;
}

bool Module::LoadLegacySharedFont() {
    // This is the legacy method to load shared font.
    // The expected format is a decrypted, uncompressed BCFNT file with the 0x80 byte header
    // generated by the APT:U service. The best way to get is by dumping it from RAM. We've provided
    // a homebrew app to do this: https://github.com/citra-emu/3dsutils. Put the resulting file
    // "shared_font.bin" in the Citra "sysdata" directory.
    std::string filepath = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + SHARED_FONT;

    FileUtil::CreateFullPath(filepath); // Create path if not already created
    FileUtil::IOFile file(filepath, "rb");
    if (file.IsOpen()) {
        file.ReadBytes(shared_font_mem->GetPointer(), file.GetSize());
        return true;
    }

    return false;
}

void Module::Interface::GetSharedFont(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x44, 0, 0); // 0x00440000
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    // Log in telemetry if the game uses the shared font
    apt->system.TelemetrySession().AddField(Telemetry::FieldType::Session, "RequiresSharedFont",
                                            true);

    if (!apt->shared_font_loaded) {
        // On real 3DS, font loading happens on booting. However, we load it on demand to coordinate
        // with CFG region auto configuration, which happens later than APT initialization.
        if (apt->LoadSharedFont()) {
            apt->shared_font_loaded = true;
        } else if (apt->LoadLegacySharedFont()) {
            LOG_WARNING(Service_APT, "Loaded shared font by legacy method");
            apt->shared_font_loaded = true;
        } else {
            LOG_ERROR(Service_APT, "shared font file missing - go dump it from your 3ds");
            rb.Push<u32>(-1); // TODO: Find the right error code
            rb.Push<u32>(0);
            rb.PushCopyObjects<Kernel::Object>(nullptr);
            apt->system.SetStatus(Core::System::ResultStatus::ErrorSystemFiles, "Shared fonts");
            return;
        }
    }

    // The shared font has to be relocated to the new address before being passed to the
    // application.
    auto maybe_vaddr =
        Memory::PhysicalToVirtualAddress(apt->shared_font_mem->linear_heap_phys_address);
    ASSERT(maybe_vaddr);
    VAddr target_address = *maybe_vaddr;
    if (!apt->shared_font_relocated) {
        BCFNT::RelocateSharedFont(apt->shared_font_mem, target_address);
        apt->shared_font_relocated = true;
    }

    rb.Push(RESULT_SUCCESS); // No error
    // Since the SharedMemory interface doesn't provide the address at which the memory was
    // allocated, the real APT service calculates this address by scanning the entire address space
    // (using svcQueryMemory) and searches for an allocation of the same size as the Shared Font.
    rb.Push(target_address);
    rb.PushCopyObjects(apt->shared_font_mem);
}

void Module::Interface::NotifyToWait(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x43, 1, 0); // 0x430040
    u32 app_id = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id={}", app_id);
}

void Module::Interface::GetLockHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 1, 0); // 0x10040

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
    rb.PushCopyObjects(apt->lock);

    LOG_WARNING(Service_APT, "(STUBBED) called applet_attributes={:#010X}", applet_attributes);
}

void Module::Interface::Enable(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3, 1, 0); // 0x30040
    u32 attributes = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called attributes={:#010X}", attributes);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->Enable(attributes));
}

void Module::Interface::GetAppletManInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 1, 0); // 0x50040
    u32 unk = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push(static_cast<u32>(AppletId::HomeMenu));    // Home menu AppID
    rb.Push(static_cast<u32>(AppletId::Application)); // TODO(purpasmart96): Do this correctly

    LOG_WARNING(Service_APT, "(STUBBED) called unk={:#010X}", unk);
}

void Module::Interface::IsRegistered(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 1, 0); // 0x90040
    AppletId app_id = rp.PopEnum<AppletId>();
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(apt->applet_manager->IsRegistered(app_id));

    LOG_DEBUG(Service_APT, "called app_id={:#010X}", static_cast<u32>(app_id));
}

void Module::Interface::InquireNotification(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 1, 0); // 0xB0040
    u32 app_id = rp.Pop<u32>();
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);                     // No error
    rb.Push(static_cast<u32>(SignalType::None)); // Signal type
    LOG_WARNING(Service_APT, "(STUBBED) called app_id={:#010X}", app_id);
}

void Module::Interface::SendParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xC, 4, 4); // 0xC0104
    AppletId src_app_id = rp.PopEnum<AppletId>();
    AppletId dst_app_id = rp.PopEnum<AppletId>();
    SignalType signal_type = rp.PopEnum<SignalType>();
    u32 buffer_size = rp.Pop<u32>();
    Kernel::SharedPtr<Kernel::Object> object = rp.PopGenericObject();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_DEBUG(Service_APT,
              "called src_app_id={:#010X}, dst_app_id={:#010X}, signal_type={:#010X},"
              "buffer_size={:#010X}",
              static_cast<u32>(src_app_id), static_cast<u32>(dst_app_id),
              static_cast<u32>(signal_type), buffer_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    MessageParameter param;
    param.destination_id = dst_app_id;
    param.sender_id = src_app_id;
    param.object = std::move(object);
    param.signal = signal_type;
    param.buffer = std::move(buffer);

    rb.Push(apt->applet_manager->SendParameter(param));
}

void Module::Interface::ReceiveParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xD, 2, 0); // 0xD0080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 buffer_size = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called app_id={:#010X}, buffer_size={:#010X}", static_cast<u32>(app_id),
              buffer_size);

    auto next_parameter = apt->applet_manager->ReceiveParameter(app_id);

    if (next_parameter.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(next_parameter.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 4);

    rb.Push(RESULT_SUCCESS); // No error
    rb.PushEnum(next_parameter->sender_id);
    rb.PushEnum(next_parameter->signal); // Signal type
    ASSERT_MSG(next_parameter->buffer.size() <= buffer_size, "Input static buffer is too small!");
    rb.Push(static_cast<u32>(next_parameter->buffer.size())); // Parameter buffer size
    rb.PushMoveObjects(next_parameter->object);
    next_parameter->buffer.resize(buffer_size, 0); // APT always push a buffer with the maximum size
    rb.PushStaticBuffer(next_parameter->buffer, 0);
}

void Module::Interface::GlanceParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xE, 2, 0); // 0xE0080
    AppletId app_id = rp.PopEnum<AppletId>();
    u32 buffer_size = rp.Pop<u32>();

    LOG_DEBUG(Service_APT, "called app_id={:#010X}, buffer_size={:#010X}", static_cast<u32>(app_id),
              buffer_size);

    auto next_parameter = apt->applet_manager->GlanceParameter(app_id);

    if (next_parameter.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(next_parameter.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 4);
    rb.Push(RESULT_SUCCESS); // No error
    rb.PushEnum(next_parameter->sender_id);
    rb.PushEnum(next_parameter->signal); // Signal type
    ASSERT_MSG(next_parameter->buffer.size() <= buffer_size, "Input static buffer is too small!");
    rb.Push(static_cast<u32>(next_parameter->buffer.size())); // Parameter buffer size
    rb.PushMoveObjects(next_parameter->object);
    next_parameter->buffer.resize(buffer_size, 0); // APT always push a buffer with the maximum size
    rb.PushStaticBuffer(next_parameter->buffer, 0);
}

void Module::Interface::CancelParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xF, 4, 0); // 0xF0100

    bool check_sender = rp.Pop<bool>();
    AppletId sender_appid = rp.PopEnum<AppletId>();
    bool check_receiver = rp.Pop<bool>();
    AppletId receiver_appid = rp.PopEnum<AppletId>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(apt->applet_manager->CancelParameter(check_sender, sender_appid, check_receiver,
                                                 receiver_appid));

    LOG_DEBUG(Service_APT,
              "called check_sender={}, sender_appid={:#010X}, "
              "check_receiver={}, receiver_appid={:#010X}",
              check_sender, static_cast<u32>(sender_appid), check_receiver,
              static_cast<u32>(receiver_appid));
}

void Module::Interface::PrepareToDoApplicationJump(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x31, 4, 0); // 0x00310100
    auto flags = rp.PopEnum<ApplicationJumpFlags>();
    u64 title_id = rp.Pop<u64>();
    u8 media_type = rp.Pop<u8>();

    LOG_WARNING(Service_APT, "(STUBBED) called title_id={:016X}, media_type={:#01X}, flags={:#08X}",
                title_id, media_type, static_cast<u8>(flags));

    ResultCode result = apt->applet_manager->PrepareToDoApplicationJump(
        title_id, static_cast<FS::MediaType>(media_type), flags);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::DoApplicationJump(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x32, 2, 4); // 0x00320084
    u32 param_size = rp.Pop<u32>();
    u32 hmac_size = rp.Pop<u32>();

    auto param = rp.PopStaticBuffer();
    auto hmac = rp.PopStaticBuffer();

    LOG_WARNING(Service_APT, "(STUBBED) called param_size={:08X}, hmac_size={:08X}", param_size,
                hmac_size);

    // TODO(Subv): Set the delivery parameters before starting the new application.

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->DoApplicationJump());
}

void Module::Interface::GetProgramIdOnApplicationJump(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x33, 0, 0); // 0x00330000

    LOG_DEBUG(Service_APT, "called");

    auto parameters = apt->applet_manager->GetApplicationJumpParameters();

    IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(parameters.current_title_id);
    rb.Push(static_cast<u8>(parameters.current_media_type));
    rb.Push<u64>(parameters.next_title_id);
    rb.Push(static_cast<u8>(parameters.next_media_type));
}

void Module::Interface::PrepareToStartApplication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 5, 0); // 0x00150140
    u32 title_info1 = rp.Pop<u32>();
    u32 title_info2 = rp.Pop<u32>();
    u32 title_info3 = rp.Pop<u32>();
    u32 title_info4 = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();

    if (flags & 0x00000100) {
        apt->unknown_ns_state_field = 1;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called title_info1={:#010X}, title_info2={:#010X}, title_info3={:#010X},"
                "title_info4={:#010X}, flags={:#010X}",
                title_info1, title_info2, title_info3, title_info4, flags);
}

void Module::Interface::StartApplication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 3, 4); // 0x001B00C4
    u32 buffer1_size = rp.Pop<u32>();
    u32 buffer2_size = rp.Pop<u32>();
    u32 flag = rp.Pop<u32>();
    std::vector<u8> buffer1 = rp.PopStaticBuffer();
    std::vector<u8> buffer2 = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called buffer1_size={:#010X}, buffer2_size={:#010X}, flag={:#010X}",
                buffer1_size, buffer2_size, flag);
}

void Module::Interface::AppletUtility(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x4B, 3, 2); // 0x004B00C2

    // These are from 3dbrew - I'm not really sure what they're used for.
    u32 utility_command = rp.Pop<u32>();
    u32 input_size = rp.Pop<u32>();
    u32 output_size = rp.Pop<u32>();
    std::vector<u8> input = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT,
                "(STUBBED) called command={:#010X}, input_size={:#010X}, output_size={:#010X}",
                utility_command, input_size, output_size);
}

void Module::Interface::SetAppCpuTimeLimit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x4F, 2, 0); // 0x4F0080
    u32 value = rp.Pop<u32>();
    apt->cpu_percent = rp.Pop<u32>();

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually {}!", value);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error

    LOG_WARNING(Service_APT, "(STUBBED) called, cpu_percent={}, value={}", apt->cpu_percent, value);
}

void Module::Interface::GetAppCpuTimeLimit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x50, 1, 0); // 0x500040
    u32 value = rp.Pop<u32>();

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually {}!", value);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(apt->cpu_percent);

    LOG_WARNING(Service_APT, "(STUBBED) called, value={}", value);
}

void Module::Interface::PrepareToStartLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 1, 0); // 0x180040
    AppletId applet_id = rp.PopEnum<AppletId>();

    LOG_DEBUG(Service_APT, "called, applet_id={:08X}", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->PrepareToStartLibraryApplet(applet_id));
}

void Module::Interface::PrepareToStartNewestHomeMenu(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 0, 0); // 0x1A0000
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    // TODO(Subv): This command can only be called by a System Applet (return 0xC8A0CC04 otherwise).

    // This command must return an error when called, otherwise the Home Menu will try to reboot the
    // system.
    rb.Push(ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                       ErrorSummary::InvalidState, ErrorLevel::Status));

    LOG_DEBUG(Service_APT, "called");
}

void Module::Interface::PreloadLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 1, 0); // 0x160040
    AppletId applet_id = rp.PopEnum<AppletId>();

    LOG_DEBUG(Service_APT, "called, applet_id={:08X}", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->PreloadLibraryApplet(applet_id));
}

void Module::Interface::FinishPreloadingLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 1, 0); // 0x00170040
    AppletId applet_id = rp.PopEnum<AppletId>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->FinishPreloadingLibraryApplet(applet_id));

    LOG_WARNING(Service_APT, "(STUBBED) called, applet_id={:#05X}", static_cast<u32>(applet_id));
}

void Module::Interface::StartLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 2, 4); // 0x1E0084
    AppletId applet_id = rp.PopEnum<AppletId>();

    std::size_t buffer_size = rp.Pop<u32>();
    Kernel::SharedPtr<Kernel::Object> object = rp.PopGenericObject();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_DEBUG(Service_APT, "called, applet_id={:08X}", static_cast<u32>(applet_id));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->StartLibraryApplet(applet_id, object, buffer));
}

void Module::Interface::CloseApplication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x27, 1, 4);
    u32 parameters_size = rp.Pop<u32>();
    Kernel::SharedPtr<Kernel::Object> object = rp.PopGenericObject();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_DEBUG(Service_APT, "called");

    apt->system.RequestShutdown();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::CancelLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3B, 1, 0); // 0x003B0040
    bool exiting = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push<u32>(1); // TODO: Find the return code meaning

    LOG_WARNING(Service_APT, "(STUBBED) called exiting={}", exiting);
}

void Module::Interface::PrepareToCloseLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x25, 3, 0); // 0x002500C0
    bool not_pause = rp.Pop<bool>();
    bool exiting = rp.Pop<bool>();
    bool jump_to_home = rp.Pop<bool>();

    LOG_DEBUG(Service_APT, "called not_pause={} exiting={} jump_to_home={}", not_pause, exiting,
              jump_to_home);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->PrepareToCloseLibraryApplet(not_pause, exiting, jump_to_home));
}

void Module::Interface::CloseLibraryApplet(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x28, 1, 4); // 0x00280044
    u32 parameter_size = rp.Pop<u32>();
    auto object = rp.PopGenericObject();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_DEBUG(Service_APT, "called size={}", parameter_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(apt->applet_manager->CloseLibraryApplet(std::move(object), std::move(buffer)));
}

void Module::Interface::SendCaptureBufferInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x40, 1, 2); // 0x00400042
    u32 size = rp.Pop<u32>();
    ASSERT(size == 0x20);
    apt->screen_capture_buffer = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::ReceiveCaptureBufferInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x41, 1, 0); // 0x00410040
    u32 size = rp.Pop<u32>();
    ASSERT(size == 0x20);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(apt->screen_capture_buffer.size()));
    rb.PushStaticBuffer(std::move(apt->screen_capture_buffer), 0);
}

void Module::Interface::SetScreenCapPostPermission(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x55, 1, 0); // 0x00550040

    apt->screen_capture_post_permission = static_cast<ScreencapPostPermission>(rp.Pop<u32>() & 0xF);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS); // No error
    LOG_WARNING(Service_APT, "(STUBBED) called, screen_capture_post_permission={}",
                static_cast<u32>(apt->screen_capture_post_permission));
}

void Module::Interface::GetScreenCapPostPermission(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x56, 0, 0); // 0x00560000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(static_cast<u32>(apt->screen_capture_post_permission));
    LOG_WARNING(Service_APT, "(STUBBED) called, screen_capture_post_permission={}",
                static_cast<u32>(apt->screen_capture_post_permission));
}

void Module::Interface::GetAppletInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x6, 1, 0); // 0x60040
    auto app_id = rp.PopEnum<AppletId>();

    LOG_DEBUG(Service_APT, "called, app_id={}", static_cast<u32>(app_id));

    auto info = apt->applet_manager->GetAppletInfo(app_id);
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

void Module::Interface::GetStartupArgument(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x51, 2, 0); // 0x00510080
    u32 parameter_size = rp.Pop<u32>();
    StartupArgumentType startup_argument_type = static_cast<StartupArgumentType>(rp.Pop<u8>());

    const u32 max_parameter_size{0x1000};

    if (parameter_size > max_parameter_size) {
        LOG_ERROR(Service_APT,
                  "Parameter size is outside the valid range (capped to {:#010X}): "
                  "parameter_size={:#010X}",
                  max_parameter_size, parameter_size);
        parameter_size = max_parameter_size;
    }

    std::vector<u8> parameter(parameter_size, 0);

    LOG_WARNING(Service_APT, "(STUBBED) called, startup_argument_type={}, parameter_size={:#010X}",
                static_cast<u32>(startup_argument_type), parameter_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    rb.PushStaticBuffer(parameter, 0);
}

void Module::Interface::Wrap(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x46, 4, 4);
    const u32 output_size = rp.Pop<u32>();
    const u32 input_size = rp.Pop<u32>();
    const u32 nonce_offset = rp.Pop<u32>();
    u32 nonce_size = rp.Pop<u32>();
    auto& input = rp.PopMappedBuffer();
    ASSERT(input.GetSize() == input_size);
    auto& output = rp.PopMappedBuffer();
    ASSERT(output.GetSize() == output_size);

    // Note: real 3DS still returns SUCCESS when the sizes don't match. It seems that it doesn't
    // check the buffer size and writes data with potential overflow.
    ASSERT_MSG(output_size == input_size + HW::AES::CCM_MAC_SIZE,
               "input_size ({}) doesn't match to output_size ({})", input_size, output_size);

    LOG_DEBUG(Service_APT, "called, output_size={}, input_size={}, nonce_offset={}, nonce_size={}",
              output_size, input_size, nonce_offset, nonce_size);

    // Note: This weird nonce size modification is verified against real 3DS
    nonce_size = std::min<u32>(nonce_size & ~3, HW::AES::CCM_NONCE_SIZE);

    // Reads nonce and concatenates the rest of the input as plaintext
    HW::AES::CCMNonce nonce{};
    input.Read(nonce.data(), nonce_offset, nonce_size);
    u32 pdata_size = input_size - nonce_size;
    std::vector<u8> pdata(pdata_size);
    input.Read(pdata.data(), 0, nonce_offset);
    input.Read(pdata.data() + nonce_offset, nonce_offset + nonce_size, pdata_size - nonce_offset);

    // Encrypts the plaintext using AES-CCM
    auto cipher = HW::AES::EncryptSignCCM(pdata, nonce, HW::AES::KeySlotID::APTWrap);

    // Puts the nonce to the beginning of the output, with ciphertext followed
    output.Write(nonce.data(), 0, nonce_size);
    output.Write(cipher.data(), nonce_size, cipher.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);

    // Unmap buffer
    rb.PushMappedBuffer(input);
    rb.PushMappedBuffer(output);
}

void Module::Interface::Unwrap(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x47, 4, 4);
    const u32 output_size = rp.Pop<u32>();
    const u32 input_size = rp.Pop<u32>();
    const u32 nonce_offset = rp.Pop<u32>();
    u32 nonce_size = rp.Pop<u32>();
    auto& input = rp.PopMappedBuffer();
    ASSERT(input.GetSize() == input_size);
    auto& output = rp.PopMappedBuffer();
    ASSERT(output.GetSize() == output_size);

    // Note: real 3DS still returns SUCCESS when the sizes don't match. It seems that it doesn't
    // check the buffer size and writes data with potential overflow.
    ASSERT_MSG(output_size == input_size - HW::AES::CCM_MAC_SIZE,
               "input_size ({}) doesn't match to output_size ({})", input_size, output_size);

    LOG_DEBUG(Service_APT, "called, output_size={}, input_size={}, nonce_offset={}, nonce_size={}",
              output_size, input_size, nonce_offset, nonce_size);

    // Note: This weird nonce size modification is verified against real 3DS
    nonce_size = std::min<u32>(nonce_size & ~3, HW::AES::CCM_NONCE_SIZE);

    // Reads nonce and cipher text
    HW::AES::CCMNonce nonce{};
    input.Read(nonce.data(), 0, nonce_size);
    u32 cipher_size = input_size - nonce_size;
    std::vector<u8> cipher(cipher_size);
    input.Read(cipher.data(), nonce_size, cipher_size);

    // Decrypts the ciphertext using AES-CCM
    auto pdata = HW::AES::DecryptVerifyCCM(cipher, nonce, HW::AES::KeySlotID::APTWrap);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    if (!pdata.empty()) {
        // Splits the plaintext and put the nonce in between
        output.Write(pdata.data(), 0, nonce_offset);
        output.Write(nonce.data(), nonce_offset, nonce_size);
        output.Write(pdata.data() + nonce_offset, nonce_offset + nonce_size,
                     pdata.size() - nonce_offset);
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_APT, "Failed to decrypt data");
        rb.Push(ResultCode(static_cast<ErrorDescription>(1), ErrorModule::PS,
                           ErrorSummary::WrongArgument, ErrorLevel::Status));
    }

    // Unmap buffer
    rb.PushMappedBuffer(input);
    rb.PushMappedBuffer(output);
}

void Module::Interface::CheckNew3DSApp(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x101, 0, 0); // 0x01010000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (apt->unknown_ns_state_field) {
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
    } else {
        PTM::CheckNew3DS(rb);
    }

    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void Module::Interface::CheckNew3DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x102, 0, 0); // 0x01020000
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    PTM::CheckNew3DS(rb);

    LOG_WARNING(Service_APT, "(STUBBED) called");
}

Module::Interface::Interface(std::shared_ptr<Module> apt, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), apt(std::move(apt)) {}

Module::Interface::~Interface() = default;

Module::Module(Core::System& system) : system(system) {
    applet_manager = std::make_shared<AppletManager>(system);

    using Kernel::MemoryPermission;
    shared_font_mem =
        system.Kernel().CreateSharedMemory(nullptr, 0x332000, // 3272 KB
                                           MemoryPermission::ReadWrite, MemoryPermission::Read, 0,
                                           Kernel::MemoryRegion::SYSTEM, "APT:SharedFont");

    lock = system.Kernel().CreateMutex(false, "APT_U:Lock");
}

Module::~Module() {}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto apt = std::make_shared<Module>(system);
    std::make_shared<APT_U>(apt)->InstallAsService(service_manager);
    std::make_shared<APT_S>(apt)->InstallAsService(service_manager);
    std::make_shared<APT_A>(apt)->InstallAsService(service_manager);
}

} // namespace Service::APT
