// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/news/news_u.h"

namespace Service::NEWS {

NEWS_U::NEWS_U() : ServiceFramework("news:u", 1) {
    const FunctionInfo functions[] = {
        {0x000100C8, nullptr, "AddNotification"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::NEWS
