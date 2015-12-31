// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.


#include "core/hle/hle.h"
#include "core/hle/service/ns_s.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NS_S

namespace NS_S {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C0, nullptr,                      "LaunchFIRM"},
    {0x000200C0, nullptr,                      "LaunchTitle"},
    {0x000500C0, nullptr,                      "LaunchApplicationFIRM"},
    {0x00060042, nullptr,                      "SetFIRMParams4A0"},
    {0x00070042, nullptr,                      "CardUpdateInitialize"},
    {0x000D0140, nullptr,                      "SetFIRMParams4B0"},
    {0x000E0000, nullptr,                      "ShutdownAsync"},
    {0x00100180, nullptr,                      "RebootSystem"},
    {0x00150140, nullptr,                      "LaunchApplication"},
    {0x00160000, nullptr,                      "HardRebootSystem"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
