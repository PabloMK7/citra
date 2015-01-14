// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/news_s.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NEWS_S

namespace NEWS_S {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr,               "AddNotification"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
