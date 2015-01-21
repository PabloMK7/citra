// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/csnd_snd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CSND_SND

namespace CSND_SND {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010140, nullptr,               "Initialize"},
    {0x00020000, nullptr,               "Shutdown"},
    {0x00030040, nullptr,               "ExecuteType0Commands"},
    {0x00040080, nullptr,               "ExecuteType1Commands"},
    {0x00050000, nullptr,               "AcquireSoundChannels"},
    {0x00060000, nullptr,               "ReleaseSoundChannels"},
    {0x00070000, nullptr,               "AcquireCaptureDevice"},
    {0x00080040, nullptr,               "ReleaseCaptureDevice"},
    {0x00090082, nullptr,               "FlushDCache"},
    {0x000A0082, nullptr,               "StoreDCache"},
    {0x000B0082, nullptr,               "InvalidateDCache"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
