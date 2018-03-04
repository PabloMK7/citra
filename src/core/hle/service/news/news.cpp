// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/service/news/news_s.h"
#include "core/hle/service/news/news_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace NEWS {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<NEWS_S>()->InstallAsService(service_manager);
    std::make_shared<NEWS_U>()->InstallAsService(service_manager);
}

} // namespace NEWS

} // namespace Service
