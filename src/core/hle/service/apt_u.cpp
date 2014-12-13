// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/common.h"
#include "common/file_util.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "apt_u.h"

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

// Handle to shared memory region designated to for shared system font
static Handle shared_font_mem = 0;

static Handle lock_handle = 0;
static std::vector<u8> shared_font;

/// Signals used by APT functions
enum class SignalType : u32 {
    None            = 0x0,
    AppJustStarted  = 0x1,
    ReturningToApp  = 0xB,
    ExitingApp      = 0xC,
};

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    cmd_buff[3] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "APT_U:Menu");  // APT menu event handle
    cmd_buff[4] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "APT_U:Pause"); // APT pause event handle

    Kernel::SetEventLocked(cmd_buff[3], true);
    Kernel::SetEventLocked(cmd_buff[4], false); // Fire start event

    _assert_msg_(KERNEL, (0 != lock_handle), "Cannot initialize without lock");
    Kernel::ReleaseMutex(lock_handle);

    cmd_buff[1] = 0; // No error

    DEBUG_LOG(KERNEL, "called");
}

void GetLockHandle(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 flags = cmd_buff[1]; // TODO(bunnei): Figure out the purpose of the flag field

    if (0 == lock_handle) {
        // TODO(bunnei): Verify if this is created here or at application boot?
        lock_handle = Kernel::CreateMutex(false, "APT_U:Lock");
        Kernel::ReleaseMutex(lock_handle);
    }
    cmd_buff[1] = 0; // No error

    // Not sure what these parameters are used for, but retail apps check that they are 0 after
    // GetLockHandle has been called.
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    cmd_buff[4] = 0;

    cmd_buff[5] = lock_handle;
    DEBUG_LOG(KERNEL, "called handle=0x%08X", cmd_buff[5]);
}

void Enable(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 unk = cmd_buff[1]; // TODO(bunnei): What is this field used for?
    cmd_buff[1] = 0; // No error
    WARN_LOG(KERNEL, "(STUBBED) called unk=0x%08X", unk);
}

void InquireNotification(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 app_id = cmd_buff[2];
    cmd_buff[1] = 0; // No error
    cmd_buff[2] = static_cast<u32>(SignalType::None); // Signal type
    WARN_LOG(KERNEL, "(STUBBED) called app_id=0x%08X", app_id);
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
 *      2 : Unknown, for now assume AppID of the process which sent these parameters
 *      3 : Unknown, for now assume Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 */
void ReceiveParameter(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];
    cmd_buff[1] = 0; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = static_cast<u32>(SignalType::AppJustStarted); // Signal type
    cmd_buff[4] = 0x10; // Parameter buffer size (16)
    cmd_buff[5] = 0;
    cmd_buff[6] = 0;
    cmd_buff[7] = 0;
    WARN_LOG(KERNEL, "(STUBBED) called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
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
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 app_id = cmd_buff[1];
    u32 buffer_size = cmd_buff[2];

    cmd_buff[1] = 0; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = static_cast<u32>(SignalType::AppJustStarted); // Signal type
    cmd_buff[4] = 0x10; // Parameter buffer size (16)
    cmd_buff[5] = 0;
    cmd_buff[6] = 0;
    cmd_buff[7] = 0;

    WARN_LOG(KERNEL, "(STUBBED) called app_id=0x%08X, buffer_size=0x%08X", app_id, buffer_size);
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
    u32* cmd_buff = Service::GetCommandBuffer();

    // These are from 3dbrew - I'm not really sure what they're used for.
    u32 unk = cmd_buff[1];
    u32 buffer1_size = cmd_buff[2];
    u32 buffer2_size = cmd_buff[3];
    u32 buffer1_addr = cmd_buff[5];
    u32 buffer2_addr = cmd_buff[65];

    cmd_buff[1] = 0; // No error

    WARN_LOG(KERNEL, "(STUBBED) called unk=0x%08X, buffer1_size=0x%08x, buffer2_size=0x%08x, "
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
    DEBUG_LOG(KERNEL, "called");

    u32* cmd_buff = Service::GetCommandBuffer();

    if (!shared_font.empty()) {
        // TODO(bunnei): This function shouldn't copy the shared font every time it's called.
        // Instead, it should probably map the shared font as RO memory. We don't currently have
        // an easy way to do this, but the copy should be sufficient for now.
        memcpy(Memory::GetPointer(SHARED_FONT_VADDR), shared_font.data(), shared_font.size());

        cmd_buff[0] = 0x00440082;
        cmd_buff[1] = 0; // No error
        cmd_buff[2] = SHARED_FONT_VADDR;
        cmd_buff[4] = shared_font_mem;
    } else {
        cmd_buff[1] = -1; // Generic error (not really possible to verify this on hardware)
        ERROR_LOG(KERNEL, "called, but %s has not been loaded!", SHARED_FONT);
    }
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle,         "GetLockHandle"},
    {0x00020080, Initialize,            "Initialize"},
    {0x00030040, Enable,                "Enable"},
    {0x00040040, nullptr,               "Finalize"},
    {0x00050040, nullptr,               "GetAppletManInfo"},
    {0x00060040, nullptr,               "GetAppletInfo"},
    {0x00070000, nullptr,               "GetLastSignaledAppletId"},
    {0x00080000, nullptr,               "CountRegisteredApplet"},
    {0x00090040, nullptr,               "IsRegistered"},
    {0x000A0040, nullptr,               "GetAttribute"},
    {0x000B0040, InquireNotification,   "InquireNotification"},
    {0x000C0104, nullptr,               "SendParameter"},
    {0x000D0080, ReceiveParameter,      "ReceiveParameter"},
    {0x000E0080, GlanceParameter,       "GlanceParameter"},
    {0x000F0100, nullptr,               "CancelParameter"},
    {0x001000C2, nullptr,               "DebugFunc"},
    {0x001100C0, nullptr,               "MapProgramIdForDebug"},
    {0x00120040, nullptr,               "SetHomeMenuAppletIdForDebug"},
    {0x00130000, nullptr,               "GetPreparationState"},
    {0x00140040, nullptr,               "SetPreparationState"},
    {0x00150140, nullptr,               "PrepareToStartApplication"},
    {0x00160040, nullptr,               "PreloadLibraryApplet"},
    {0x00170040, nullptr,               "FinishPreloadingLibraryApplet"},
    {0x00180040, nullptr,               "PrepareToStartLibraryApplet"},
    {0x00190040, nullptr,               "PrepareToStartSystemApplet"},
    {0x001A0000, nullptr,               "PrepareToStartNewestHomeMenu"},
    {0x001B00C4, nullptr,               "StartApplication"},
    {0x001C0000, nullptr,               "WakeupApplication"},
    {0x001D0000, nullptr,               "CancelApplication"},
    {0x001E0084, nullptr,               "StartLibraryApplet"},
    {0x001F0084, nullptr,               "StartSystemApplet"},
    {0x00200044, nullptr,               "StartNewestHomeMenu"},
    {0x00210000, nullptr,               "OrderToCloseApplication"},
    {0x00220040, nullptr,               "PrepareToCloseApplication"},
    {0x00230040, nullptr,               "PrepareToJumpToApplication"},
    {0x00240044, nullptr,               "JumpToApplication"},
    {0x002500C0, nullptr,               "PrepareToCloseLibraryApplet"},
    {0x00260000, nullptr,               "PrepareToCloseSystemApplet"},
    {0x00270044, nullptr,               "CloseApplication"},
    {0x00280044, nullptr,               "CloseLibraryApplet"},
    {0x00290044, nullptr,               "CloseSystemApplet"},
    {0x002A0000, nullptr,               "OrderToCloseSystemApplet"},
    {0x002B0000, nullptr,               "PrepareToJumpToHomeMenu"},
    {0x002C0044, nullptr,               "JumpToHomeMenu"},
    {0x002D0000, nullptr,               "PrepareToLeaveHomeMenu"},
    {0x002E0044, nullptr,               "LeaveHomeMenu"},
    {0x002F0040, nullptr,               "PrepareToLeaveResidentApplet"},
    {0x00300044, nullptr,               "LeaveResidentApplet"},
    {0x00310100, nullptr,               "PrepareToDoApplicationJump"},
    {0x00320084, nullptr,               "DoApplicationJump"},
    {0x00330000, nullptr,               "GetProgramIdOnApplicationJump"},
    {0x00340084, nullptr,               "SendDeliverArg"},
    {0x00350080, nullptr,               "ReceiveDeliverArg"},
    {0x00360040, nullptr,               "LoadSysMenuArg"},
    {0x00370042, nullptr,               "StoreSysMenuArg"},
    {0x00380040, nullptr,               "PreloadResidentApplet"},
    {0x00390040, nullptr,               "PrepareToStartResidentApplet"},
    {0x003A0044, nullptr,               "StartResidentApplet"},
    {0x003B0040, nullptr,               "CancelLibraryApplet"},
    {0x003C0042, nullptr,               "SendDspSleep"},
    {0x003D0042, nullptr,               "SendDspWakeUp"},
    {0x003E0080, nullptr,               "ReplySleepQuery"},
    {0x003F0040, nullptr,               "ReplySleepNotificationComplete"},
    {0x00400042, nullptr,               "SendCaptureBufferInfo"},
    {0x00410040, nullptr,               "ReceiveCaptureBufferInfo"},
    {0x00420080, nullptr,               "SleepSystem"},
    {0x00430040, nullptr,               "NotifyToWait"},
    {0x00440000, GetSharedFont,         "GetSharedFont"},
    {0x00450040, nullptr,               "GetWirelessRebootInfo"},
    {0x00460104, nullptr,               "Wrap"},
    {0x00470104, nullptr,               "Unwrap"},
    {0x00480100, nullptr,               "GetProgramInfo"},
    {0x00490180, nullptr,               "Reboot"},
    {0x004A0040, nullptr,               "GetCaptureInfo"},
    {0x004B00C2, AppletUtility,         "AppletUtility"},
    {0x004C0000, nullptr,               "SetFatalErrDispMode"},
    {0x004D0080, nullptr,               "GetAppletProgramInfo"},
    {0x004E0000, nullptr,               "HardwareResetAsync"},
    {0x004F0080, nullptr,               "SetApplicationCpuTimeLimit"},
    {0x00500040, nullptr,               "GetApplicationCpuTimeLimit"},
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
        shared_font.resize(file.GetSize());
        file.ReadBytes(shared_font.data(), file.GetSize());

        // Create shared font memory object
        shared_font_mem = Kernel::CreateSharedMemory("APT_U:shared_font_mem");
    } else {
        WARN_LOG(KERNEL, "Unable to load shared font: %s", filepath.c_str());
        shared_font_mem = 0;
    }

    lock_handle = 0;

    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
