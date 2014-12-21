// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/csnd_snd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CSND_SND

namespace CSND_SND {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010140, nullptr,               "Initialize"},
    {0x00020000, nullptr,               "Shutdown"},
    {0x00030040, nullptr,               "Unknown"},
    {0x00040080, nullptr,               "Unknown"},
    {0x00050000, nullptr,               "Unknown"},
    {0x00060000, nullptr,               "Unknown"},
    {0x00070000, nullptr,               "Unknown"},
    {0x00080040, nullptr,               "Unknown"},
    {0x00090082, nullptr,               "FlushDCache"},
    {0x000A0082, nullptr,               "StoreDCache"},
    {0x000B0082, nullptr,               "InvalidateDCache"},
    {0x000C0000, nullptr,               "Unknown"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
