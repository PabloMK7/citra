// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/news/news_s.h"

namespace Service {
namespace NEWS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr,               "AddNotification"},
};

NEWS_S_Interface::NEWS_S_Interface() {
    Register(FunctionTable);
}

} // namespace NEWS
} // namespace Service
