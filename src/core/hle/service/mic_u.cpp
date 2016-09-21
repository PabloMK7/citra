// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/mic_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace MIC_U

namespace MIC_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, nullptr, "MapSharedMem"},
    {0x00020000, nullptr, "UnmapSharedMem"},
    {0x00030140, nullptr, "Initialize"},
    {0x00040040, nullptr, "AdjustSampling"},
    {0x00050000, nullptr, "StopSampling"},
    {0x00060000, nullptr, "IsSampling"},
    {0x00070000, nullptr, "GetEventHandle"},
    {0x00080040, nullptr, "SetGain"},
    {0x00090000, nullptr, "GetGain"},
    {0x000A0040, nullptr, "SetPower"},
    {0x000B0000, nullptr, "GetPower"},
    {0x000C0042, nullptr, "size"},
    {0x000D0040, nullptr, "SetClamp"},
    {0x000E0000, nullptr, "GetClamp"},
    {0x000F0040, nullptr, "SetAllowShellClosed"},
    {0x00100040, nullptr, "unknown_input2"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
