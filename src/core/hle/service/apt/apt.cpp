// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"

#include "core/hle/applets/applet.h"
#include "core/hle/service/service.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"
#include "core/hle/service/apt/apt_s.h"
#include "core/hle/service/apt/apt_u.h"
#include "core/hle/service/apt/bcfnt/bcfnt.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/ptm/ptm.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_memory.h"

namespace Service {
namespace APT {

/// Handle to shared memory region designated to for shared system font
static Kernel::SharedPtr<Kernel::SharedMemory> shared_font_mem;
static bool shared_font_relocated = false;

static Kernel::SharedPtr<Kernel::Mutex> lock;
static Kernel::SharedPtr<Kernel::Event> notification_event; ///< APT notification event
static Kernel::SharedPtr<Kernel::Event> parameter_event; ///< APT parameter event

static u32 cpu_percent; ///< CPU time available to the running application

// APT::CheckNew3DSApp will check this unknown_ns_state_field to determine processing mode
static u8 unknown_ns_state_field;

/// Parameter data to be returned in the next call to Glance/ReceiveParameter
static MessageParameter next_parameter;

void SendParameter(const MessageParameter& parameter) {
    next_parameter = parameter;
    // Signal the event to let the application know that a new parameter is ready to be read
    parameter_event->Signal();
}

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 flags  = cmd_buff[2];

    cmd_buff[2] = IPC::MoveHandleDesc(2);
    cmd_buff[3] = Kernel::g_handle_table.Create(notification_event).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(parameter_event).MoveFrom();

    // TODO(bunnei): Check if these events are cleared every time Initialize is called.
    notification_event->Clear();
    parameter_event->Clear();

    ASSERT_MSG((nullptr != lock), "Cannot initialize without lock");
    lock->Release();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_DEBUG(Service_APT, "called app_id=0x%08X, flags=0x%08X", app_id, flags);
}

void GetSharedFont(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // The shared font has to be relocated to the new address before being passed to the application.
    VAddr target_address = Memory::PhysicalToVirtualAddress(shared_font_mem->linear_heap_phys_address);
    // The shared font dumped by 3dsutils (https://github.com/citra-emu/3dsutils) uses this address as base,
    // so we relocate it from there to our real address.
    // TODO(Subv): This address is wrong if the shared font is dumped from a n3DS,
    // we need a way to automatically calculate the original address of the font from the file.
    static const VAddr SHARED_FONT_VADDR = 0x18000000;
    if (!shared_font_relocated) {
        BCFNT::RelocateSharedFont(shared_font_mem, SHARED_FONT_VADDR, target_address);
        shared_font_relocated = true;
    }
    cmd_buff[0] = IPC::MakeHeader(0x44, 2, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    // Since the SharedMemory interface doesn't provide the address at which the memory was allocated,
    // the real APT service calculates this address by scanning the entire address space (using svcQueryMemory)
    // and searches for an allocation of the same size as the Shared Font.
    cmd_buff[2] = target_address;
    cmd_buff[3] = IPC::MoveHandleDesc();
    cmd_buff[4] = Kernel::g_handle_table.Create(shared_font_mem).MoveFrom();
}

void NotifyToWait(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id=%u", app_id);
}

void GetLockHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // Bits [0:2] are the applet type (System, Library, etc)
    // Bit 5 tells the application that there's a pending APT parameter,
    // this will cause the app to wait until parameter_event is signaled.
    u32 applet_attributes = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    cmd_buff[2] = applet_attributes; // Applet Attributes, this value is passed to Enable.
    cmd_buff[3] = 0; // Least significant bit = power button state
    cmd_buff[4] = IPC::CopyHandleDesc();
    cmd_buff[5] = Kernel::g_handle_table.Create(lock).MoveFrom();

    LOG_WARNING(Service_APT, "(STUBBED) called handle=0x%08X applet_attributes=0x%08X", cmd_buff[5], applet_attributes);
}

void Enable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 attributes = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    parameter_event->Signal(); // Let the application know that it has been started
    LOG_WARNING(Service_APT, "(STUBBED) called attributes=0x%08X", attributes);
}

void GetAppletManInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 unk = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    cmd_buff[4] = static_cast<u32>(AppletId::HomeMenu); // Home menu AppID
    cmd_buff[5] = static_cast<u32>(AppletId::Application); // TODO(purpasmart96): Do this correctly

    LOG_WARNING(Service_APT, "(STUBBED) called unk=0x%08X", unk);
}

void IsRegistered(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    // TODO(Subv): An application is considered "registered" if it has already called APT::Enable
    // handle this properly once we implement multiprocess support.
    cmd_buff[2] = 0; // Set to not registered by default

    if (app_id == static_cast<u32>(AppletId::AnyLibraryApplet)) {
        cmd_buff[2] = HLE::Applets::IsLibraryAppletRunning() ? 1 : 0;
    } else if (auto applet = HLE::Applets::Applet::Get(static_cast<AppletId>(app_id))) {
        cmd_buff[2] = 1; // Set to registered
    }
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void InquireNotification(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(SignalType::None); // Signal type
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void SendParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 src_app_id     = cmd_buff[1];
    u32 dst_app_id     = cmd_buff[2];
    u32 signal_type    = cmd_buff[3];
    u32 buffer_size    = cmd_buff[4];
    u32 value          = cmd_buff[5];
    u32 handle         = cmd_buff[6];
    u32 size           = cmd_buff[7];
    u32 buffer         = cmd_buff[8];

    std::shared_ptr<HLE::Applets::Applet> dest_applet = HLE::Applets::Applet::Get(static_cast<AppletId>(dst_app_id));

    if (dest_applet == nullptr) {
        LOG_ERROR(Service_APT, "Unknown applet id=0x%08X", dst_app_id);
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    MessageParameter param;
    param.destination_id = dst_app_id;
    param.sender_id = src_app_id;
    param.object = Kernel::g_handle_table.GetGeneric(handle);
    param.signal = signal_type;
    param.buffer.resize(buffer_size);
    Memory::ReadBlock(buffer, param.buffer.data(), param.buffer.size());

    cmd_buff[1] = dest_applet->ReceiveParameter(param).raw;

    LOG_WARNING(Service_APT, "(STUBBED) called src_app_id=0x%08X, dst_app_id=0x%08X, signal_type=0x%08X,"
               "buffer_size=0x%08X, value=0x%08X, handle=0x%08X, size=0x%08X, in_param_buffer_ptr=0x%08X",
               src_app_id, dst_app_id, signal_type, buffer_size, value, handle, size, buffer);
}

void ReceiveParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    VAddr buffer = cmd_buff[0x104 >> 2];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = next_parameter.sender_id;
    cmd_buff[3] = next_parameter.signal; // Signal type
    cmd_buff[4] = next_parameter.buffer.size(); // Parameter buffer size
    cmd_buff[5] = 0x10;
    cmd_buff[6] = 0;
    if (next_parameter.object != nullptr)
        cmd_buff[6] = Kernel::g_handle_table.Create(next_parameter.object).MoveFrom();
    cmd_buff[7] = (next_parameter.buffer.size() << 14) | 2;
    cmd_buff[8] = buffer;

    Memory::WriteBlock(buffer, next_parameter.buffer.data(), next_parameter.buffer.size());

    LOG_WARNING(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

void GlanceParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    VAddr buffer = cmd_buff[0x104 >> 2];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = next_parameter.sender_id;
    cmd_buff[3] = next_parameter.signal; // Signal type
    cmd_buff[4] = next_parameter.buffer.size(); // Parameter buffer size
    cmd_buff[5] = 0x10;
    cmd_buff[6] = 0;
    if (next_parameter.object != nullptr)
        cmd_buff[6] = Kernel::g_handle_table.Create(next_parameter.object).MoveFrom();
    cmd_buff[7] = (next_parameter.buffer.size() << 14) | 2;
    cmd_buff[8] = buffer;

    Memory::WriteBlock(buffer, next_parameter.buffer.data(), std::min(static_cast<size_t>(buffer_size), next_parameter.buffer.size()));

    LOG_WARNING(Service_APT, "called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

void CancelParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 flag1  = cmd_buff[1];
    u32 unk    = cmd_buff[2];
    u32 flag2  = cmd_buff[3];
    u32 app_id = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 1; // Set to Success

    LOG_WARNING(Service_APT, "(STUBBED) called flag1=0x%08X, unk=0x%08X, flag2=0x%08X, app_id=0x%08X",
                flag1, unk, flag2, app_id);
}

void PrepareToStartApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 title_info1  = cmd_buff[1];
    u32 title_info2  = cmd_buff[2];
    u32 title_info3  = cmd_buff[3];
    u32 title_info4  = cmd_buff[4];
    u32 flags        = cmd_buff[5];

    if (flags & 0x00000100) {
        unknown_ns_state_field = 1;
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called title_info1=0x%08X, title_info2=0x%08X, title_info3=0x%08X,"
               "title_info4=0x%08X, flags=0x%08X", title_info1, title_info2, title_info3, title_info4, flags);
}

void StartApplication(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 buffer1_size = cmd_buff[1];
    u32 buffer2_size = cmd_buff[2];
    u32 flag         = cmd_buff[3];
    u32 size1        = cmd_buff[4];
    u32 buffer1_ptr  = cmd_buff[5];
    u32 size2        = cmd_buff[6];
    u32 buffer2_ptr  = cmd_buff[7];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called buffer1_size=0x%08X, buffer2_size=0x%08X, flag=0x%08X,"
               "size1=0x%08X, buffer1_ptr=0x%08X, size2=0x%08X, buffer2_ptr=0x%08X",
               buffer1_size, buffer2_size, flag, size1, buffer1_ptr, size2, buffer2_ptr);
}

void AppletUtility(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // These are from 3dbrew - I'm not really sure what they're used for.
    u32 command = cmd_buff[1];
    u32 buffer1_size = cmd_buff[2];
    u32 buffer2_size = cmd_buff[3];
    u32 buffer1_addr = cmd_buff[5];
    u32 buffer2_addr = cmd_buff[65];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called command=0x%08X, buffer1_size=0x%08X, buffer2_size=0x%08X, "
             "buffer1_addr=0x%08X, buffer2_addr=0x%08X", command, buffer1_size, buffer2_size,
             buffer1_addr, buffer2_addr);
}

void SetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value   = cmd_buff[1];
    cpu_percent = cmd_buff[2];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called cpu_percent=%u, value=%u", cpu_percent, value);
}

void GetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value = cmd_buff[1];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value should be one, but is actually %u!", value);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = cpu_percent;

    LOG_WARNING(Service_APT, "(STUBBED) called value=%u", value);
}

void PrepareToStartLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id=%08X", applet_id);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        cmd_buff[1] = HLE::Applets::Applet::Create(applet_id).raw;
    }
    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);
}

void PreloadLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id=%08X", applet_id);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        cmd_buff[1] = HLE::Applets::Applet::Create(applet_id).raw;
    }
    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);
}

void StartLibraryApplet(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    AppletId applet_id = static_cast<AppletId>(cmd_buff[1]);
    std::shared_ptr<HLE::Applets::Applet> applet = HLE::Applets::Applet::Get(applet_id);

    LOG_DEBUG(Service_APT, "called applet_id=%08X", applet_id);

    if (applet == nullptr) {
        LOG_ERROR(Service_APT, "unknown applet id=%08X", applet_id);
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    size_t buffer_size = cmd_buff[2];
    VAddr buffer_addr = cmd_buff[6];

    AppletStartupParameter parameter;
    parameter.object = Kernel::g_handle_table.GetGeneric(cmd_buff[4]);
    parameter.buffer.resize(buffer_size);
    Memory::ReadBlock(buffer_addr, parameter.buffer.data(), parameter.buffer.size());

    cmd_buff[1] = applet->Start(parameter).raw;
}

void SetNSStateField(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    unknown_ns_state_field = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x55, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_APT, "(STUBBED) unknown_ns_state_field=%u", unknown_ns_state_field);
}

void GetNSStateField(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x56, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[8] = unknown_ns_state_field;
    LOG_WARNING(Service_APT, "(STUBBED) unknown_ns_state_field=%u", unknown_ns_state_field);
}

void GetAppletInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    auto app_id = static_cast<AppletId>(cmd_buff[1]);

    if (auto applet = HLE::Applets::Applet::Get(app_id)) {
        // TODO(Subv): Get the title id for the current applet and write it in the response[2-3]
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[4] = static_cast<u32>(Service::FS::MediaType::NAND);
        cmd_buff[5] = 1; // Registered
        cmd_buff[6] = 1; // Loaded
        cmd_buff[7] = 0; // Applet Attributes
    } else {
        cmd_buff[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                                 ErrorSummary::NotFound, ErrorLevel::Status).raw;
    }
    LOG_WARNING(Service_APT, "(stubbed) called appid=%u", app_id);
}

void GetStartupArgument(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 parameter_size = cmd_buff[1];
    StartupArgumentType startup_argument_type = static_cast<StartupArgumentType>(cmd_buff[2]);

    if (parameter_size >= 0x300) {
        LOG_ERROR(Service_APT, "Parameter size is outside the valid range (capped to 0x300): parameter_size=0x%08x", parameter_size);
        return;
    }

    LOG_WARNING(Service_APT,"(stubbed) called startup_argument_type=%u , parameter_size=0x%08x , parameter_value=0x%08x",
                startup_argument_type, parameter_size, Memory::Read32(cmd_buff[41]));

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (parameter_size > 0) ? 1 : 0;
}

void CheckNew3DSApp(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (unknown_ns_state_field) {
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = 0;
    } else {
        PTM::CheckNew3DS(self);
    }

    cmd_buff[0] = IPC::MakeHeader(0x101, 2, 0);
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void CheckNew3DS(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    PTM::CheckNew3DS(self);

    cmd_buff[0] = IPC::MakeHeader(0x102, 2, 0);
    LOG_WARNING(Service_APT, "(STUBBED) called");
}

void Init() {
    AddService(new APT_A_Interface);
    AddService(new APT_S_Interface);
    AddService(new APT_U_Interface);

    HLE::Applets::Init();

    // Load the shared system font (if available).
    // The expected format is a decrypted, uncompressed BCFNT file with the 0x80 byte header
    // generated by the APT:U service. The best way to get is by dumping it from RAM. We've provided
    // a homebrew app to do this: https://github.com/citra-emu/3dsutils. Put the resulting file
    // "shared_font.bin" in the Citra "sysdata" directory.

    std::string filepath = FileUtil::GetUserPath(D_SYSDATA_IDX) + SHARED_FONT;

    FileUtil::CreateFullPath(filepath); // Create path if not already created
    FileUtil::IOFile file(filepath, "rb");

    if (file.IsOpen()) {
        // Create shared font memory object
        using Kernel::MemoryPermission;
        shared_font_mem = Kernel::SharedMemory::Create(nullptr, 0x332000, // 3272 KB
                MemoryPermission::ReadWrite, MemoryPermission::Read, 0, Kernel::MemoryRegion::SYSTEM, "APT:SharedFont");
        // Read shared font data
        file.ReadBytes(shared_font_mem->GetPointer(), file.GetSize());
    } else {
        LOG_WARNING(Service_APT, "Unable to load shared font: %s", filepath.c_str());
        shared_font_mem = nullptr;
    }

    lock = Kernel::Mutex::Create(false, "APT_U:Lock");

    cpu_percent = 0;
    unknown_ns_state_field = 0;

    // TODO(bunnei): Check if these are created in Initialize or on APT process startup.
    notification_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "APT_U:Notification");
    parameter_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "APT_U:Start");

    next_parameter.signal = static_cast<u32>(SignalType::AppJustStarted);
    next_parameter.destination_id = 0x300;
}

void Shutdown() {
    shared_font_mem = nullptr;
    shared_font_relocated = false;
    lock = nullptr;
    notification_event = nullptr;
    parameter_event = nullptr;

    next_parameter.object = nullptr;

    HLE::Applets::Shutdown();
}

} // namespace APT
} // namespace Service
