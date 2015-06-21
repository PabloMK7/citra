// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_sys.h"

namespace Service {
namespace AM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, TitleIDListGetTotal,         "TitleIDListGetTotal"},
    {0x00020082, GetTitleIDList,              "GetTitleIDList"},
};

AM_SYS_Interface::AM_SYS_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
