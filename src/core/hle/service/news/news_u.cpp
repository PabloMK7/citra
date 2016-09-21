// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/news/news_u.h"

namespace Service {
namespace NEWS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr, "AddNotification"},
};

NEWS_U_Interface::NEWS_U_Interface() {
    Register(FunctionTable);
}

} // namespace NEWS
} // namespace Service
