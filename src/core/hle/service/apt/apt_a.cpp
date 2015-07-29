// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/apt/apt_a.h"

namespace Service {
namespace APT {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, GetLockHandle,                "GetLockHandle?"},
    {0x00020080, Initialize,                   "Initialize?"},
    {0x00030040, Enable,                       "Enable?"},
    {0x00040040, nullptr,                      "Finalize?"},
    {0x00050040, nullptr,                      "GetAppletManInfo?"},
    {0x00060040, nullptr,                      "GetAppletInfo?"},
    {0x00090040, IsRegistered,                 "IsRegistered"},
    {0x000C0104, SendParameter,                "SendParameter"},
    {0x000D0080, ReceiveParameter,             "ReceiveParameter"},
    {0x000E0080, GlanceParameter,              "GlanceParameter"},
    {0x000F0100, CancelParameter,              "CancelParameter"},
    {0x00160040, PreloadLibraryApplet,         "PreloadLibraryApplet"},
    {0x00180040, PrepareToStartLibraryApplet,  "PrepareToStartLibraryApplet"},
    {0x001E0084, StartLibraryApplet,           "StartLibraryApplet"},
    {0x003B0040, nullptr,                      "CancelLibraryApplet?"},
    {0x00430040, NotifyToWait,                 "NotifyToWait?"},
    {0x00440000, GetSharedFont,                "GetSharedFont?"},
    {0x004B00C2, AppletUtility,                "AppletUtility?"},
    {0x00550040, nullptr,                      "WriteInputToNsState?"},
};

APT_A_Interface::APT_A_Interface() {
    Register(FunctionTable);
}

} // namespace APT
} // namespace Service
