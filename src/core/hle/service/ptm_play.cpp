// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/ptm_play.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace PTM_PLAY

namespace PTM_PLAY {

const Interface::FunctionInfo FunctionTable[] = {
    { 0x08070082, nullptr,               "GetPlayHistory" },
    { 0x08080000, nullptr,               "GetPlayHistoryStart" },
    { 0x08090000, nullptr,               "GetPlayHistoryLength" },
    { 0x080B0080, nullptr,               "CalcPlayHistoryStart" },
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}
    
} // namespace
