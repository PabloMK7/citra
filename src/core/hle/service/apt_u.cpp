// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.


#include "common/common.h"
#include "common/file_util.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/apt_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace APT_U

namespace APT_U {

// Address used for shared font (as observed on HW)
// TODO(bunnei): This is the hard-coded address where we currently dump the shared font from via
// https://github.com/citra-emu/3dsutils. This is technically a hack, and will not work at any
// address other than 0x18000000 due to internal pointers in the shared font dump that would need to
// be relocated. This might be fixed by dumping the shared font @ address 0x00000000 and then
// correctly mapping it in Citra, however we still do not understand how the mapping is determined.
static const VAddr SHARED_FONT_VADDR = 0x18000000;

/// Handle to shared memory region designated to for shared system font
static Handle shared_font_mem = 0;

static Handle lock_handle = 0;
static Handle notification_event_handle = 0; ///< APT notification event handle
static Handle pause_event_handle = 0; ///< APT pause event handle
static std::vector<u8> shared_font;

/// Signals used by APT functions
enum class SignalType : u32 {
    None            = 0x0,
    AppJustStarted  = 0x1,
    ReturningToApp  = 0xB,
    ExitingApp      = 0xC,
};

/// App Id's used by APT functions
enum class AppID : u32 {
    HomeMenu           = 0x101,
    AlternateMenu      = 0x103,
    Camera             = 0x110,
    FriendsList        = 0x112,
    GameNotes          = 0x113,
    InternetBrowser    = 0x114,
    InstructionManual  = 0x115,
    Notifications      = 0x116,
    Miiverse           = 0x117,
    SoftwareKeyboard1  = 0x201,
    Ed                 = 0x202,
    PnoteApp           = 0x204,
    SnoteApp           = 0x205,
    Error              = 0x206,
    Mint               = 0x207,
    Extrapad           = 0x208,
    Memolib            = 0x209,
    Application        = 0x300,
    SoftwareKeyboard2  = 0x401,
};

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    notification_event_handle = Kernel::CreateEvent(RESETTYPE_ONESHOT, "APT_U:Notification");
    pause_event_handle = Kernel::CreateEvent(RESETTYPE_ONESHOT, "APT_U:Pause");

    cmd_buff[3] = notification_event_handle;
    cmd_buff[4] = pause_event_handle;

    Kernel::ClearEvent(notification_event_handle);
    Kernel::SignalEvent(pause_event_handle); // Fire start event

    _assert_msg_(KERNEL, (0 != lock_handle), "Cannot initialize without lock");
    Kernel::ReleaseMutex(lock_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

/**
 * APT_U::NotifyToWait service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void NotifyToWait(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    // TODO(Subv): Verify this, it seems to get SWKBD and Home Menu further.
    Kernel::SignalEvent(pause_event_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) app_id=%u", app_id);
}

void GetLockHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 flags = cmd_buff[1]; // TODO(bunnei): Figure out the purpose of the flag field

    if (0 == lock_handle) {
        // TODO(bunnei): Verify if this is created here or at application boot?
        lock_handle = Kernel::CreateMutex(false, "APT_U:Lock");
        Kernel::ReleaseMutex(lock_handle);
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    // Not sure what these parameters are used for, but retail apps check that they are 0 after
    // GetLockHandle has been called.
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    cmd_buff[4] = 0;

    cmd_buff[5] = lock_handle;
    LOG_TRACE(Service_APT, "called handle=0x%08X", cmd_buff[5]);
}

void Enable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 unk = cmd_buff[1]; // TODO(bunnei): What is this field used for?
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_APT, "(STUBBED) called unk=0x%08X", unk);
}

/**
 * APT_U::GetAppletManInfo service function.
 *  Inputs:
 *      1 : Unknown
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Unknown u32 value
 *      3 : Unknown u8 value
 *      4 : Home Menu AppId
 *      5 : AppID of currently active app
 */
void GetAppletManInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 unk = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    cmd_buff[4] = static_cast<u32>(AppID::HomeMenu); // Home menu AppID
    cmd_buff[5] = static_cast<u32>(AppID::Application); // TODO(purpasmart96): Do this correctly

    LOG_WARNING(Service_APT, "(STUBBED) called unk=0x%08X", unk);
}

/**
 * APT_U::IsRegistered service function. This returns whether the specified AppID is registered with NS yet.
 * An AppID is "registered" once the process associated with the AppID uses APT:Enable. Home Menu uses this
 * command to determine when the launched process is running and to determine when to stop using GSP etc,
 * while displaying the "Nintendo 3DS" loading screen.
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output, 0 = not registered, 1 = registered. 
 */
void IsRegistered(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 1; // Set to registered
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

void InquireNotification(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(SignalType::None); // Signal type
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X", app_id);
}

/**
 * APT_U::SendParameter service function. This sets the parameter data state. 
 * Inputs:
 *     1 : Source AppID
 *     2 : Destination AppID
 *     3 : Signal type
 *     4 : Parameter buffer size, max size is 0x1000 (this can be zero)
 *     5 : Value
 *     6 : Handle to the destination process, likely used for shared memory (this can be zero)
 *     7 : (Size<<14) | 2
 *     8 : Input parameter buffer ptr
 * Outputs:
 *     0 : Return Header
 *     1 : Result of function, 0 on success, otherwise error code
*/
void SendParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 src_app_id          = cmd_buff[1];
    u32 dst_app_id          = cmd_buff[2];
    u32 signal_type         = cmd_buff[3];
    u32 buffer_size         = cmd_buff[4];
    u32 value               = cmd_buff[5];
    u32 handle              = cmd_buff[6];
    u32 size                = cmd_buff[7];
    u32 in_param_buffer_ptr = cmd_buff[8];
    
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called src_app_id=0x%08X, dst_app_id=0x%08X, signal_type=0x%08X,"
               "buffer_size=0x%08X, value=0x%08X, handle=0x%08X, size=0x%08X, in_param_buffer_ptr=0x%08X",
               src_app_id, dst_app_id, signal_type, buffer_size, value, handle, size, in_param_buffer_ptr);
}

/**
 * APT_U::ReceiveParameter service function. This returns the current parameter data from NS state,
 * from the source process which set the parameters. Once finished, NS will clear a flag in the NS
 * state so that this command will return an error if this command is used again if parameters were
 * not set again. This is called when the second Initialize event is triggered. It returns a signal
 * type indicating why it was triggered.
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : AppID of the process which sent these parameters
 *      3 : Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 */
void ReceiveParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = static_cast<u32>(SignalType::AppJustStarted); // Signal type
    cmd_buff[4] = 0x10; // Parameter buffer size (16)
    cmd_buff[5] = 0;
    cmd_buff[6] = 0;
    cmd_buff[7] = 0;
    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

/**
 * APT_U::GlanceParameter service function. This is exactly the same as APT_U::ReceiveParameter
 * (except for the word value prior to the output handle), except this will not clear the flag
 * (except when responseword[3]==8 || responseword[3]==9) in NS state.
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Unknown, for now assume AppID of the process which sent these parameters
 *      3 : Unknown, for now assume Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 */
void GlanceParameter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = static_cast<u32>(SignalType::AppJustStarted); // Signal type
    cmd_buff[4] = 0x10; // Parameter buffer size (16)
    cmd_buff[5] = 0;
    cmd_buff[6] = 0;
    cmd_buff[7] = 0;

    LOG_WARNING(Service_APT, "(STUBBED) called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
}

/**
 * APT_U::CancelParameter service function. When the parameter data is available, and when the above
 * specified fields match the ones in NS state(for the ones where the checks are enabled), this
 * clears the flag which indicates that parameter data is available
 * (same flag cleared by APT:ReceiveParameter).
 *  Inputs:
 *      1 : Flag, when non-zero NS will compare the word after this one with a field in the NS state.
 *      2 : Unknown, this is the same as the first unknown field returned by APT:ReceiveParameter.
 *      3 : Flag, when non-zero NS will compare the word after this one with a field in the NS state.
 *      4 : AppID
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Status flag, 0 = failure due to no parameter data being available, or the above enabled
 *          fields don't match the fields in NS state. 1 = success.
 */
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

/**
 * APT_U::AppletUtility service function
 *  Inputs:
 *      1 : Unknown, but clearly used for something
 *      2 : Buffer 1 size (purpose is unknown)
 *      3 : Buffer 2 size (purpose is unknown)
 *      5 : Buffer 1 address (purpose is unknown)
 *      65 : Buffer 2 address (purpose is unknown)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void AppletUtility(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // These are from 3dbrew - I'm not really sure what they're used for.
    u32 unk = cmd_buff[1];
    u32 buffer1_size = cmd_buff[2];
    u32 buffer2_size = cmd_buff[3];
    u32 buffer1_addr = cmd_buff[5];
    u32 buffer2_addr = cmd_buff[65];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called unk=0x%08X, buffer1_size=0x%08x, buffer2_size=0x%08x, "
             "buffer1_addr=0x%08x, buffer2_addr=0x%08x", unk, buffer1_size, buffer2_size,
             buffer1_addr, buffer2_addr);
}

/**
 * APT_U::GetSharedFont service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Virtual address of where shared font will be loaded in memory
 *      4 : Handle to shared font memory
 */
void GetSharedFont(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (!shared_font.empty()) {
        // TODO(bunnei): This function shouldn't copy the shared font every time it's called.
        // Instead, it should probably map the shared font as RO memory. We don't currently have
        // an easy way to do this, but the copy should be sufficient for now.
        memcpy(Memory::GetPointer(SHARED_FONT_VADDR), shared_font.data(), shared_font.size());

        cmd_buff[0] = 0x00440082;
        cmd_buff[1] = RESULT_SUCCESS.raw; // No error
        cmd_buff[2] = SHARED_FONT_VADDR;
        cmd_buff[4] = shared_font_mem;
    } else {
        cmd_buff[1] = -1; // Generic error (not really possible to verify this on hardware)
        LOG_ERROR(Kernel_SVC, "called, but %s has not been loaded!", SHARED_FONT);
    }
}

/**
 * APT_U::SetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *      2 : Percentage of CPU time from 5 to 80
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value = cmd_buff[1];
    u32 percent = cmd_buff[2];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value must be one!", value);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_APT, "(STUBBED) called percent=0x%08X, value=0x%08x", percent, value);
}

/**
 * APT_U::GetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : System core CPU time percentage
 */
void GetAppCpuTimeLimit(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 value = cmd_buff[1];

    if (value != 1) {
        LOG_ERROR(Service_APT, "This value must be one!", value);
    }

    // TODO(purpasmart96): This is incorrect, I'm pretty sure the percentage should
    // be set by the application.

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0x80; // Set to 80%

    LOG_WARNING(Service_APT, "(STUBBED) called value=0x%08x", value);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle,                   "GetLockHandle"},
    {0x00020080, Initialize,                      "Initialize"},
    {0x00030040, Enable,                          "Enable"},
    {0x00040040, nullptr,                         "Finalize"},
    {0x00050040, GetAppletManInfo,                "GetAppletManInfo"},
    {0x00060040, nullptr,                         "GetAppletInfo"},
    {0x00070000, nullptr,                         "GetLastSignaledAppletId"},
    {0x00080000, nullptr,                         "CountRegisteredApplet"},
    {0x00090040, IsRegistered,                    "IsRegistered"},
    {0x000A0040, nullptr,                         "GetAttribute"},
    {0x000B0040, InquireNotification,             "InquireNotification"},
    {0x000C0104, SendParameter,                   "SendParameter"},
    {0x000D0080, ReceiveParameter,                "ReceiveParameter"},
    {0x000E0080, GlanceParameter,                 "GlanceParameter"},
    {0x000F0100, CancelParameter,                 "CancelParameter"},
    {0x001000C2, nullptr,                         "DebugFunc"},
    {0x001100C0, nullptr,                         "MapProgramIdForDebug"},
    {0x00120040, nullptr,                         "SetHomeMenuAppletIdForDebug"},
    {0x00130000, nullptr,                         "GetPreparationState"},
    {0x00140040, nullptr,                         "SetPreparationState"},
    {0x00150140, nullptr,                         "PrepareToStartApplication"},
    {0x00160040, nullptr,                         "PreloadLibraryApplet"},
    {0x00170040, nullptr,                         "FinishPreloadingLibraryApplet"},
    {0x00180040, nullptr,                         "PrepareToStartLibraryApplet"},
    {0x00190040, nullptr,                         "PrepareToStartSystemApplet"},
    {0x001A0000, nullptr,                         "PrepareToStartNewestHomeMenu"},
    {0x001B00C4, nullptr,                         "StartApplication"},
    {0x001C0000, nullptr,                         "WakeupApplication"},
    {0x001D0000, nullptr,                         "CancelApplication"},
    {0x001E0084, nullptr,                         "StartLibraryApplet"},
    {0x001F0084, nullptr,                         "StartSystemApplet"},
    {0x00200044, nullptr,                         "StartNewestHomeMenu"},
    {0x00210000, nullptr,                         "OrderToCloseApplication"},
    {0x00220040, nullptr,                         "PrepareToCloseApplication"},
    {0x00230040, nullptr,                         "PrepareToJumpToApplication"},
    {0x00240044, nullptr,                         "JumpToApplication"},
    {0x002500C0, nullptr,                         "PrepareToCloseLibraryApplet"},
    {0x00260000, nullptr,                         "PrepareToCloseSystemApplet"},
    {0x00270044, nullptr,                         "CloseApplication"},
    {0x00280044, nullptr,                         "CloseLibraryApplet"},
    {0x00290044, nullptr,                         "CloseSystemApplet"},
    {0x002A0000, nullptr,                         "OrderToCloseSystemApplet"},
    {0x002B0000, nullptr,                         "PrepareToJumpToHomeMenu"},
    {0x002C0044, nullptr,                         "JumpToHomeMenu"},
    {0x002D0000, nullptr,                         "PrepareToLeaveHomeMenu"},
    {0x002E0044, nullptr,                         "LeaveHomeMenu"},
    {0x002F0040, nullptr,                         "PrepareToLeaveResidentApplet"},
    {0x00300044, nullptr,                         "LeaveResidentApplet"},
    {0x00310100, nullptr,                         "PrepareToDoApplicationJump"},
    {0x00320084, nullptr,                         "DoApplicationJump"},
    {0x00330000, nullptr,                         "GetProgramIdOnApplicationJump"},
    {0x00340084, nullptr,                         "SendDeliverArg"},
    {0x00350080, nullptr,                         "ReceiveDeliverArg"},
    {0x00360040, nullptr,                         "LoadSysMenuArg"},
    {0x00370042, nullptr,                         "StoreSysMenuArg"},
    {0x00380040, nullptr,                         "PreloadResidentApplet"},
    {0x00390040, nullptr,                         "PrepareToStartResidentApplet"},
    {0x003A0044, nullptr,                         "StartResidentApplet"},
    {0x003B0040, nullptr,                         "CancelLibraryApplet"},
    {0x003C0042, nullptr,                         "SendDspSleep"},
    {0x003D0042, nullptr,                         "SendDspWakeUp"},
    {0x003E0080, nullptr,                         "ReplySleepQuery"},
    {0x003F0040, nullptr,                         "ReplySleepNotificationComplete"},
    {0x00400042, nullptr,                         "SendCaptureBufferInfo"},
    {0x00410040, nullptr,                         "ReceiveCaptureBufferInfo"},
    {0x00420080, nullptr,                         "SleepSystem"},
    {0x00430040, NotifyToWait,                    "NotifyToWait"},
    {0x00440000, GetSharedFont,                   "GetSharedFont"},
    {0x00450040, nullptr,                         "GetWirelessRebootInfo"},
    {0x00460104, nullptr,                         "Wrap"},
    {0x00470104, nullptr,                         "Unwrap"},
    {0x00480100, nullptr,                         "GetProgramInfo"},
    {0x00490180, nullptr,                         "Reboot"},
    {0x004A0040, nullptr,                         "GetCaptureInfo"},
    {0x004B00C2, AppletUtility,                   "AppletUtility"},
    {0x004C0000, nullptr,                         "SetFatalErrDispMode"},
    {0x004D0080, nullptr,                         "GetAppletProgramInfo"},
    {0x004E0000, nullptr,                         "HardwareResetAsync"},
    {0x004F0080, SetAppCpuTimeLimit,              "SetAppCpuTimeLimit"},
    {0x00500040, GetAppCpuTimeLimit,              "GetAppCpuTimeLimit"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    // Load the shared system font (if available).
    // The expected format is a decrypted, uncompressed BCFNT file with the 0x80 byte header
    // generated by the APT:U service. The best way to get is by dumping it from RAM. We've provided
    // a homebrew app to do this: https://github.com/citra-emu/3dsutils. Put the resulting file
    // "shared_font.bin" in the Citra "sysdata" directory.

    shared_font.clear();
    std::string filepath = FileUtil::GetUserPath(D_SYSDATA_IDX) + SHARED_FONT;

    FileUtil::CreateFullPath(filepath); // Create path if not already created
    FileUtil::IOFile file(filepath, "rb");

    if (file.IsOpen()) {
        // Read shared font data
        shared_font.resize((size_t)file.GetSize());
        file.ReadBytes(shared_font.data(), (size_t)file.GetSize());

        // Create shared font memory object
        shared_font_mem = Kernel::CreateSharedMemory("APT_U:shared_font_mem");
    } else {
        LOG_WARNING(Service_APT, "Unable to load shared font: %s", filepath.c_str());
        shared_font_mem = 0;
    }

    lock_handle = 0;

    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
