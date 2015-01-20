// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/apt_a.h"

namespace APT_U {
    extern void Initialize(Service::Interface* self);
    extern void GetLockHandle(Service::Interface* self);
    extern void ReceiveParameter(Service::Interface* self);
    extern void GlanceParameter(Service::Interface* self);
    extern void GetSharedFont(Service::Interface* self);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace APT_A

namespace APT_A {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, APT_U::GetLockHandle,      "GetLockHandle?"},
    {0x00020080, APT_U::Initialize,         "Initialize?"},
    {0x00030040, nullptr,                   "Enable?"},
    {0x00040040, nullptr,                   "Finalize?"},
    {0x00050040, nullptr,                   "GetAppletManInfo?"},
    {0x00060040, nullptr,                   "GetAppletInfo?"},
    {0x000D0080, APT_U::ReceiveParameter,   "ReceiveParameter?"},
    {0x000E0080, APT_U::GlanceParameter,    "GlanceParameter?"},
    {0x003B0040, nullptr,                   "CancelLibraryApplet?"},
    {0x00430040, nullptr,                   "NotifyToWait?"},
    {0x00440000, APT_U::GetSharedFont,      "GetSharedFont?"},
    {0x004B00C2, nullptr,                   "AppletUtility?"},
    {0x00550040, nullptr,                   "WriteInputToNsState?"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
