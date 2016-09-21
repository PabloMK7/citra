// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"

namespace Service {
namespace APT {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle, "GetLockHandle?"},
    {0x00020080, Initialize, "Initialize?"},
    {0x00030040, Enable, "Enable?"},
    {0x00040040, nullptr, "Finalize?"},
    {0x00050040, GetAppletManInfo, "GetAppletManInfo"},
    {0x00060040, GetAppletInfo, "GetAppletInfo"},
    {0x00090040, IsRegistered, "IsRegistered"},
    {0x000B0040, InquireNotification, "InquireNotification"},
    {0x000C0104, SendParameter, "SendParameter"},
    {0x000D0080, ReceiveParameter, "ReceiveParameter"},
    {0x000E0080, GlanceParameter, "GlanceParameter"},
    {0x000F0100, CancelParameter, "CancelParameter"},
    {0x00150140, PrepareToStartApplication, "PrepareToStartApplication"},
    {0x00160040, PreloadLibraryApplet, "PreloadLibraryApplet"},
    {0x00180040, PrepareToStartLibraryApplet, "PrepareToStartLibraryApplet"},
    {0x001E0084, StartLibraryApplet, "StartLibraryApplet"},
    {0x003B0040, nullptr, "CancelLibraryApplet?"},
    {0x003E0080, nullptr, "ReplySleepQuery"},
    {0x00430040, NotifyToWait, "NotifyToWait?"},
    {0x00440000, GetSharedFont, "GetSharedFont?"},
    {0x004B00C2, AppletUtility, "AppletUtility?"},
    {0x004F0080, SetAppCpuTimeLimit, "SetAppCpuTimeLimit"},
    {0x00500040, GetAppCpuTimeLimit, "GetAppCpuTimeLimit"},
    {0x00510080, GetStartupArgument, "GetStartupArgument"},
    {0x00550040, SetScreenCapPostPermission, "SetScreenCapPostPermission"},
    {0x00560000, GetScreenCapPostPermission, "GetScreenCapPostPermission"},
    {0x01010000, CheckNew3DSApp, "CheckNew3DSApp"},
    {0x01020000, CheckNew3DS, "CheckNew3DS"},
};

APT_A_Interface::APT_A_Interface() {
    Register(FunctionTable);
}

} // namespace APT
} // namespace Service
