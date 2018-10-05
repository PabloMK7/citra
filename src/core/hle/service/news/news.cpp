// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/news/news_s.h"
#include "core/hle/service/news/news_u.h"
#include "core/hle/service/service.h"

namespace Service::NEWS {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<NEWS_S>()->InstallAsService(service_manager);
    std::make_shared<NEWS_U>()->InstallAsService(service_manager);
}

} // namespace Service::NEWS
