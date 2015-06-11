// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/service/service.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/news/news_s.h"
#include "core/hle/service/news/news_u.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"

namespace Service {
namespace NEWS {

void Init() {
    using namespace Kernel;

    AddService(new NEWS_S_Interface);
    AddService(new NEWS_U_Interface);
}

void Shutdown() {
}

} // namespace NEWS

} // namespace Service
