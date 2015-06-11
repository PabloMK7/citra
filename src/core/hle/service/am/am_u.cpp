// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_u.h"

namespace Service {
namespace AM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, TitleIDListGetTotal,         "TitleIDListGetTotal"},
    {0x00020082, GetTitleIDList,              "GetTitleIDList"},
};

AM_U_Interface::AM_U_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
