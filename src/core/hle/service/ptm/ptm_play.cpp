// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/ptm/ptm_play.h"

namespace Service {
namespace PTM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x08070082, nullptr,               "GetPlayHistory"},
    {0x08080000, nullptr,               "GetPlayHistoryStart"},
    {0x08090000, nullptr,               "GetPlayHistoryLength"},
    {0x080B0080, nullptr,               "CalcPlayHistoryStart"},
};

PTM_Play_Interface::PTM_Play_Interface() {
    Register(FunctionTable);
}

} // namespace PTM
} // namespace Service