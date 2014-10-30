// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_U

namespace CFG_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, nullptr,               "GetConfigInfoBlk2"},
    {0x00020000, nullptr,               "SecureInfoGetRegion"},
    {0x00030000, nullptr,               "GenHashConsoleUnique"},
    {0x00040000, nullptr,               "GetRegionCanadaUSA"},
    {0x00050000, nullptr,               "GetSystemModel"},
    {0x00060000, nullptr,               "GetModelNintendo2DS"},
    {0x00070040, nullptr,               "unknown"},
    {0x00080080, nullptr,               "unknown"},
    {0x00090080, nullptr,               "GetCountryCodeString"},
    {0x000A0040, nullptr,               "GetCountryCodeID"},
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
