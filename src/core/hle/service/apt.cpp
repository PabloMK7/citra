// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/common.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/service/apt.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace APT_U

namespace APT_U {

void Initialize(Service::Interface* self) {
    NOTICE_LOG(OSHLE, "APT_U::Sync - Initialize");
}

void GetLockHandle(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 flags = cmd_buff[1]; // TODO(bunnei): Figure out the purpose of the flag field
    cmd_buff[1] = Kernel::CreateMutex(cmd_buff[5], false);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle, "GetLockHandle"},
    {0x00020080, Initialize,    "Initialize"},
    {0x00030040, NULL,          "Enable"},
    {0x00040040, NULL,          "Finalize"},
    {0x00050040, NULL,          "GetAppletManInfo"},
    {0x00060040, NULL,          "GetAppletInfo"},
    {0x00070000, NULL,          "GetLastSignaledAppletId"},
    {0x00080000, NULL,          "CountRegisteredApplet"},
    {0x00090040, NULL,          "IsRegistered"},
    {0x000A0040, NULL,          "GetAttribute"},
    {0x000B0040, NULL,          "InquireNotification"},
    {0x000C0104, NULL,          "SendParameter"},
    {0x000D0080, NULL,          "ReceiveParameter"},
    {0x000E0080, NULL,          "GlanceParameter"},
    {0x000F0100, NULL,          "CancelParameter"},
    {0x001000C2, NULL,          "DebugFunc"},
    {0x001100C0, NULL,          "MapProgramIdForDebug"},
    {0x00120040, NULL,          "SetHomeMenuAppletIdForDebug"},
    {0x00130000, NULL,          "GetPreparationState"},
    {0x00140040, NULL,          "SetPreparationState"},
    {0x00150140, NULL,          "PrepareToStartApplication"},
    {0x00160040, NULL,          "PreloadLibraryApplet"},
    {0x00170040, NULL,          "FinishPreloadingLibraryApplet"},
    {0x00180040, NULL,          "PrepareToStartLibraryApplet"},
    {0x00190040, NULL,          "PrepareToStartSystemApplet"},
    {0x001A0000, NULL,          "PrepareToStartNewestHomeMenu"},
    {0x001B00C4, NULL,          "StartApplication"},
    {0x001C0000, NULL,          "WakeupApplication"},
    {0x001D0000, NULL,          "CancelApplication"},
    {0x001E0084, NULL,          "StartLibraryApplet"},
    {0x001F0084, NULL,          "StartSystemApplet"},
    {0x00200044, NULL,          "StartNewestHomeMenu"},
    {0x00210000, NULL,          "OrderToCloseApplication"},
    {0x00220040, NULL,          "PrepareToCloseApplication"},
    {0x00230040, NULL,          "PrepareToJumpToApplication"},
    {0x00240044, NULL,          "JumpToApplication"},
    {0x002500C0, NULL,          "PrepareToCloseLibraryApplet"},
    {0x00260000, NULL,          "PrepareToCloseSystemApplet"},
    {0x00270044, NULL,          "CloseApplication"},
    {0x00280044, NULL,          "CloseLibraryApplet"},
    {0x00290044, NULL,          "CloseSystemApplet"},
    {0x002A0000, NULL,          "OrderToCloseSystemApplet"},
    {0x002B0000, NULL,          "PrepareToJumpToHomeMenu"},
    {0x002C0044, NULL,          "JumpToHomeMenu"},
    {0x002D0000, NULL,          "PrepareToLeaveHomeMenu"},
    {0x002E0044, NULL,          "LeaveHomeMenu"},
    {0x002F0040, NULL,          "PrepareToLeaveResidentApplet"},
    {0x00300044, NULL,          "LeaveResidentApplet"},
    {0x00310100, NULL,          "PrepareToDoApplicationJump"},
    {0x00320084, NULL,          "DoApplicationJump"},
    {0x00330000, NULL,          "GetProgramIdOnApplicationJump"},
    {0x00340084, NULL,          "SendDeliverArg"},
    {0x00350080, NULL,          "ReceiveDeliverArg"},
    {0x00360040, NULL,          "LoadSysMenuArg"},
    {0x00370042, NULL,          "StoreSysMenuArg"},
    {0x00380040, NULL,          "PreloadResidentApplet"},
    {0x00390040, NULL,          "PrepareToStartResidentApplet"},
    {0x003A0044, NULL,          "StartResidentApplet"},
    {0x003B0040, NULL,          "CancelLibraryApplet"},
    {0x003C0042, NULL,          "SendDspSleep"},
    {0x003D0042, NULL,          "SendDspWakeUp"},
    {0x003E0080, NULL,          "ReplySleepQuery"},
    {0x003F0040, NULL,          "ReplySleepNotificationComplete"},
    {0x00400042, NULL,          "SendCaptureBufferInfo"},
    {0x00410040, NULL,          "ReceiveCaptureBufferInfo"},
    {0x00420080, NULL,          "SleepSystem"},
    {0x00430040, NULL,          "NotifyToWait"},
    {0x00440000, NULL,          "GetSharedFont"},
    {0x00450040, NULL,          "GetWirelessRebootInfo"},
    {0x00460104, NULL,          "Wrap"},
    {0x00470104, NULL,          "Unwrap"},
    {0x00480100, NULL,          "GetProgramInfo"},
    {0x00490180, NULL,          "Reboot"},
    {0x004A0040, NULL,          "GetCaptureInfo"},
    {0x004B00C2, NULL,          "AppletUtility"},
    {0x004C0000, NULL,          "SetFatalErrDispMode"},
    {0x004D0080, NULL,          "GetAppletProgramInfo"},
    {0x004E0000, NULL,          "HardwareResetAsync"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
